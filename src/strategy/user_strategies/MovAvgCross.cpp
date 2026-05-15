#include "../../../include/strategy/IStrategy.h"
#include "../../../include/strategy/StrategyRegistry.h"
#include "../../../include/utils/TimeUtils.h"
#include <spdlog/spdlog.h>
#include <deque>
#include <numeric>

namespace backtester {
    constexpr uint64_t kSampleIntervalNs = time::k2MinuteNs;
    constexpr int64_t kOnePoint = 1'000'000'000LL;

    constexpr int64_t kTakeProfitPoints = 5;
    constexpr int64_t kStopLossPoints = 7;

    struct CurrentPosition {
        int64_t quantity = 0;
        int64_t avg_entry_price = 0;
        uint64_t last_update_ts = 0;

        bool IsFlat() { return quantity == 0; }
        bool IsShort() { return quantity < 0; }
        bool IsLong() { return quantity > 0; }

        double UnrealizedPnL(double currentPrice) {
            return quantity * (currentPrice - avg_entry_price);
        }
    };

    struct PendingOrder {
        OrderSide side = OrderSide::kNone;
        int64_t price = 0;
        uint32_t size = 0;
    };

    enum class CrossType {
        kCrossBelow,
        kCrossAbove
    };

    class MovAvgCross : public IStrategy {
    public:
        MovAvgCross(const std::string& strategy_id, const IMarketDataProvider& market_data) :
            IStrategy(strategy_id, market_data) {
        }

        virtual void Initialize(const Strategy& config) override {
            traded_instr_ = config.traded_instr_id;
            fast_window_ = config.params[0];
            slow_window_ = config.params[1];

            if (fast_window_ <= 0 || slow_window_ <= 0) {
                throw std::invalid_argument(
                    "MovAvgCross window sizes must be positive");
            }
            if (fast_window_ >= slow_window_) {
                throw std::invalid_argument(
                    "MovAvgCross fast_window must be < slow_window");
            }
            spdlog::info("MovAvgCross[{}] initialized: fast={} slow={} instr={}",
                strategy_id_, fast_window_, slow_window_, traded_instr_);

        }

        virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
            const MarketByOrderEvent& event) override {

            if (event.type != EventType::kMarketTrade ||
                event.instrument_id != traded_instr_) return nullptr;
            current_price_ = event.price;

            const bool sampled = SamplePrice(event.timestamp);

            if (!pending_order_ && !cur_pos_.IsFlat()) {
                return CheckExit(event.timestamp);
            }

            if (!sampled || pending_order_) {
                return nullptr;
            }

            if (price_history_.size() < slow_window_) {
                return nullptr;  // not enough history yet
            }

            return CheckCrossoverEntry(event.timestamp);
        }

        virtual void OnFill(const StrategyFillEvent& fill) override {

            const int64_t signed_qty = (fill.side == OrderSide::kBid)
                ? static_cast<int64_t>(fill.fill_quantity)
                : -static_cast<int64_t>(fill.fill_quantity);

            const int64_t prev_qty = cur_pos_.quantity;
            const int64_t new_qty = prev_qty + signed_qty;

            if (prev_qty == 0) {
                // Opening new position.
                cur_pos_.quantity = new_qty;
                cur_pos_.avg_entry_price = fill.fill_price;
            }
            else if ((prev_qty > 0) == (signed_qty > 0)) {
                // Same direction — increase. Weight-average the entry price.
                const int64_t total_notional =
                    prev_qty * cur_pos_.avg_entry_price +
                    signed_qty * fill.fill_price;
                cur_pos_.quantity = new_qty;
                cur_pos_.avg_entry_price = total_notional / new_qty;
            }
            else {
                // Opposite direction — reduce, close, or flip.
                if (new_qty == 0) {
                    // Full close.
                    cur_pos_ = {};
                }
                else if ((prev_qty > 0) == (new_qty > 0)) {
                    // Partial close, same side remains. Entry price unchanged.
                    cur_pos_.quantity = new_qty;
                }
                else {
                    // Flip: residual is on the opposite side at the fill price.
                    cur_pos_.quantity = new_qty;
                    cur_pos_.avg_entry_price = fill.fill_price;
                }
            }
            cur_pos_.last_update_ts = fill.timestamp;
            pending_order_ = false;

            spdlog::debug("MovAvgCross[{}] fill: qty_delta={} price={} -> pos qty={} avg={}",
                strategy_id_, signed_qty, fill.fill_price,
                cur_pos_.quantity, cur_pos_.avg_entry_price);

        }

        virtual void OnEndOfDay(uint64_t timestamp) override {
            return;
        }

    private:
        CurrentPosition cur_pos_;
        bool pending_order_ = false;
        uint64_t last_sample_ts_ = 0;
        int64_t current_price_ = 0;
        std::deque<int64_t> price_history_;
        uint32_t traded_instr_;
        uint32_t slow_window_;
        uint32_t fast_window_;
        CrossType last_cross_;

        bool SamplePrice(uint64_t ts) {
            if (last_sample_ts_ != 0 &&
                ts - last_sample_ts_ < kSampleIntervalNs) {
                return false;
            }

            price_history_.push_back(current_price_);
            if (static_cast<int>(price_history_.size()) > slow_window_) {
                price_history_.pop_front();
            }
            last_sample_ts_ = ts;
            return true;
        }

        std::unique_ptr<StrategySignalEvent> CheckCrossoverEntry(uint64_t ts) {
            const int64_t fast_sma = ComputeSma(fast_window_);
            const int64_t slow_sma = ComputeSma(slow_window_);

            // Bullish cross: fast above slow, and we weren't already above.
            if (fast_sma > slow_sma && last_cross_ != CrossType::kCrossAbove) {
                last_cross_ = CrossType::kCrossAbove;

                // If short, flatten first; the next cross will go long.
                if (cur_pos_.IsShort()) {
                    spdlog::info("buy signal at price: {}", current_price_);
                    pending_order_ = true;
                    return MakeSignal(SignalType::kBuySignal, traded_instr_,
                        current_price_,
                        static_cast<uint32_t>(std::abs(cur_pos_.quantity)),
                        ts);
                }
                if (cur_pos_.IsFlat()) {
                    spdlog::info("buy signal at price: {}", current_price_);
                    pending_order_ = true;
                    return MakeSignal(SignalType::kBuySignal, traded_instr_,
                        current_price_, 1, ts);
                }
                // Already long — nothing to do.
                return nullptr;
            }

            // Bearish cross: fast below slow, and we weren't already below.
            if (fast_sma < slow_sma && last_cross_ != CrossType::kCrossBelow) {
                last_cross_ = CrossType::kCrossBelow;

                if (cur_pos_.IsLong()) {
                    spdlog::info("sell signal at price: {}", current_price_);
                    pending_order_ = true;
                    return MakeSignal(SignalType::kSellSignal, traded_instr_,
                        current_price_,
                        static_cast<uint32_t>(std::abs(cur_pos_.quantity)),
                        ts);
                }
                if (cur_pos_.IsFlat()) {
                    spdlog::info("sell signal at price: {}", current_price_);
                    pending_order_ = true;
                    return MakeSignal(SignalType::kSellSignal, traded_instr_,
                        current_price_, 1, ts);
                }
                return nullptr;
            }

            return nullptr;
        }

        int64_t ComputeSma(int window) const {
            __uint128_t sum = 0;
            auto it = price_history_.end() - window;
            for (; it != price_history_.end(); ++it) {
                sum += static_cast<__uint128_t>(*it);
            }
            return static_cast<int64_t>(sum / static_cast<__uint128_t>(window));
        }

        std::unique_ptr<StrategySignalEvent> CheckExit(uint64_t ts) {
            const int64_t tp_dist = kTakeProfitPoints * kOnePoint;
            const int64_t sl_dist = kStopLossPoints * kOnePoint;

            if (cur_pos_.IsLong()) {
                const bool hit_target =
                    current_price_ >= cur_pos_.avg_entry_price + tp_dist;
                const bool hit_stop =
                    current_price_ <= cur_pos_.avg_entry_price - sl_dist;

                if (hit_target || hit_stop) {
                    pending_order_ = true;
                    spdlog::debug("MovAvgCross[{}] long exit ({}) at {}",
                        strategy_id_,
                        hit_target ? "target" : "stop",
                        current_price_);
                    return MakeSignal(SignalType::kSellSignal, traded_instr_,
                        current_price_,
                        static_cast<uint32_t>(cur_pos_.quantity),
                        ts);
                }
            }
            else if (cur_pos_.IsShort()) {
                const bool hit_target =
                    current_price_ <= cur_pos_.avg_entry_price - tp_dist;
                const bool hit_stop =
                    current_price_ >= cur_pos_.avg_entry_price + sl_dist;

                if (hit_target || hit_stop) {
                    pending_order_ = true;
                    spdlog::debug("MovAvgCross[{}] short exit ({}) at {}",
                        strategy_id_,
                        hit_target ? "target" : "stop",
                        current_price_);
                    return MakeSignal(SignalType::kBuySignal, traded_instr_,
                        current_price_,
                        static_cast<uint32_t>(std::abs(cur_pos_.quantity)),
                        ts);
                }
            }
            return nullptr;
        }

    };

    // ==========================================================
    // Required Strategy Registration: Class Name, Strategy Name
    // ==========================================================
    REGISTER_STRATEGY(MovAvgCross, "MovAvgCrossMin");

}