#include "../../include/portfolio/PortfolioManager.h"
#include "spdlog/spdlog.h"
#include <vector>

namespace backtester {

PortfolioManager::PortfolioManager(const AppConfig& config)
    : initial_capital_(config.initial_cash), 
      current_cash_(config.initial_cash), 
      config_(config) {    
    max_equity_seen_ = initial_capital_;
}

// =============================================================================
// MARK: Risk Gate & Order Request
// =============================================================================

std::unique_ptr<StrategyOrderEvent> PortfolioManager::RequestOrder(
    const StrategySignalEvent* signal, 
    const std::unordered_map<uint32_t, BidAskPair>& current_prices) {
    
    switch (signal->signal_type) {
        case SignalType::kBuySignal:
        case SignalType::kSellSignal:
            return HandleAddRequest(signal, current_prices);

        case SignalType::kModifySignal:
            return HandleModifyRequest(signal, current_prices);

        case SignalType::kCancelSignal:
            return HandleCancelRequest(signal);

        default:
            spdlog::error("Portfolio: Unknown signal type received from Strategy {}", 
                signal->strategy_id);
            return nullptr;
    }
}

// MARK: HandleAdd
std::unique_ptr<StrategyOrderEvent> PortfolioManager::HandleAddRequest(
    const StrategySignalEvent* signal, 
    const std::unordered_map<uint32_t, BidAskPair>& latest_prices) {

    if (!IsValidTick(signal->instrument_id, signal->price)) {
        spdlog::warn("Portfolio: Rejected price {} - not a valid tick multiple.", signal->price);
        return nullptr;
    }
    
    // 2. Risk Check: Max Drawdown
    int64_t current_equity = GetTotalEquity(latest_prices);
    if (GetCurrentDrawdown(current_equity) > config_.risk_limits.max_drawdown_pct) {
        spdlog::warn("Portfolio: Order rejected. Max Drawdown {:.2f}% exceeded.", 
                     config_.risk_limits.max_drawdown_pct * 100);
        return nullptr;
    }

    const TradedInstrument* instr = GetTradedInstr(signal->instrument_id);
    if(instr == nullptr){
        spdlog::error(R"(Strategy {} is trying to trade instrument {} that is 
            not specified in traded_instruments in the config. Add it or fix 
            the strategy)", signal->strategy_id, signal->instrument_id);
        throw std::runtime_error(fmt::format(R"(Strategy {} is trying to trade 
            instrument {} that is not specified in traded_instruments in the 
            config. Add it or fix the strategy)", signal->strategy_id, 
            signal->instrument_id));
    }
    // 3. Risk Check: Buying Power (Margin)
    int64_t margin_required = CalculateMarginRequirement(signal->instrument_id,
         signal->quantity, signal->price);
    int64_t available_bp = GetBuyingPower(latest_prices ,instr->instrument_type);

    if (margin_required > available_bp) {
        spdlog::warn("Portfolio: Insufficient Buying Power. Req: {}, Avail: {}", 
                     margin_required, available_bp);
        return nullptr;
    }

    // 4. Risk Check: Position Limits
    int64_t current_qty = GetPositionQty(signal->instrument_id);
    int64_t potential_qty = current_qty + (signal->signal_type == SignalType::kBuySignal ? 
        signal->quantity : -static_cast<int64_t>(signal->quantity));
    
    if (config_.risk_limits.max_position_size > 0 && 
        std::abs(potential_qty) > config_.risk_limits.max_position_size) {
        spdlog::warn("Portfolio: Position limit exceeded. Current: {}, New Potential: {}", current_qty, potential_qty);
        return nullptr;
    }

    // 5. Construct Order Event
    OrderSide side = (signal->signal_type == SignalType::kBuySignal) ? 
        OrderSide::kBid : OrderSide::kAsk;
    ReserveMargin(signal->signal_id, signal->instrument_id, signal->quantity, signal->price);

    return std::make_unique<StrategyOrderEvent>(
        signal->timestamp,
        EventType::kStrategyOrderAdd,
        signal->signal_id,  // signal_id becomes order id for tracking
        signal->instrument_id,
        side, 
        signal->price, 
        signal->quantity,       
        signal->strategy_id
    );
}

// MARK: HandleModify
std::unique_ptr<StrategyOrderEvent> PortfolioManager::HandleModifyRequest(
    const StrategySignalEvent* signal,
    const std::unordered_map<uint32_t, BidAskPair>& latest_prices) {

    if (!IsValidTick(signal->instrument_id, signal->price)) {
        spdlog::warn("Portfolio: Modify rejected. Invalid tick price {}.", signal->price);
        return nullptr;
    }

    const TradedInstrument* instr = GetTradedInstr(signal->instrument_id);
    if (!instr) {
        spdlog::warn("Portfolio: Modify rejected. Unknown instrument {}.", 
            signal->instrument_id);
        return nullptr;
    }

    // Only pending orders can be modified
    auto it = reserved_margin_by_order_id_.find(signal->signal_id);
    if (it == reserved_margin_by_order_id_.end()) {
        spdlog::warn("Portfolio: Modify rejected. No pending order found for "
            "order_id {}.", signal->signal_id);
        return nullptr;
    }

    OrderSide side = (signal->signal_type == SignalType::kBuySignal) ?
        OrderSide::kBid : OrderSide::kAsk;
    int64_t current_qty = GetPositionQty(signal->instrument_id);
    bool is_increasing = (current_qty >= 0 && side == OrderSide::kBid) ||
                         (current_qty <= 0 && side == OrderSide::kAsk);

    int64_t new_margin = CalculateMarginRequirement(signal->instrument_id,
        signal->quantity, signal->price);
    int64_t old_margin = it->second;
    int64_t margin_delta = new_margin - old_margin;

    if (is_increasing && margin_delta > 0) {
        // Run risk checks only when exposure is increasing
        int64_t current_equity = GetTotalEquity(latest_prices);
        if (GetCurrentDrawdown(current_equity) > config_.risk_limits.max_drawdown_pct) {
            spdlog::warn("Portfolio: Modify rejected. Max drawdown exceeded.");
            return nullptr;
        }

        int64_t potential_qty = current_qty + (side == OrderSide::kBid ?
            signal->quantity : -static_cast<int64_t>(signal->quantity));
        if (config_.risk_limits.max_position_size > 0 &&
            std::abs(potential_qty) > config_.risk_limits.max_position_size) {
            spdlog::warn("Portfolio: Modify rejected. Position limit exceeded.");
            return nullptr;
        }

        int64_t available_bp = GetBuyingPower(latest_prices ,instr->instrument_type);
        if (margin_delta > available_bp) {
            spdlog::warn("Portfolio: Modify rejected. Insufficient buying power. "
                "Required: {}, Available: {}", margin_delta, available_bp);
            return nullptr;
        }
    }

    it->second = new_margin;
    reserved_margin_used_ += margin_delta;
   
    return std::make_unique<StrategyOrderEvent>(
        signal->timestamp,
        EventType::kStrategyOrderModify,
        signal->signal_id,
        signal->instrument_id,
        side,
        signal->price,
        signal->quantity,
        signal->strategy_id
    );
}

// MARK: HandleCancel
std::unique_ptr<StrategyOrderEvent> PortfolioManager::HandleCancelRequest(
    const StrategySignalEvent* signal) {

    // Only pending orders can be modified
    auto it = reserved_margin_by_order_id_.find(signal->signal_id);
    if (it == reserved_margin_by_order_id_.end()) {
        spdlog::warn("Portfolio: Cancel rejected. No pending order found for "
            "order_id {}.", signal->signal_id);
        return nullptr;
    }
    
    OrderSide side = (signal->signal_type == SignalType::kBuySignal) ? 
        OrderSide::kBid : OrderSide::kAsk;
    ReleaseMargin(signal->signal_id); 
    return std::make_unique<StrategyOrderEvent>(
        signal->timestamp,
        EventType::kStrategyOrderCancel,
        signal->signal_id,
        signal->instrument_id,
        side,
        signal->price, 
        signal->quantity,
        signal->strategy_id
    );
}

// =============================================================================
// MARK: Execution & Position Management
// =============================================================================


void PortfolioManager::ProcessFill(const StrategyFillEvent& fill) {
    ReleaseMargin(fill.order_id);

    Position& pos = positions_[fill.instrument_id];
    const TradedInstrument* instr = GetTradedInstr(fill.instrument_id);
    
    int64_t fill_qty_signed = (fill.side == OrderSide::kBid) ?
        fill.fill_quantity : -static_cast<int64_t>(fill.fill_quantity);
    
    current_cash_ -= fill.commission;

    bool is_opening = pos.quantity == 0 ||
        (pos.quantity > 0 && fill_qty_signed > 0) ||
        (pos.quantity < 0 && fill_qty_signed < 0);

    // Detect flip: fill exceeds current position in opposite direction
    // e.g. long 2, fill -5 -> close 2, open 3 short
    bool is_flip = !is_opening && 
        std::abs(fill_qty_signed) > std::abs(pos.quantity);

    if (is_opening) {
        OpenOrIncrease(pos, instr, fill, fill_qty_signed);
    }
    else if (is_flip) {
        // Step 1: Close the entire existing position
        int64_t close_qty_signed = -pos.quantity; // opposite sign to close fully
        int64_t trade_pnl = CloseOrReduce(pos, instr, fill, close_qty_signed);
        total_realized_pnl_ += trade_pnl;

        // Step 2: Open remaining quantity in opposite direction
        // e.g. long 2, fill -5: close 2, then open 3 short
        int64_t remaining_qty_signed = fill_qty_signed + close_qty_signed;
        // pos.quantity is now 0 after close, safe to open
        OpenOrIncrease(pos, instr, fill, remaining_qty_signed);
    }
    else {
        // Pure close/reduce
        int64_t trade_pnl = CloseOrReduce(pos, instr, fill, fill_qty_signed);
        total_realized_pnl_ += trade_pnl;
    }

    pos.last_update_ts = fill.timestamp;

    TradeRecord record;
    record.timestamp = fill.timestamp;
    record.instrument_id = fill.instrument_id;
    record.side = fill.side;
    record.price = fill.fill_price;
    record.quantity = fill.fill_quantity;
    record.strategy_id = fill.strategy_id;
    record.realized_pnl = total_realized_pnl_; // or pass trade_pnl from above
    trade_history_.push_back(record);
}

void PortfolioManager::OpenOrIncrease(Position& pos, const TradedInstrument* instr,
    const StrategyFillEvent& fill, int64_t fill_qty_signed) {

    if (instr->instrument_type == InstrumentType::STOCK) {
        current_cash_ -= std::abs(fill_qty_signed) * fill.fill_price;
    }
    if (instr->instrument_type == InstrumentType::FUT) {
        maintenance_margin_used_ += instr->main_margin_req * std::abs(fill_qty_signed);
    }

    int64_t current_notional = std::abs(pos.quantity) * pos.avg_entry_price;
    int64_t fill_notional = std::abs(fill_qty_signed) * fill.fill_price;
    pos.quantity += fill_qty_signed;
    pos.avg_entry_price = (current_notional + fill_notional) / std::abs(pos.quantity);
    pos.instrument_id = fill.instrument_id;
}

int64_t PortfolioManager::CloseOrReduce(Position& pos, const TradedInstrument* instr,
    const StrategyFillEvent& fill, int64_t fill_qty_signed) {

    int64_t quantity_closed = std::min(std::abs(pos.quantity), std::abs(fill_qty_signed));
    int64_t trade_pnl = 0;

    if (instr->instrument_type == InstrumentType::FUT) {
        int64_t price_diff = (pos.quantity > 0) ?
            (fill.fill_price - pos.avg_entry_price) :
            (pos.avg_entry_price - fill.fill_price);
        int64_t ticks_captured = price_diff / instr->tick_size;
        trade_pnl = ticks_captured * instr->tick_value * quantity_closed;
        current_cash_ += trade_pnl;
        maintenance_margin_used_ -= instr->main_margin_req * quantity_closed;
    } else {
        int64_t proceeds = quantity_closed * fill.fill_price;
        int64_t cost_basis = quantity_closed * pos.avg_entry_price;
        trade_pnl = proceeds - cost_basis;
        current_cash_ += proceeds;
    }

    pos.quantity += fill_qty_signed;
    if (pos.quantity == 0) pos.avg_entry_price = 0;

    return trade_pnl;
}
// =============================================================================
// MARK: Valuation & Metrics
// =============================================================================

int64_t PortfolioManager::GetUnrealizedPnL(const Position& pos, BidAskPair& cur_Bbo) const {
    if (pos.quantity == 0) return 0;
    const TradedInstrument* traded_instr_ptr = GetTradedInstr(pos.instrument_id);

    int64_t pnl = 0;
    
    if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
        int64_t price_diff = (pos.quantity > 0) ? (cur_Bbo.bid.price - pos.avg_entry_price)
                                                : (pos.avg_entry_price - cur_Bbo.ask.price);
        
        int64_t ticks = price_diff / traded_instr_ptr->tick_size;
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

int64_t PortfolioManager::GetTotalEquity(const std::unordered_map<uint32_t, BidAskPair>& cur_Bbos) const {
    int64_t unrealized = 0;
    
    if(!positions_.empty()){
        for (const auto& [id, pos] : positions_) {
            if (pos.quantity == 0) continue;
            
            BidAskPair bbo = {pos.avg_entry_price, 0, 0, pos.avg_entry_price, 0, 0}; // Fallback
            if (cur_Bbos.count(id)) {
                bbo = cur_Bbos.at(id);
            } 
            
            unrealized += GetUnrealizedPnL(pos, bbo);
        }
    }
    
    int64_t equity = current_cash_ + unrealized;

    if (equity > max_equity_seen_) {
        const_cast<PortfolioManager*>(this)->max_equity_seen_ = equity;
    }

    return equity;
}

int64_t PortfolioManager::GetBuyingPower(
    const std::unordered_map<uint32_t, BidAskPair>& cur_prices,
    InstrumentType instr_type) const {

    int64_t futures_unrealized = 0;
    for (const auto& [id, pos] : positions_) {
        const TradedInstrument* instr = GetTradedInstr(id);
        if (instr && instr->instrument_type == InstrumentType::FUT
            && pos.quantity != 0 && cur_prices.count(id)) {
            BidAskPair bbo = cur_prices.at(id);
            futures_unrealized += GetUnrealizedPnL(pos, bbo);
        }
    }

    int64_t base = current_cash_
        + futures_unrealized
        - maintenance_margin_used_
        - reserved_margin_used_;

    // Futures get additional credit: unrealized gains can offset margin requirements
    // allowing more futures positions to be opened. Stocks do not get this credit
    // since unrealized gains aren't spendable until realized.
    if (instr_type == InstrumentType::FUT) {
        return std::max<int64_t>(0, base);
    } else {
        return std::max<int64_t>(0, base - futures_unrealized);
    }
}

// int64_t PortfolioManager::GetFuturesBuyingPower(
//     const std::unordered_map<uint32_t, BidAskPair>& cur_prices) const {
    
//     // Futures maintenance margin can be reduced by unrealized futures gains
//     // since CME marks to market and credits gains to your cash balance
//     int64_t futures_unrealized = 0;
//     for (const auto& [id, pos] : positions_) {
//         const TradedInstrument* instr = GetTradedInstr(id);
//         if (instr && instr->instrument_type == InstrumentType::FUT
//             && pos.quantity != 0 && cur_prices.count(id)) {
//             BidAskPair bbo = cur_prices.at(id);
//             futures_unrealized += GetUnrealizedPnL(pos, bbo);
//         }
//     }

//     return std::max<int64_t>(0, current_cash_ 
//         + futures_unrealized
//         - maintenance_margin_used_      
//         - reserved_margin_used_); 
// }

// int64_t PortfolioManager::GetStockBuyingPower() const {
//     // Stock cash is already debited from current_cash_ on purchase
//     // so buying power is just remaining cash minus any futures margin obligations
//     return std::max<int64_t>(0, current_cash_ 
//         - maintenance_margin_used_ 
//         - reserved_margin_used_);
// }

void PortfolioManager::ReserveMargin(int32_t order_id, uint32_t instrument_id, 
    int64_t quantity, int64_t price) {
    int64_t margin = CalculateMarginRequirement(instrument_id, quantity, price);
    reserved_margin_by_order_id_[order_id] = margin;
    reserved_margin_used_ += margin;
}

void PortfolioManager::ReleaseMargin(int32_t order_id) {
    auto it = reserved_margin_by_order_id_.find(order_id);
    if (it == reserved_margin_by_order_id_.end()) return;
    reserved_margin_used_ -= it->second;
    reserved_margin_by_order_id_.erase(it);
}

int64_t PortfolioManager::GetDelta(uint32_t instrument_id, BidAskPair cur_Bbo) const {
    auto it = positions_.find(instrument_id);
    if (it == positions_.end()) return 0;

    if (cur_Bbo.bid.price == 0 || cur_Bbo.ask.price == 0 || 
        cur_Bbo.ask.price == kUndefPrice || cur_Bbo.bid.price == kUndefPrice) return 0;

    const TradedInstrument* traded_instr_ptr = GetTradedInstr(instrument_id);
    
    int64_t mid_price = ((cur_Bbo.ask.price - cur_Bbo.bid.price) / 2) + 
        cur_Bbo.bid.price;

    if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {   
        int64_t ticks = mid_price / traded_instr_ptr->tick_size;
        int64_t contract_value = ticks * traded_instr_ptr->tick_value;
        
        return it->second.quantity * contract_value;
    }    
    // Stock Dollar Value = Price * Qty
    return it->second.quantity * mid_price;
}

int64_t PortfolioManager::GetTotalPortfolioDelta(const std::unordered_map<uint32_t, 
                                                BidAskPair>& cur_prices) const {
    int64_t total_delta = 0;
    
    for (const auto& [id, pos] : positions_) {
        if (pos.quantity == 0) continue;

        BidAskPair bbo = {pos.avg_entry_price, 0, 0, pos.avg_entry_price, 0, 0};
        if (cur_prices.count(id)) {
            bbo = cur_prices.at(id);
        } 
        
        total_delta += GetDelta(id, bbo);
    }
    return total_delta;
}

// =============================================================================
// MARK: Helpers & Getters
// =============================================================================

int64_t PortfolioManager::GetCurrentDrawdown(int64_t current_equity) const {
    if (max_equity_seen_ == 0) return 0;
    return static_cast<int64_t>(
    (static_cast<__int128_t>(max_equity_seen_ - current_equity) * 1'000'000'000LL) 
    / max_equity_seen_);

}

const Position& PortfolioManager::GetPositionByInstrId(uint32_t instrument_id) const {
    static const Position empty_pos;
    auto it = positions_.find(instrument_id);
    return (it != positions_.end()) ? it->second : empty_pos;
}

int64_t PortfolioManager::GetPositionQty(uint32_t instrument_id) const {
    auto it = positions_.find(instrument_id);
    return (it != positions_.end()) ? it->second.quantity : 0;
}

bool PortfolioManager::HasPosition(uint32_t instrument_id) const {
    auto it = positions_.find(instrument_id);
    return (it != positions_.end() && it->second.quantity != 0);
}

int64_t PortfolioManager::CalculateMarginRequirement(uint32_t instrument_id,
     int64_t quantity, int64_t price) const {
    const TradedInstrument* traded_instr_ptr = GetTradedInstr(instrument_id);
    if(traded_instr_ptr == nullptr) {
        spdlog::error("CalcMarginReq: Unknown instrument {}", 
                instrument_id);
        return 0;
    }
    if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
        return quantity * traded_instr_ptr->init_margin_req;
    }
    // Stocks: Qty * Price
    return quantity * price;
}

bool PortfolioManager::IsValidTick(uint32_t instrument_id, int64_t price) const {   
    int64_t tick_size = GetTradedInstr(instrument_id)->tick_size;
    if (tick_size == 0) return true; // Safety
    return (price % tick_size) == 0;
}

}