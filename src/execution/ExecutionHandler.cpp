#include "../../include/execution/ExecutionHandler.h"
#include "spdlog/spdlog.h"
#include <algorithm>
namespace backtester {

    // =============================================================================
    // MARK: Constructor
    // =============================================================================

    ExecutionHandler::ExecutionHandler(EventQueue& event_queue, const AppConfig& config)
        : event_queue_(event_queue),
        latency_ns_(config.execution_latency_ms * 1'000'000ULL),
        config_(config),
        fill_model_(FillModel::QueuePosition) {
    }

    // =============================================================================
    // MARK: Strategy Order Intake
    // =============================================================================

    void ExecutionHandler::OnStrategyOrder(const StrategyOrderEvent& order,
        const BidAskPair& current_bbo, int64_t queue_depth_at_level) {

        switch (order.type) {
        case EventType::kStrategyOrderAdd:
            HandleAdd(order, current_bbo, queue_depth_at_level);
            break;
        case EventType::kStrategyOrderCancel:
            HandleCancel(order);
            break;
        case EventType::kStrategyOrderModify:
            HandleModify(order, current_bbo, queue_depth_at_level);
            break;
        default:
            spdlog::warn("Execution: Unhandled strategy order type {}",
                static_cast<int>(order.type));
            break;
        }
    }

    // =============================================================================
    // MARK: Handle Add
    // =============================================================================

    void ExecutionHandler::HandleAdd(const StrategyOrderEvent& order,
        const BidAskPair& current_bbo, int64_t queue_depth_at_level) {

        if (pending_orders_.count(order.order_id)) {
            spdlog::warn("Execution: Duplicate order_id {} rejected", order.order_id);
            return;
        }

        uint64_t submit_ts = order.timestamp;
        uint64_t live_ts = submit_ts + latency_ns_;

        // Marketable order: crosses the spread, fill immediately at resting price.
        // The fill timestamp is live_ts (order arrives at exchange after latency).
        if (IsMarketable(order.side, order.price, current_bbo)) {
            int64_t fill_price = (order.side == OrderSide::kBid)
                ? current_bbo.ask.price
                : current_bbo.bid.price;

            // Build a temporary PendingOrder just for the EmitFill call
            PendingOrder aggressive_order{
                order.order_id,
                order.strategy_id,
                order.instrument_id,
                order.side,
                order.price,
                static_cast<uint32_t>(order.quantity),
                submit_ts,
                live_ts,
                0  // No queue — crossing the spread
            };

            spdlog::info("Execution: Marketable order {} filled immediately at {} "
                "(submitted={}, live={})",
                order.order_id, fill_price, submit_ts, live_ts);

            EmitFill(aggressive_order, fill_price,
                static_cast<uint32_t>(order.quantity), live_ts);
            return;
        }

        // Passive order: joins the back of the queue at its price level.
        PendingOrder pending{
            order.order_id,
            order.strategy_id,
            order.instrument_id,
            order.side,
            order.price,
            order.quantity,
            submit_ts,
            live_ts,
            queue_depth_at_level
        };

        spdlog::debug("Execution: Order {} queued at price={} side={} qty_ahead={} "
            "live_ts={}",
            pending.order_id, pending.price, static_cast<int>(pending.side),
            pending.qty_ahead, pending.live_ts);

        pending_orders_.emplace(order.order_id, std::move(pending));
    }

    // =============================================================================
    // MARK: Handle Cancel
    // =============================================================================

    void ExecutionHandler::HandleCancel(const StrategyOrderEvent& order) {
        auto it = pending_orders_.find(order.order_id);
        if (it == pending_orders_.end()) {
            spdlog::warn("Execution: Cancel for unknown order_id {}", order.order_id);
            return;
        }

        spdlog::debug("Execution: Order {} cancelled", order.order_id);
        pending_orders_.erase(it);
    }

    // =============================================================================
    // MARK: Handle Modify
    // =============================================================================

    void ExecutionHandler::HandleModify(const StrategyOrderEvent& order,
        const BidAskPair& current_bbo, int64_t queue_depth_at_level) {

        auto it = pending_orders_.find(order.order_id);
        if (it == pending_orders_.end()) {
            spdlog::warn("Execution: Modify for unknown order_id {}", order.order_id);
            return;
        }

        PendingOrder& pending = it->second;
        int64_t old_price = pending.price;
        uint32_t old_qty = pending.remaining_qty;

        // Price change or size increase: loses queue priority (goes to back)
        bool loses_priority = (order.price != old_price) || order.quantity > old_qty;

        pending.price = order.price;
        pending.remaining_qty = order.quantity;

        if (loses_priority) {
            pending.qty_ahead = queue_depth_at_level;
            pending.live_ts = static_cast<uint64_t>(order.timestamp) + latency_ns_;

            spdlog::debug("Execution: Order {} modified (lost priority). "
                "new_price={} new_qty={} new_qty_ahead={}",
                order.order_id, pending.price, pending.remaining_qty,
                pending.qty_ahead);
        }
        else {
            // Size decrease: retains queue position
            spdlog::debug("Execution: Order {} modified (retained priority). "
                "new_qty={}", order.order_id, pending.remaining_qty);
        }
    }

    // =============================================================================
    // MARK: Market Event Processing (Fill Detection)
    // =============================================================================

    void ExecutionHandler::OnMarketEvent(const MarketByOrderEvent& mbo_event,
        const BidAskPair& current_bbo) {

        if (pending_orders_.empty()) return;

        switch (fill_model_) {
        case FillModel::QueuePosition:
            CheckFillsQueuePosition(mbo_event);
            break;
        case FillModel::TopOfBook:
            CheckFillsTopOfBook(mbo_event, current_bbo);
            break;
        }
    }

    // =============================================================================
    // MARK: Queue Position Fill Model
    // =============================================================================
    //
    // Core logic: We track how much resting size was ahead of us when we joined.
    // As market events occur at our price level:
    //   - Trades (kMarketTrade/kMarketFill): volume executed drains the queue.
    //     Decrement qty_ahead by the traded size. When qty_ahead <= 0, the
    //     remaining volume reaches us and we get filled.
    //   - Cancels (kMarketOrderCancel): an order ahead of us was pulled.
    //     Decrement qty_ahead by the cancelled size. This improves our position
    //     but does NOT fill us — only trades fill.
    //
    // We do NOT increment qty_ahead for new adds behind us (they joined after
    // us and don't affect our position). Adds at our price that occur before
    // our live_ts would technically be ahead, but since our latency model already
    // captured the queue depth at placement + latency offset, we accept this as
    // a reasonable approximation.

    void ExecutionHandler::CheckFillsQueuePosition(const MarketByOrderEvent& mbo_event) {
        std::vector<int32_t> filled_order_ids;

        for (auto& [order_id, pending] : pending_orders_) {
            if (!pending.IsLive(mbo_event.timestamp)) continue;
            if (pending.instrument_id != mbo_event.instrument_id) continue;

            // Determine if this market event is on the same side of the book
            // as our resting order
            bool same_side = (mbo_event.side == pending.side);

            // Trade through: price has moved through our level entirely.
            // For a resting bid, any trade at a price BELOW ours means our
            // entire level was consumed. For a resting ask, any trade ABOVE.
            bool traded_through = false;
            if (mbo_event.type == EventType::kMarketTrade ||
                mbo_event.type == EventType::kMarketFill) {

                if (pending.side == OrderSide::kBid && mbo_event.price < pending.price) {
                    traded_through = true;
                }
                else if (pending.side == OrderSide::kAsk && mbo_event.price > pending.price) {
                    traded_through = true;
                }
            }

            if (traded_through) {
                // Market traded through our price — guaranteed fill
                spdlog::info("Execution: Order {} filled (traded through). "
                    "mkt_price={} order_price={}",
                    order_id, mbo_event.price, pending.price);

                EmitFill(pending, pending.price, pending.remaining_qty,
                    mbo_event.timestamp);
                filled_order_ids.push_back(order_id);
                continue;
            }
            // Didn't trade through and isn't at our price - didn't fill
            if (mbo_event.price != pending.price) continue;

            // Cancel at our price level on our side: drains queue ahead
            if (mbo_event.type == EventType::kMarketOrderCancel && same_side) {
                pending.qty_ahead -= static_cast<int64_t>(mbo_event.size);
                // qty_ahead can go negative if cancels exceed our tracked depth;
                // that's fine — it means we're at the front
                continue;
            }

            // Trade at our price level: could fill us
            if (mbo_event.type == EventType::kMarketFill && same_side) {
                int64_t fill_size = static_cast<int64_t>(mbo_event.size);

                if (pending.qty_ahead > 0) {
                    int64_t drained = std::min(pending.qty_ahead, fill_size);
                    pending.qty_ahead -= drained;
                    fill_size -= drained;
                }

                if (pending.qty_ahead <= 0 && fill_size > 0) {
                    uint32_t fill_qty = std::min(
                        static_cast<uint32_t>(fill_size), pending.remaining_qty);

                    EmitFill(pending, pending.price, fill_qty, mbo_event.timestamp);

                    if (pending.remaining_qty == 0) {
                        filled_order_ids.push_back(order_id);
                    }
                }
            }
        }

        // Remove fully filled orders outside the iteration loop
        for (int32_t id : filled_order_ids) {
            pending_orders_.erase(id);
        }
    }

    std::vector<std::unique_ptr<StrategyFillEvent>> ExecutionHandler::CancelAllPendingOrders(
        uint64_t cancel_ts) {

        if (pending_orders_.empty()) return {};

        spdlog::info("ExecutionHandler: Cancelling {} pending orders.",
            pending_orders_.size());

        std::vector<std::unique_ptr<StrategyFillEvent>> cancel_fills;
        cancel_fills.reserve(pending_orders_.size());

        for (auto& [order_id, pending] : pending_orders_) {
            cancel_fills.push_back(std::make_unique<StrategyFillEvent>(
                cancel_ts,
                order_id,
                pending.instrument_id,
                pending.side,
                pending.price,
                0,  // zero fill quantity — no shares actually traded
                pending.strategy_id
            ));
        }

        pending_orders_.clear();
        return cancel_fills;
    }

    // =============================================================================
    // MARK: Top-of-Book Fill Model
    // =============================================================================
    // Simpler / more optimistic: fills when BBO reaches or crosses our price.
    // Useful as an upper-bound on strategy performance.

    void ExecutionHandler::CheckFillsTopOfBook(const MarketByOrderEvent& mbo_event,
        const BidAskPair& current_bbo) {

        std::vector<int32_t> filled_order_ids;

        for (auto& [order_id, pending] : pending_orders_) {
            if (!pending.IsLive(mbo_event.timestamp)) continue;
            if (pending.instrument_id != mbo_event.instrument_id) continue;

            bool should_fill = false;

            if (pending.side == OrderSide::kBid) {
                // Our bid fills when the market ask drops to or below our price
                should_fill = (current_bbo.ask.price > 0 &&
                    current_bbo.ask.price <= pending.price);
            }
            else if (pending.side == OrderSide::kAsk) {
                // Our ask fills when the market bid rises to or above our price
                should_fill = (current_bbo.bid.price > 0 &&
                    current_bbo.bid.price >= pending.price);
            }

            if (should_fill) {
                spdlog::info("Execution: Order {} filled (TOB model). price={}",
                    order_id, pending.price);
                EmitFill(pending, pending.price, pending.remaining_qty,
                    mbo_event.timestamp);
                filled_order_ids.push_back(order_id);
            }
        }

        for (int32_t id : filled_order_ids) {
            pending_orders_.erase(id);
        }
    }

    // =============================================================================
    // MARK: Helpers
    // =============================================================================

    bool ExecutionHandler::IsMarketable(OrderSide side, int64_t order_price,
        const BidAskPair& current_bbo) const {

        if (current_bbo.bid.price == 0 || current_bbo.ask.price == 0) return false;

        if (side == OrderSide::kBid) {
            // Buying at or above the current ask = crossing the spread
            return order_price >= current_bbo.ask.price;
        }
        else if (side == OrderSide::kAsk) {
            // Selling at or below the current bid = crossing the spread
            return order_price <= current_bbo.bid.price;
        }
        return false;
    }

    void ExecutionHandler::EmitFill(PendingOrder& order, int64_t fill_price,
        uint32_t fill_qty, uint64_t fill_ts) {

        order.remaining_qty -= fill_qty;
        auto instr = std::find_if(config_.traded_instruments.begin(),
            config_.traded_instruments.end(), [order](TradedInstrument traded_instr) {
                return traded_instr.instrument_id == order.instrument_id;
            });

        int64_t commission;
        if (instr->instrument_type == InstrumentType::FUT) {
            commission = fill_qty * config_.commission_struct.fut_per_contract;
        }
        else { // STOCK handling
            int64_t base_comm = std::max(config_.commission_struct.stock_order_min,
                fill_qty * config_.commission_struct.stock_per_share);
            int64_t clearing_total = fill_qty * config_.commission_struct.stock_clearing_fee;

            commission = base_comm + clearing_total;
        }

        auto fill = std::make_unique<StrategyFillEvent>(
            fill_ts,
            order.order_id,
            order.instrument_id,
            order.side,
            fill_price,
            fill_qty,
            order.strategy_id,
            commission
        );

        spdlog::info("Execution: FillEvent emitted — order_id={} instr={} side={} "
            "price={} qty={} ts={}",
            order.order_id, order.instrument_id, static_cast<int>(order.side),
            fill_price, fill_qty, fill_ts);

        event_queue_.PushEvent(std::move(fill));
    }

    const PendingOrder* ExecutionHandler::GetPendingOrder(int32_t order_id) const {
        auto it = pending_orders_.find(order_id);
        return (it != pending_orders_.end()) ? &it->second : nullptr;
    }

}

