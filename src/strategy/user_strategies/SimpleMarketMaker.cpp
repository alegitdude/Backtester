#include <spdlog/spdlog.h>

#include <cmath>
#include <vector>

#include "strategy/IStrategy.h"
#include "strategy/StrategyRegistry.h"
#include "utils/TimeUtils.h"

namespace backtester {

class SimpleMarketMaker : public IStrategy {
 public:
  static constexpr uint64_t kMinRequoteIntervalNs = 50'000'000; 

  SimpleMarketMaker(const std::string& strategy_id, const IMarketDataProvider& market_data)
      : IStrategy(strategy_id, market_data) {
    signals_.reserve(4);
  }

  void Initialize(const Strategy& config) override {
    traded_instr_ = config.traded_instr_id;
    tick_size_ = config.instr_tick_size;
    tick_value_ = config.instr_tick_value;

    if (config.params.size() < 4) {
      throw std::invalid_argument("SimpleMarketMaker requires at least 4 params");
    }

    half_spread_ticks_ = static_cast<int64_t>(config.params[0]);
    order_size_ = static_cast<int64_t>(config.params[1]);
    max_position_ = static_cast<int64_t>(config.params[2]);
    skew_ticks_per_unit_ = static_cast<int64_t>(config.params[3]);
    requote_threshold_ticks_ =
        (config.params.size() > 4) ? static_cast<int64_t>(config.params[4]) : 1;
    requote_ns_throttle_ = (config.params.size() > 5)
                               ? static_cast<uint64_t>(config.params[5])
                               : kMinRequoteIntervalNs;

    if (half_spread_ticks_ < 1 || order_size_ == 0 || max_position_ <= 0 || tick_size_ <= 0) {
      throw std::invalid_argument("SimpleMarketMaker: invalid parameters");
    }
    spdlog::info("SimpleMarketMaker[{}] init: half_spread={} size={} max_pos={} skew={}",
                 strategy_id_, half_spread_ticks_, order_size_, max_position_,
                 skew_ticks_per_unit_);
  }

  std::vector<StrategySignalEvent> OnMarketEvent(const MarketByOrderEvent& event) override {
    if (event.instrument_id != traded_instr_) return {};
    if (event.header.timestamp - last_requote_ts_ < requote_ns_throttle_) return {};

    signals_.clear();

    const auto* snap = market_data_.GetSnapshotByInstr(traded_instr_);
    if (!snap) return signals_;

    last_bbo_ = snap->bbo;
    if (last_bbo_.bid.price == kUndefPrice || last_bbo_.ask.price == kUndefPrice) return signals_;

    // Mid + microprice
    mid_price_ = (last_bbo_.bid.price + last_bbo_.ask.price) / 2;
    if (last_bbo_.bid.size + last_bbo_.ask.size == 0) {
      microprice_ = mid_price_;
    } else {
      const __int128_t num =
          static_cast<__int128_t>(last_bbo_.ask.price) * last_bbo_.bid.size +
          static_cast<__int128_t>(last_bbo_.bid.price) * last_bbo_.ask.size;
      microprice_ = static_cast<int64_t>(num / (last_bbo_.bid.size + last_bbo_.ask.size));
    }

    last_requote_ts_ = event.header.timestamp;

    // Reservation price 
    reservation_price_ =
        microprice_ - (inventory_ * skew_ticks_per_unit_ * tick_size_);

    int64_t desired_bid = reservation_price_ - (half_spread_ticks_ * tick_size_);
    int64_t desired_ask = reservation_price_ + (half_spread_ticks_ * tick_size_);

    // Round to tick 
    desired_bid = (desired_bid / tick_size_) * tick_size_;
    desired_ask = (desired_ask / tick_size_) * tick_size_;

    // Do not cross the market
    if (desired_bid >= last_bbo_.ask.price)
      desired_bid = last_bbo_.ask.price - tick_size_;
    if (desired_ask <= last_bbo_.bid.price)
      desired_ask = last_bbo_.bid.price + tick_size_;

    // Re-round after clamp
    desired_bid = (desired_bid / tick_size_) * tick_size_;
    desired_ask = (desired_ask / tick_size_) * tick_size_;

    if (desired_bid >= desired_ask) return signals_;  // still crossed / locked — skip

    const bool want_bid = (inventory_ < max_position_);
    const bool want_ask = (inventory_ > -max_position_);

    const int64_t thresh = requote_threshold_ticks_ * tick_size_;
    const bool bid_ok =
        bid_quote_.active && std::abs(bid_quote_.price - desired_bid) <= thresh;
    const bool ask_ok =
        ask_quote_.active && std::abs(ask_quote_.price - desired_ask) <= thresh;

    // ----- Bid side -----
    if (want_bid) {
      if (!bid_quote_.active) {
        PostBid(desired_bid, event.header.timestamp);
      } else if (!bid_ok) {
        CancelBid(event.header.timestamp);
        PostBid(desired_bid, event.header.timestamp);
      }
    } else if (bid_quote_.active) {
      CancelBid(event.header.timestamp);
    }

    // ----- Ask side -----
    if (want_ask) {
      if (!ask_quote_.active) {
        PostAsk(desired_ask, event.header.timestamp);
      } else if (!ask_ok) {
        CancelAsk(event.header.timestamp);
        PostAsk(desired_ask, event.header.timestamp);
      }
    } else if (ask_quote_.active) {
      CancelAsk(event.header.timestamp);
    }

    return signals_;
  }

  void OnFill(const StrategyFillEvent& fill) override {
    inventory_ += (fill.side == OrderSide::kBid)
                      ? static_cast<int64_t>(fill.quantity)
                      : -static_cast<int64_t>(fill.quantity);

    if (bid_quote_.active && fill.order_id == bid_quote_.signal_id) {
      bid_quote_ = {};
    }
    if (ask_quote_.active && fill.order_id == ask_quote_.signal_id) {
      ask_quote_ = {};
    }

    spdlog::info("SimpleMarketMaker[{}] fill side={} qty={} px={} -> inv={}",
                 strategy_id_, static_cast<int>(fill.side), fill.quantity, fill.price,
                 inventory_);
  }

  void OnRejection(const StrategyOrderRejectionEvent& event) override {
    if (bid_quote_.active && event.signal_id == bid_quote_.signal_id) {
      bid_quote_ = {};
    }
    if (ask_quote_.active && event.signal_id == ask_quote_.signal_id) {
      ask_quote_ = {};
    }
    spdlog::warn("SimpleMarketMaker[{}] rejection signal_id={} reason={}",
                 strategy_id_, event.signal_id, static_cast<int>(event.reason));
  }

  void OnEndOfDay(uint64_t timestamp) override {
    spdlog::info("SimpleMarketMaker[{}] EOD inventory={}", strategy_id_, inventory_);
  }

 private:
  struct LiveQuote {
    bool active = false;
    int64_t signal_id = 0;
    int64_t price = 0;
  };

  void PostBid(int64_t price, uint64_t ts) {
    auto sig = MakeSignal(SignalType::kBuySignal, traded_instr_, price, order_size_, ts);
    bid_quote_.active = true;
    bid_quote_.signal_id = sig.signal_id;
    bid_quote_.price = price;
    signals_.push_back(sig);
  }

  void PostAsk(int64_t price, uint64_t ts) {
    auto sig = MakeSignal(SignalType::kSellSignal, traded_instr_, price, order_size_, ts);
    ask_quote_.active = true;
    ask_quote_.signal_id = sig.signal_id;
    ask_quote_.price = price;
    signals_.push_back(sig);
  }

  void CancelBid(uint64_t ts) {
    if (!bid_quote_.active) return;
    StrategySignalEvent cancel{
        .header = {.timestamp = ts, .type = EventType::kStrategySignal},
        .strategy_id = strategy_idx_,
        .signal_id = bid_quote_.signal_id, 
        .instrument_id = traded_instr_,
        .signal_type = SignalType::kCancelSignal,
        .price = bid_quote_.price,
        .quantity = order_size_};
    signals_.push_back(cancel);
    bid_quote_ = {};
  }

  void CancelAsk(uint64_t ts) {
    if (!ask_quote_.active) return;
    StrategySignalEvent cancel{
        .header = {.timestamp = ts, .type = EventType::kStrategySignal},
        .strategy_id = strategy_idx_,
        .signal_id = ask_quote_.signal_id,
        .instrument_id = traded_instr_,
        .signal_type = SignalType::kCancelSignal,
        .price = ask_quote_.price,
        .quantity = order_size_};
    signals_.push_back(cancel);
    ask_quote_ = {};
  }

  std::vector<StrategySignalEvent> signals_;

  uint32_t traded_instr_ = 0;
  int64_t tick_size_ = 0;
  int64_t tick_value_ = 0;

  int64_t half_spread_ticks_ = 1;
  int64_t order_size_ = 1;
  int64_t max_position_ = 1;
  int64_t skew_ticks_per_unit_ = 0;
  int64_t requote_threshold_ticks_ = 1;
  uint64_t requote_ns_throttle_ = 0;

  BidAskPair last_bbo_;
  int64_t mid_price_ = 0;
  int64_t microprice_ = 0;
  uint64_t last_requote_ts_ = 0;

  int64_t inventory_ = 0;
  LiveQuote bid_quote_;
  LiveQuote ask_quote_;
  int64_t reservation_price_ = 0;
};

REGISTER_STRATEGY(SimpleMarketMaker, "SimpleMarketMaker");

}  // namespace backtester