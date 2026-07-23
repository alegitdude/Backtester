#include "execution/ExecutionHandler.h"
#include "spdlog/spdlog.h"
#include <algorithm>

namespace backtester {

    // =============================================================================
    // MARK: Constructor
    // =============================================================================

    ExecutionHandler::ExecutionHandler(EventQueue& event_queue, const AppConfig& config,
        const IMarketDataProvider& market_snapshots)
        : event_queue_(event_queue), config_(config),
        market_snapshots_(market_snapshots),
        latency_ns_(config.execution_latency_ms * 1'000'000ULL),
        fill_model_(FillModel::QueuePosition) {
        pending_orders_.reserve(PENDING_ORDERS_RESERVE);
        filled_idxs_.reserve(10);
    }

    // =============================================================================
    // MARK: On Strategy Order
    // =============================================================================

    void ExecutionHandler::OnStrategyOrder(const StrategyOrderEvent& order) {
        RunFillModel(order.timestamp, nullptr);

        switch (order.type) {
        case EventType::kStrategyOrderAdd:
            HandleAdd(order);
            break;
        case EventType::kStrategyOrderCancel:
            HandleCancel(order);
            break;
        case EventType::kStrategyOrderModify:
            HandleModify(order);
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

    void ExecutionHandler::HandleAdd(const StrategyOrderEvent& order) {

        if (GetPendingOrder(order.order_id)) {
            spdlog::warn("Execution: Duplicate order_id {} rejected", order.order_id);
            return;
        }

        uint64_t submit_ts = order.timestamp;
        uint64_t live_ts = submit_ts + latency_ns_;

        // Passive order: waits for submission timestamp + latency setting to be live.
        PendingOrder pending{
            order.order_id,
            order.strategy_id,
            order.instrument_id,
            order.side,
            order.price,
            order.quantity,
            submit_ts,
            live_ts,
            ZERO_QUANTITY
        };

        spdlog::debug("Execution: Order {} queued at price={} side={} qty_ahead={} "
            "live_ts={}",
            pending.order_id, pending.price, static_cast<int>(pending.side),
            pending.qty_ahead, pending.live_ts);

        pending_orders_.emplace_back(std::move(pending));
    }

    // =============================================================================
    // MARK: Handle Cancel
    // =============================================================================

    void ExecutionHandler::HandleCancel(const StrategyOrderEvent& order) {
        auto order_it = std::find_if(pending_orders_.begin(), pending_orders_.end(),
            [order](const PendingOrder& pending_order) {
                return order.order_id == pending_order.order_id;
            });
        if (order_it == pending_orders_.end()) {
            spdlog::warn("Execution: Cancel for unknown order_id {}", order.order_id);
            return;
        }

        spdlog::info("Execution: Order {} cancelled", order.order_id);
        pending_orders_.erase(order_it);
    }

    // =============================================================================
    // MARK: Handle Modify
    // =============================================================================

    void ExecutionHandler::HandleModify(const StrategyOrderEvent& order) {

        auto order_it = std::find_if(pending_orders_.begin(), pending_orders_.end(),
            [order](const PendingOrder& pending_order) {
                return order.order_id == pending_order.order_id;
            });
        if (order_it == pending_orders_.end()) {
            spdlog::error("Execution: Modify for unknown order_id {}", order.order_id);
            return;
        }

        PendingOrder& pending = *order_it;
        int64_t old_price = pending.price;
        qty_t old_qty = pending.remaining_qty;

        pending.price = order.price;
        pending.remaining_qty = order.quantity;

        // Price change or size increase: loses queue priority (goes to back)
        bool loses_priority = (order.price != old_price) || order.quantity > old_qty;

        if (loses_priority) {
            PendingOrder tmp = std::move(*order_it);   // or copy of `pending`
            tmp.live_ts = static_cast<uint64_t>(order.timestamp) + latency_ns_;
            tmp.state = OrderState::PendingLive;
            pending_orders_.erase(order_it);
            pending_orders_.emplace_back(std::move(tmp));

            spdlog::info("Execution: Order {} modified (lost priority). "
                "new_price={} new_qty={} new_qty_ahead={}",
                order.order_id, tmp.price, tmp.remaining_qty,
                tmp.qty_ahead);
        }
        else {
            // Size decrease: retains queue position
            spdlog::info("Execution: Order {} modified (retained priority). "
                "new_qty={}", order.order_id, pending.remaining_qty);
        }
    }

    void ExecutionHandler::RunFillModel(timestamp_t now, const MarketByOrderEvent* ev) {
        switch (fill_model_) {
        case FillModel::QueuePosition: CheckFillsQueuePosition(now, ev); break;
        case FillModel::TopOfBook:     CheckFillsTopOfBook(now, ev);     break;
        }
    }

    // =============================================================================
    // MARK: Market Event Processing (Fill Detection)
    // =============================================================================

    void ExecutionHandler::OnMarketEvent(const MarketByOrderEvent& mbo_event) {

        if (pending_orders_.empty()) return;
        RunFillModel(mbo_event.timestamp, &mbo_event);

        // switch (fill_model_) {
        // case FillModel::QueuePosition:
        //     CheckFillsQueuePosition(mbo_event.timestamp, mbo_event);
        //     break;
        // case FillModel::TopOfBook:
        //     CheckFillsTopOfBook(mbo_event);
        //     break;
        // }
    }

    // =============================================================================
    // MARK: Queue Position Fill Model
    // =============================================================================
    //
    // Order lifecycle: PendingLive -> Marketability -> Live
    //
    // Core logic: We track how much resting size was ahead of us when we joined.
    // After our marketability check, we track market events at our price level:
    //   - Trades (kMarketFill): volume executed drains the queue.
    //     Decrement qty_ahead by the traded size. When qty_ahead <= 0, the
    //     remaining volume reaches us and we get filled. We don't count Trade
    //     messages as they will always be accompanied by a Fill message and 
    //     we don't want to double count the transaction 
    //   - Cancels (kMarketOrderCancel): an order ahead of us was pulled.
    //     Decrement qty_ahead by the cancelled size. This improves our position
    //     but does NOT fill us — only trades fill.
    //
    // We do NOT increment qty_ahead for new adds behind us (they joined after
    // us and don't affect our position). Adds at our price that occur before
    // our live_ts would technically be ahead, but since our latency model already
    // captured the queue depth at placement + latency offset, we accept this as
    // a reasonable approximation.

    void ExecutionHandler::CheckFillsQueuePosition(timestamp_t now, const MarketByOrderEvent* mbo_event) {
        filled_idxs_.clear();

        for (size_t i = 0; i < pending_orders_.size(); i++) {
            auto& pending = pending_orders_[i];
            if (pending.state == OrderState::PendingLive) {
                if (!pending.IsLive(now)) continue;
                if (GoLive(pending, mbo_event)) {
                    filled_idxs_.push_back(i);
                }
                continue; // snapshot consumed this event
            }
            if (!mbo_event || pending.instrument_id != mbo_event->instrument_id) continue;

            bool same_side = (mbo_event->side == pending.side);

            // Trade through: price has moved through our level entirely.
            // For a resting bid, any trade at a price BELOW ours means our
            // entire level was consumed. For a resting ask, any trade ABOVE.
            bool traded_through = false;
            if (mbo_event->type == EventType::kMarketTrade ||
                mbo_event->type == EventType::kMarketFill) {

                if (pending.side == OrderSide::kBid && mbo_event->price < pending.price) {
                    traded_through = true;
                }
                else if (pending.side == OrderSide::kAsk && mbo_event->price > pending.price) {
                    traded_through = true;
                }
            }

            if (traded_through) {
                // Market traded through our price — guaranteed fill
                spdlog::info("Execution: Order {} filled (traded through). "
                    "mkt_price={} order_price={}",
                    pending.order_id, mbo_event->price, pending.price);

                EmitFill(pending, pending.price, pending.remaining_qty,
                    mbo_event->timestamp);
                filled_idxs_.push_back(i);
                continue;
            }

            // Didn't trade through and isn't at our price - didn't fill
            if (mbo_event->price != pending.price) continue;

            // Cancel at our price level on our side: drains queue ahead
            if (mbo_event->type == EventType::kMarketOrderCancel && same_side) {
                pending.qty_ahead -= static_cast<int64_t>(mbo_event->size);
                // qty_ahead can go negative if cancels exceed our tracked depth;
                // that's fine — it means we're at the front
                continue;
            }

            // Trade at our price level: could fill us
            if (mbo_event->type == EventType::kMarketFill && same_side) {
                int64_t fill_size = static_cast<int64_t>(mbo_event->size);

                if (pending.qty_ahead > 0) {
                    int64_t drained = std::min(pending.qty_ahead, fill_size);
                    pending.qty_ahead -= drained;
                    fill_size -= drained;
                }

                if (pending.qty_ahead <= 0 && fill_size > 0) {
                    qty_t fill_qty = std::min(fill_size, pending.remaining_qty);

                    EmitFill(pending, pending.price, fill_qty, mbo_event->timestamp);

                    if (pending.remaining_qty == 0) {
                        filled_idxs_.push_back(i);
                    }
                }
            }
        }

        for (auto it = filled_idxs_.rbegin(); it != filled_idxs_.rend(); ++it) {
            size_t idx = *it;
            pending_orders_.erase(pending_orders_.begin() + static_cast<int64_t>(idx));
        }
    }

    void ExecutionHandler::CancelAllPendingOrders() {
        spdlog::info("ExecutionHandler: Cancelling {} pending orders.",
            pending_orders_.size());

        pending_orders_.clear();
    }

    // =============================================================================
    // MARK: Top-of-Book Fill Model
    // =============================================================================
    // Simpler / more optimistic: fills when BBO reaches or crosses our price.
    // Useful as an upper-bound on strategy performance.

    void ExecutionHandler::CheckFillsTopOfBook(timestamp_t now, const MarketByOrderEvent* mbo_event) {
        filled_idxs_.clear();
        BidAskPair bbo;
        for (size_t i = 0; i < pending_orders_.size(); i++) {
            auto& pending = pending_orders_[i];
            if (pending.state == OrderState::PendingLive) {
                if (!pending.IsLive(now)) continue;
                if (GoLive(pending, mbo_event)) {
                    filled_idxs_.push_back(i);
                }
                continue; // snapshot consumed this event
            }
            if (!mbo_event || pending.instrument_id != mbo_event->instrument_id) continue;

            bbo = market_snapshots_.GetSnapshotByInstr(mbo_event->instrument_id)->bbo;

            bool should_fill = false;

            if (pending.side == OrderSide::kBid) {
                // Our bid fills when the market ask drops to or below our price
                should_fill = (bbo.ask.price > 0 &&
                    bbo.ask.price <= pending.price);
            }
            else if (pending.side == OrderSide::kAsk) {
                // Our ask fills when the market bid rises to or above our price
                should_fill = (bbo.bid.price > 0 &&
                    bbo.bid.price >= pending.price);
            }

            if (should_fill) {
                spdlog::info("Execution: Order {} filled (TOB model). price={}",
                    pending.order_id, pending.price);
                EmitFill(pending, pending.price, pending.remaining_qty,
                    mbo_event->timestamp);
                filled_idxs_.push_back(i);
            }
        }

        for (auto it = filled_idxs_.rbegin(); it != filled_idxs_.rend(); ++it) {
            size_t idx = *it;
            pending_orders_.erase(pending_orders_.begin() + static_cast<int64_t>(idx));
        }
    }

    // =============================================================================
    // MARK: Helpers
    // =============================================================================

    bool ExecutionHandler::GoLive(PendingOrder& pending,
        const MarketByOrderEvent* mbo_event) {
        pending.state = OrderState::Live;

        auto& instr_bbo = market_snapshots_.GetSnapshotByInstr(pending.instrument_id)->bbo;
        if (pending.side == OrderSide::kAsk) {
            if (instr_bbo.bid.price >= pending.price && instr_bbo.bid.price != kUndefPrice) {
                if (instr_bbo.bid.size >= pending.remaining_qty) {
                    EmitFill(pending, instr_bbo.bid.price, pending.remaining_qty, pending.live_ts);
                    return true;
                }
                else {
                    return WalkTheBook<ConsumeBids>(pending, instr_bbo);
                }
            }
        }
        else {
            if (instr_bbo.ask.price <= pending.price) {
                if (instr_bbo.ask.size >= pending.remaining_qty) {
                    EmitFill(pending, instr_bbo.ask.price, pending.remaining_qty, pending.live_ts);
                    return true;
                }
                else {
                    return WalkTheBook<ConsumeAsks>(pending, instr_bbo);
                }
            }
        }
        pending.qty_ahead = market_snapshots_.GetQueueDepth(
            pending.instrument_id, pending.side, pending.price);
        // Book state includes the trigger event, which postdates live_ts.
        // If it joined our level on our side, it's behind us, not ahead.
        if (mbo_event && mbo_event->instrument_id == pending.instrument_id &&
            mbo_event->side == pending.side &&
            mbo_event->price == pending.price &&
            (mbo_event->type == EventType::kMarketOrderAdd ||
                mbo_event->type == EventType::kMarketOrderModify)) {
            pending.qty_ahead -= static_cast<int64_t>(mbo_event->size);
        }
        return false;
    }

    money_t ExecutionHandler::GetCommissionsByInstr(uint32_t instrument_id, qty_t fill_qty) {
        auto instr = std::find_if(config_.traded_instruments.begin(),
            config_.traded_instruments.end(), [instrument_id](TradedInstrument traded_instr) {
                return traded_instr.instrument_id == instrument_id;
            });
        if (instr == config_.traded_instruments.end()) return kUndefPrice;

        if (instr->instrument_type == InstrumentType::FUT) {
            return fill_qty * config_.commission_struct.fut_per_contract;
        }
        else { // STOCK handling
            money_t base_comm = std::max(config_.commission_struct.stock_order_min,
                fill_qty * config_.commission_struct.stock_per_share);
            money_t clearing_total = fill_qty * config_.commission_struct.stock_clearing_fee;

            return base_comm + clearing_total;
        }
    }

    void ExecutionHandler::EmitFill(PendingOrder& order, price_t fill_price,
        qty_t fill_qty, timestamp_t fill_ts) {

        money_t commission = GetCommissionsByInstr(order.instrument_id, fill_qty);
        if (commission == kUndefPrice) {
            spdlog::error("Error emitting fill for unknown instrument with id {}"
                "submitted at {} ", order.instrument_id, order.submit_ts);
            return;
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

        order.remaining_qty -= fill_qty;

        event_queue_.PushEvent(std::move(fill));
    }

    template bool ExecutionHandler::WalkTheBook<ConsumeBids>(PendingOrder&, const BidAskPair&);
    template bool ExecutionHandler::WalkTheBook<ConsumeAsks>(PendingOrder&, const BidAskPair&);

    template <class Side>
    bool ExecutionHandler::WalkTheBook(PendingOrder& order, const BidAskPair& bbo) {
        auto instr_it = std::find_if(config_.traded_instruments.begin(),
            config_.traded_instruments.end(), [order](const TradedInstrument& traded_instr) {
                return traded_instr.instrument_id == order.instrument_id;
            });
        if (BT_UNLIKELY(instr_it == config_.traded_instruments.end())) throw std::runtime_error(fmt::format(
            "Uknown instrument id: {}, for order: {}, in WalkTheBook",
            order.instrument_id, order.order_id));
        const int64_t tick_size = instr_it->tick_size;    
    
        const int64_t anchor = Side::Best(bbo).price;
        const size_t lvl_count = std::min(
            CountPriceLevels(order.price, anchor, tick_size), MAX_AGGREGATE_DEPTH);

        std::array<PriceLevel, MAX_AGGREGATE_DEPTH> snapshot_array;
        for (size_t i = 0; i < lvl_count; i++)
            snapshot_array[i].price = anchor + Side::kStep * tick_size * static_cast<int64_t>(i);

        std::span<PriceLevel> aggregated_book(snapshot_array.data(), lvl_count);
        Side::Aggregate(market_snapshots_, order.instrument_id, aggregated_book);

        for (size_t i = 0; i < lvl_count; i++) {
            if (order.remaining_qty == 0) break;
            if (aggregated_book[i].size == 0) continue;
            qty_t take = std::min(order.remaining_qty, static_cast<qty_t>(aggregated_book[i].size));
            EmitFill(order, aggregated_book[i].price, take, order.live_ts);
        }
        if (order.remaining_qty == 0) return true;
        return false;
    }

    const PendingOrder* ExecutionHandler::GetPendingOrder(int32_t order_id) const {
        auto order_it = std::find_if(pending_orders_.begin(), pending_orders_.end(),
            [order_id](const PendingOrder& order) {
                return order.order_id == order_id;
            });
        return order_it != pending_orders_.end() ? &(*order_it) : nullptr;
    }

}

