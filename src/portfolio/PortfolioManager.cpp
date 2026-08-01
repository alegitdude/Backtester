#include "portfolio/PortfolioManager.h"
#include "spdlog/spdlog.h"

namespace backtester {

    PortfolioManager::PortfolioManager(const AppConfig& config, const IMarketDataProvider& market_snapshots)
        : config_(config),
        market_snapshots_(market_snapshots),
        initial_capital_(config.initial_cash),
        current_cash_(config.initial_cash) {
        max_equity_seen_ = initial_capital_;
    }

    // =============================================================================
    // MARK: Risk Gate & Order Request
    // =============================================================================

    EventUnion PortfolioManager::RequestOrder(
        const StrategySignalEvent& signal) {

        switch (signal.signal_type) {
        case SignalType::kBuySignal:
        case SignalType::kSellSignal:
            return HandleAddRequest(signal);

        case SignalType::kModifySignal:
            return HandleModifyRequest(signal);

        case SignalType::kCancelSignal:
            return HandleCancelRequest(signal);

        default:
            spdlog::error("Portfolio: Unknown signal type received from Strategy {}",
                signal.strategy_id);
            return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
                .header = {.timestamp = signal.header.timestamp,
                    .type = EventType::kStrategyOrderRejection},
                .strategy_id = signal.strategy_id,
                .signal_id = signal.signal_id,
                .instrument_id = signal.instrument_id,
                .signal_type = signal.signal_type,
                .price = signal.price,
                .quantity = signal.quantity,
                .reason = RejectionReason::kUnknownSignalType
            } };
        }
    }

    // MARK: HANDLE ADD
    EventUnion PortfolioManager::HandleAddRequest(
        const StrategySignalEvent& signal) {

        // 1. Is Valid Order
        const TradedInstrument* instr = GetTradedInstr(signal.instrument_id);
        if (instr == nullptr) {
            spdlog::error(R"(Strategy {} is trying to trade instrument {} that is 
            not specified in config.traded_instruments. Add it or fix the strategy)"
                , signal.strategy_id, signal.instrument_id);
            throw std::runtime_error(fmt::format(R"(Strategy {} is trying to trade 
            instrument {} that is not specified in conig.traded_instruments. Add 
            it or fix the strategy)", signal.strategy_id, signal.instrument_id));
        }

        if (!IsValidTick(*instr, signal.price)) {
            spdlog::warn("Portfolio: Rejected price {} - not a valid tick multiple.",
                signal.price);
            return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
              .header = {.timestamp = signal.header.timestamp,
                  .type = EventType::kStrategyOrderRejection},
              .strategy_id = signal.strategy_id,
              .signal_id = signal.signal_id,
              .instrument_id = signal.instrument_id,
              .signal_type = signal.signal_type,
              .price = signal.price,
              .quantity = signal.quantity,
              .reason = RejectionReason::kInvalidTick
            } };
        }

        // 2. Risk Check: Max Drawdown
        if (signal.signal_id != -1) { // not eod
            int64_t current_equity = GetTotalEquity();
            if (GetCurrentDrawdown(current_equity) > config_.risk_limits.max_drawdown_pct) {
                spdlog::warn("Portfolio: Order rejected. Max drawdown {:.2f}% exceeded.",
                    static_cast<double>(config_.risk_limits.max_drawdown_pct) / 1e7);
                return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
               .header = {.timestamp = signal.header.timestamp,
                   .type = EventType::kStrategyOrderRejection},
               .strategy_id = signal.strategy_id,
               .signal_id = signal.signal_id,
               .instrument_id = signal.instrument_id,
               .signal_type = signal.signal_type,
               .price = signal.price,
               .quantity = signal.quantity,
               .reason = RejectionReason::kDrawdownLimit
             } };
            }
        }
        // Set up Commission/Fees
        money_t per_unit_init_marg = instr->init_margin_req;
        money_t per_unit_commission;
        if (instr->instrument_type == InstrumentType::FUT) {
            per_unit_commission = config_.commission_struct.fut_per_contract;
        }
        else {
            per_unit_commission = config_.commission_struct.stock_per_share;
            per_unit_init_marg = signal.price;
        }

        // 3. Risk Check: Buying Power (Margin)
        if (signal.signal_id != -1) { // not eod
            money_t available_bp = GetBuyingPower(instr->instrument_type);
            money_t margin_required = (signal.quantity * per_unit_commission) +
                (signal.quantity * per_unit_init_marg);
            if (margin_required > available_bp) {
                spdlog::warn("Portfolio: Insufficient Buying Power. Req: {}, Avail: {}",
                    margin_required, available_bp);
                return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
               .header = {.timestamp = signal.header.timestamp,
                   .type = EventType::kStrategyOrderRejection},
               .strategy_id = signal.strategy_id,
               .signal_id = signal.signal_id,
               .instrument_id = signal.instrument_id,
               .signal_type = signal.signal_type,
               .price = signal.price,
               .quantity = signal.quantity,
               .reason = RejectionReason::kInsufficientBuyingPower
             } };
            }
        }

        // 4. Risk Check: Position Limits
        if (signal.signal_id != -1) { // not eod
            int64_t current_qty = GetPositionQty(signal.instrument_id);
            int64_t potential_qty = current_qty + (signal.signal_type == SignalType::kBuySignal ?
                signal.quantity : -(signal.quantity));

            if (config_.risk_limits.max_position_size > 0 &&
                std::abs(potential_qty) > config_.risk_limits.max_position_size) {
                spdlog::warn("Portfolio: Position limit exceeded. Current: {}, New Potential: {}",
                    current_qty, potential_qty);
                return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
              .header = {.timestamp = signal.header.timestamp,
                  .type = EventType::kStrategyOrderRejection},
              .strategy_id = signal.strategy_id,
              .signal_id = signal.signal_id,
              .instrument_id = signal.instrument_id,
              .signal_type = signal.signal_type,
              .price = signal.price,
              .quantity = signal.quantity,
              .reason = RejectionReason::kPositionLimit
            } };
            }
        }

        // Reserve Margin
        pending_orders_.emplace_back(signal.signal_id, signal.instrument_id,
            signal.quantity, per_unit_init_marg, per_unit_commission);
        reserved_margin_used_ += (signal.quantity * per_unit_commission) +
            (signal.quantity * per_unit_init_marg);


        // 5. Construct Order Event
        OrderSide side = (signal.signal_type == SignalType::kBuySignal) ?
            OrderSide::kBid : OrderSide::kAsk;

        return EventUnion{ .strat_order_ev = StrategyOrderEvent {
              .header = {.timestamp = signal.header.timestamp,
                  .type = EventType::kStrategyOrderAdd},
              .strategy_id = signal.strategy_id,
              .order_id = signal.signal_id,
              .instrument_id = signal.instrument_id,
              .side = side,
              .price = signal.price,
              .quantity = signal.quantity
            } };
    }

    // MARK: HANDLE MODIFY
    EventUnion PortfolioManager::HandleModifyRequest(
        const StrategySignalEvent& signal) {

        const TradedInstrument* instr = GetTradedInstr(signal.instrument_id);
        if (!instr) {
            spdlog::error(R"(Strategy {} is trying to trade instrument {} that is 
            not specified in config.traded_instruments. Add it or fix the strategy)"
                , signal.strategy_id, signal.instrument_id);
            throw std::runtime_error(fmt::format(R"(Strategy {} is trying to trade 
            instrument {} that is not specified in conig.traded_instruments. Add 
            it or fix the strategy)", signal.strategy_id, signal.instrument_id));
        }

        if (!IsValidTick(*instr, signal.price)) {
            spdlog::warn("Portfolio: Modify rejected. Invalid tick price {}.", signal.price);
            return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
               .header = {.timestamp = signal.header.timestamp,
                   .type = EventType::kStrategyOrderRejection},
               .strategy_id = signal.strategy_id,
               .signal_id = signal.signal_id,
               .instrument_id = signal.instrument_id,
               .signal_type = signal.signal_type,
               .price = signal.price,
               .quantity = signal.quantity,
               .reason = RejectionReason::kInvalidTick
             } };
        }

        // Only pending orders can be modified
        auto prev_order = std::find_if(pending_orders_.begin(), pending_orders_.end(),
            [&](PortfolioPendingOrder& order) {return order.order_id == signal.signal_id;});
        if (prev_order == pending_orders_.end()) {
            spdlog::warn("Portfolio: Modify rejected. No pending order found for "
                "order_id {}.", signal.signal_id);
            return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
              .header = {.timestamp = signal.header.timestamp,
                  .type = EventType::kStrategyOrderRejection},
              .strategy_id = signal.strategy_id,
              .signal_id = signal.signal_id,
              .instrument_id = signal.instrument_id,
              .signal_type = signal.signal_type,
              .price = signal.price,
              .quantity = signal.quantity,
              .reason = RejectionReason::kNoOrderExists
            } };
        }

        OrderSide side = (signal.signal_type == SignalType::kBuySignal) ?
            OrderSide::kBid : OrderSide::kAsk;

        bool is_increasing = (side == OrderSide::kBid &&
            prev_order->remaining_qty < signal.quantity) ||
            (side == OrderSide::kAsk &&
                prev_order->remaining_qty > signal.quantity);

        int64_t new_margin = std::abs((signal.quantity * prev_order->per_qty_margin) +
            (signal.quantity * prev_order->per_qty_com));
        int64_t old_margin = std::abs((prev_order->remaining_qty * prev_order->per_qty_margin) +
            (prev_order->remaining_qty * prev_order->per_qty_com));
        int64_t margin_delta = new_margin - old_margin;

        if (is_increasing && margin_delta > 0) {
            // Run risk checks only when exposure is increasing
            // Max Drawdown Check
            int64_t current_equity = GetTotalEquity();
            if (GetCurrentDrawdown(current_equity) > config_.risk_limits.max_drawdown_pct) {
                spdlog::warn("Portfolio: Modify rejected. Max drawdown exceeded.");
                return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
                    .header = {.timestamp = signal.header.timestamp,
                        .type = EventType::kStrategyOrderRejection},
                    .strategy_id = signal.strategy_id,
                    .signal_id = signal.signal_id,
                    .instrument_id = signal.instrument_id,
                    .signal_type = signal.signal_type,
                    .price = signal.price,
                    .quantity = signal.quantity,
                    .reason = RejectionReason::kDrawdownLimit
                } };
            }
            // Max Position Size Check
            int64_t potential_qty = GetPositionQty(signal.instrument_id) +
                (side == OrderSide::kBid ? signal.quantity : -signal.quantity);

            if (config_.risk_limits.max_position_size > 0 &&
                std::abs(potential_qty) > config_.risk_limits.max_position_size) {
                spdlog::warn("Portfolio: Modify rejected. Position limit exceeded.");
                return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
                    .header = {.timestamp = signal.header.timestamp,
                        .type = EventType::kStrategyOrderRejection},
                    .strategy_id = signal.strategy_id,
                    .signal_id = signal.signal_id,
                    .instrument_id = signal.instrument_id,
                    .signal_type = signal.signal_type,
                    .price = signal.price,
                    .quantity = signal.quantity,
                    .reason = RejectionReason::kPositionLimit
                } };
            }
            // Buying Power Check
            int64_t available_bp = GetBuyingPower(instr->instrument_type);
            if (new_margin > available_bp) {
                spdlog::warn("Portfolio: Modify rejected. Insufficient buying power. "
                    "Required: {}, Available: {}", margin_delta, available_bp);
                return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
                    .header = {.timestamp = signal.header.timestamp,
                        .type = EventType::kStrategyOrderRejection},
                    .strategy_id = signal.strategy_id,
                    .signal_id = signal.signal_id,
                    .instrument_id = signal.instrument_id,
                    .signal_type = signal.signal_type,
                    .price = signal.price,
                    .quantity = signal.quantity,
                    .reason = RejectionReason::kInsufficientBuyingPower
                } };
            }
        }

        // Reserve Margin
        prev_order->remaining_qty = signal.quantity;
        reserved_margin_used_ += margin_delta;

        return EventUnion{ .strat_order_ev = StrategyOrderEvent {
              .header = {.timestamp = signal.header.timestamp,
                  .type = EventType::kStrategyOrderModify},
              .strategy_id = signal.strategy_id,
              .order_id = signal.signal_id,
              .instrument_id = signal.instrument_id,
              .side = side,
              .price = signal.price,
              .quantity = signal.quantity
            } };
    }

    // MARK: HandleCancel
    EventUnion PortfolioManager::HandleCancelRequest(
        const StrategySignalEvent& signal) {

        // Only pending orders can be cancelled
        auto prev_order = std::find_if(pending_orders_.begin(), pending_orders_.end(),
            [&](PortfolioPendingOrder& order) {return order.order_id == signal.signal_id;});
        if (prev_order == pending_orders_.end()) {
            spdlog::warn("Portfolio: Cancel rejected. No pending order found for "
                "order_id {}.", signal.signal_id);
            return EventUnion{ .strat_rej_ev = StrategyOrderRejectionEvent{
                    .header = {.timestamp = signal.header.timestamp,
                        .type = EventType::kStrategyOrderRejection},
                    .strategy_id = signal.strategy_id,
                    .signal_id = signal.signal_id,
                    .instrument_id = signal.instrument_id,
                    .signal_type = signal.signal_type,
                    .price = signal.price,
                    .quantity = signal.quantity,
                    .reason = RejectionReason::kNonTradableInstr
                } };
        }

        OrderSide side = (signal.signal_type == SignalType::kBuySignal) ?
            OrderSide::kBid : OrderSide::kAsk;

        //Release Margin
        reserved_margin_used_ -= std::abs((prev_order->remaining_qty * prev_order->per_qty_margin) +
            (prev_order->remaining_qty * prev_order->per_qty_com));
        pending_orders_.erase(prev_order);

        return EventUnion{ .strat_order_ev = StrategyOrderEvent {
              .header = {.timestamp = signal.header.timestamp,
                  .type = EventType::kStrategyOrderAdd},
              .strategy_id = signal.strategy_id,
              .order_id = signal.signal_id,
              .instrument_id = signal.instrument_id,
              .side = side,
              .price = signal.price,
              .quantity = signal.quantity
            } };
    }

    void PortfolioManager::CancelAllPendingOrders() {
        pending_orders_.clear();
        reserved_margin_used_ = 0;
    }

    // =============================================================================
    // MARK: Execution & Position Management
    // =============================================================================

    // MARK: ProcesFill
    void PortfolioManager::ProcessFill(const StrategyFillEvent& fill) {
        const TradedInstrument* instr = GetTradedInstr(fill.instrument_id);

        //Release initial Margin
        auto pend_order = std::find_if(pending_orders_.begin(), pending_orders_.end(),
            [&](PortfolioPendingOrder& order) {return order.order_id == fill.order_id; });
        if (pend_order == pending_orders_.end()) throw std::runtime_error(fmt::format(R"(
            Fill processed for unknown strategy order. Fill order id: {} )",
            fill.order_id));

        int64_t init_margin_released = fill.quantity * pend_order->per_qty_com;
        if (instr->instrument_type == InstrumentType::FUT) {
            init_margin_released += fill.quantity * pend_order->per_qty_margin;
        }
        else { //STOCK price can be better than order limit
            init_margin_released += fill.quantity * fill.price;
        }
        reserved_margin_used_ -= init_margin_released;

        pend_order->remaining_qty -= fill.side == OrderSide::kBid ? fill.quantity :
            -(fill.quantity);

        if (pend_order->remaining_qty == 0) pending_orders_.erase(pend_order);

        // Create Position
        Position* prev_pos = FindPosition(fill.instrument_id, fill.strategy_id);
        if (prev_pos == nullptr) {
            // First fill for this strategy/instrument pair — create new position
            positions_.emplace_back();
            prev_pos = &positions_.back();
            prev_pos->instrument_id = fill.instrument_id;
            prev_pos->strategy_id = fill.strategy_id;
        }

        int64_t fill_qty_signed = (fill.side == OrderSide::kBid) ?
            fill.quantity : -static_cast<int64_t>(fill.quantity);

        current_cash_ -= fill.commission;

        bool is_opening = prev_pos->quantity == 0 ||
            (prev_pos->quantity > 0 && fill_qty_signed > 0) ||
            (prev_pos->quantity < 0 && fill_qty_signed < 0);

        // Detect flip: fill exceeds current position in opposite direction
        // e.g. long 2, fill -5 -> close 2, open 3 short
        bool is_flip = !is_opening &&
            std::abs(fill_qty_signed) > std::abs(prev_pos->quantity);

        int64_t trade_pnl = 0;

        if (is_opening) {
            OpenOrIncrease(*prev_pos, instr, fill, fill_qty_signed);
        }
        else if (is_flip) {
            // Step 1: Close the entire existing position
            int64_t close_qty_signed = -prev_pos->quantity; // opposite sign to close fully
            trade_pnl = CloseOrReduce(*prev_pos, instr, fill, close_qty_signed);
            total_realized_pnl_ += trade_pnl;

            // Step 2: Open remaining quantity in opposite direction
            int64_t remaining_qty_signed = fill_qty_signed - close_qty_signed;
            // pos.quantity is now 0 after close, safe to open
            OpenOrIncrease(*prev_pos, instr, fill, remaining_qty_signed);
        }
        else {
            // Pure close/reduce
            trade_pnl = CloseOrReduce(*prev_pos, instr, fill, fill_qty_signed);
            total_realized_pnl_ += trade_pnl;
        }

        TradeRecord record = {
            fill.header.timestamp,
            fill.strategy_id,
            fill.instrument_id,
            fill.side,
            fill.price,
            fill.quantity,
            trade_pnl,
            fill.commission
        };
        trade_history_.push_back(record);
    }

    void PortfolioManager::OpenOrIncrease(Position& pos, const TradedInstrument* instr,
        const StrategyFillEvent& fill, int64_t fill_qty_signed) {

        if (instr->instrument_type == InstrumentType::STOCK) {
            current_cash_ -= std::abs(fill_qty_signed) * fill.price;
        }
        if (instr->instrument_type == InstrumentType::FUT) {
            maintenance_margin_used_ += instr->maint_margin_req * std::abs(fill_qty_signed);
        }

        int64_t current_notional = std::abs(pos.quantity) * pos.avg_entry_price;
        int64_t fill_notional = std::abs(fill_qty_signed) * fill.price;
        pos.quantity += fill_qty_signed;
        pos.avg_entry_price = (current_notional + fill_notional) / std::abs(pos.quantity);
        pos.last_update_ts = fill.header.timestamp;
    }

    int64_t PortfolioManager::CloseOrReduce(Position& pos, const TradedInstrument* instr,
        const StrategyFillEvent& fill, int64_t fill_qty_signed) {

        int64_t quantity_closed = std::min(std::abs(pos.quantity), std::abs(fill_qty_signed));
        int64_t trade_pnl = 0;

        if (instr->instrument_type == InstrumentType::FUT) {
            int64_t price_diff = (pos.quantity > 0) ?
                (fill.price - pos.avg_entry_price) :
                (pos.avg_entry_price - fill.price);
            int64_t ticks_captured = price_diff / static_cast<int64_t>(instr->tick_size);
            trade_pnl = ticks_captured * instr->tick_value * quantity_closed;
            current_cash_ += trade_pnl;
            maintenance_margin_used_ -= instr->maint_margin_req * quantity_closed;
        }
        else {
            int64_t proceeds = quantity_closed * fill.price;
            int64_t cost_basis = quantity_closed * pos.avg_entry_price;
            trade_pnl = proceeds - cost_basis;
            current_cash_ += proceeds;
        }

        pos.quantity += fill_qty_signed;
        pos.last_update_ts = fill.header.timestamp;

        if (pos.quantity == 0 && fill.quantity == std::abs(fill_qty_signed)) {
            uint32_t closed_instr_id = pos.instrument_id;
            uint16_t closed_strat_id = pos.strategy_id;
            positions_.erase(
                std::remove_if(positions_.begin(), positions_.end(),
                    [&](const Position& p) {
                        return p.instrument_id == closed_instr_id &&
                            p.strategy_id == closed_strat_id;
                    }),
                positions_.end()
            );
        }

        return trade_pnl;
    }

    // =============================================================================
    // MARK: Valuation & Metrics
    // =============================================================================

    int64_t PortfolioManager::GetUnrealizedPnL(const Position& pos, const BidAskPair& cur_Bbo) const {
        if (pos.quantity == 0) return 0;
        const TradedInstrument* traded_instr_ptr = GetTradedInstr(pos.instrument_id);
        if (BT_UNLIKELY(!traded_instr_ptr)) {
            spdlog::error(R"(Tried to access position instrument {} from strategy 
                {}, but was not found in config. Postion last ts: {}, 
                Position order id: {})", pos.instrument_id, pos.strategy_id,
                pos.last_update_ts, pos.last_order_id);
            throw std::runtime_error(fmt::format(R"(Tried to access position 
                instrument {} from strategy {}, but was not found in config. 
                Postion last ts: {}, Position order id: {})", pos.instrument_id,
                pos.strategy_id, pos.last_update_ts, pos.last_order_id));
        }

        int64_t pnl = 0;

        if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
            int64_t price_diff = (pos.quantity > 0) ? (cur_Bbo.bid.price - pos.avg_entry_price)
                : (pos.avg_entry_price - cur_Bbo.ask.price);

            int64_t ticks = price_diff / static_cast<int64_t>(traded_instr_ptr->tick_size);
            pnl = ticks * traded_instr_ptr->tick_value * std::abs(pos.quantity);
        }
        else {
            // STOCK
            int64_t price_diff = (pos.quantity > 0) ? (cur_Bbo.bid.price - pos.avg_entry_price)
                : (pos.avg_entry_price - cur_Bbo.ask.price);
            pnl = price_diff * std::abs(pos.quantity);
        }

        return pnl;
    }

    int64_t PortfolioManager::GetTotalEquity() const {

        int64_t unrealized = 0;
        for (const auto& pos : positions_) {
            if (pos.quantity == 0) continue;
            auto bbo = market_snapshots_.GetSnapshotByInstr(pos.instrument_id)->bbo;

            unrealized += GetUnrealizedPnL(pos, bbo);
        }

        return current_cash_ + unrealized;
    }

    // MARK: GET BUYING POWER
    // Unrealized profit/loss of stock trades not counted - opening stock trades
    // is subtracted from cash (no margin)
    money_t PortfolioManager::GetBuyingPower(InstrumentType instr_type) const {

        int64_t futures_unrealized = 0;
        for (const auto& pos : positions_) {
            const TradedInstrument* instr = GetTradedInstr(pos.instrument_id);
            const auto& bbo = market_snapshots_.GetSnapshotByInstr(pos.instrument_id)->bbo;
            if (instr && instr->instrument_type == InstrumentType::FUT) {
                futures_unrealized += GetUnrealizedPnL(pos, bbo);
            }
        }

        int64_t base = current_cash_
            + futures_unrealized
            - maintenance_margin_used_
            - reserved_margin_used_;

        if (instr_type == InstrumentType::FUT) {
            return std::max<int64_t>(0, base);
        }
        else {
            return std::max<int64_t>(0, base - futures_unrealized);
        }
    }

    int64_t PortfolioManager::GetInstrPosDelta(uint32_t instrument_id, BidAskPair cur_Bbo) const {
        int64_t qty = GetPositionQty(instrument_id);
        if (qty == 0) return 0;

        if (cur_Bbo.bid.price == 0 || cur_Bbo.ask.price == 0 ||
            cur_Bbo.ask.price == kUndefPrice || cur_Bbo.bid.price == kUndefPrice) return 0;

        const TradedInstrument* instr_ptr = GetTradedInstr(instrument_id);
        if (instr_ptr == nullptr) {
            spdlog::error(R"(Error trying to get position for unknown instrument: 
                {})", instrument_id);
            return 0;
        }

        price_t mid_price = ((cur_Bbo.ask.price - cur_Bbo.bid.price) / 2) +
            cur_Bbo.bid.price;

        if (instr_ptr->instrument_type == InstrumentType::FUT) {
            int64_t ticks = mid_price / instr_ptr->tick_size;
            money_t contract_value = ticks * instr_ptr->tick_value;

            return qty * contract_value;
        }
        // Stock Dollar Value = Price * Qty
        return qty * mid_price;
    }

    int64_t PortfolioManager::GetTotalPortfolioDelta() const {
        int64_t total_delta = 0;

        for (const auto& pos : positions_) {
            if (pos.quantity == 0) continue;
            const auto& bbo = market_snapshots_.GetSnapshotByInstr(pos.instrument_id)->bbo;

            total_delta += GetInstrPosDelta(pos.instrument_id, bbo);
        }
        return total_delta;
    }

    // =============================================================================
    // MARK: Helpers & Getters
    // =============================================================================

    money_t PortfolioManager::GetCurrentDrawdown(money_t current_equity) const {
        if (max_equity_seen_ == 0 || current_equity >= max_equity_seen_) return 0;
        auto numerator = static_cast<__int128_t>(max_equity_seen_ - current_equity) * 1'000'000'000LL;
        return static_cast<int64_t>(numerator / max_equity_seen_);
    }

    const Position& PortfolioManager::GetPositionByInstrId(uint32_t instrument_id) const {
        static const Position empty_pos;
        auto it = std::find_if(positions_.begin(), positions_.end(),
            [instrument_id](const Position& pos) {
                return pos.instrument_id == instrument_id;
            });
        return (it != positions_.end()) ? *it : empty_pos;
    }

    int64_t PortfolioManager::GetPositionQty(uint32_t instrument_id) const {
        const Position& pos = GetPositionByInstrId(instrument_id);
        return pos.quantity;
    }

    money_t PortfolioManager::CalcPerUnitMarginReq(uint32_t instrument_id,
        price_t price) const {
        const TradedInstrument* traded_instr_ptr = GetTradedInstr(instrument_id);

        if (!traded_instr_ptr) {
            spdlog::error("CalcMarginReq: Unknown instrument {}",
                instrument_id);
            return 0;
        }
        if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
            return traded_instr_ptr->init_margin_req;
        }
        // Stocks: Price
        return price;
    }

    money_t PortfolioManager::GetCommissionsByInstr(uint32_t instrument_id, qty_t fill_qty) {
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

}