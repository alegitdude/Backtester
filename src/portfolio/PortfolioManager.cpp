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
    const std::unordered_map<uint32_t, Bbo>& current_prices) {
    
    switch (signal->signal_type) {
        case SignalType::kBuySignal:
        case SignalType::kSellSignal:
            return HandleAddRequest(signal, current_prices);

        case SignalType::kModifySignal:
            return HandleModifyRequest(signal);

        case SignalType::kCancelSignal:
            return HandleCancelRequest(signal);

        default:
            spdlog::error("Portfolio: Unknown signal type received from Strategy {}", signal->strategy_id);
            return nullptr;
    }
}

std::unique_ptr<StrategyOrderEvent> PortfolioManager::HandleAddRequest(
    const StrategySignalEvent* signal, 
    const std::unordered_map<uint32_t, Bbo>& latest_prices) {

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

    // 3. Risk Check: Buying Power (Margin)
    int64_t margin_required = CalculateMarginRequirement(signal->instrument_id,
         signal->quantity, signal->price);
    int64_t available_bp = GetBuyingPowerAvailable(latest_prices);

    if (margin_required > available_bp) {
        spdlog::warn("Portfolio: Insufficient Buying Power. Req: {}, Avail: {}", 
                     margin_required, available_bp);
        return nullptr;
    }

    // 4. Risk Check: Position Limits
    int64_t current_qty = GetPositionQty(signal->instrument_id);
    int64_t potential_qty = current_qty + (signal->signal_type == SignalType::kBuySignal ? signal->quantity : -static_cast<int64_t>(signal->quantity));
    
    // Note: max_position_size is usually a raw number (e.g., 10 contracts), not scaled.
    if (config_.risk_limits.max_position_size > 0 && std::abs(potential_qty) > config_.risk_limits.max_position_size) {
        spdlog::warn("Portfolio: Position limit exceeded. Current: {}, New Potential: {}", current_qty, potential_qty);
        return nullptr;
    }

    // 5. Construct Order Event
    OrderSide side = (signal->signal_type == SignalType::kBuySignal) ? OrderSide::kBid : OrderSide::kAsk;
    
    return std::make_unique<StrategyOrderEvent>(
        signal->timestamp,
        EventType::kStrategyOrderAdd,
        signal->signal_id,  // signal_id becomes order id for tracking
        signal->instrument_id,
        OrderType::kAdd,
        side,
        signal->price, 
        signal->quantity,       
        signal->strategy_id
    );
}

std::unique_ptr<StrategyOrderEvent> PortfolioManager::HandleModifyRequest(const StrategySignalEvent* signal) {
    if (!IsValidTick(signal->instrument_id ,signal->price)) {
         spdlog::warn("Portfolio: Modify rejected. Invalid tick price {}.", signal->price);
        return nullptr;
    }
    OrderSide side = (signal->signal_type == SignalType::kBuySignal) ? OrderSide::kBid : OrderSide::kAsk;

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

std::unique_ptr<StrategyOrderEvent> PortfolioManager::HandleCancelRequest(const StrategySignalEvent* signal) {
    OrderSide side = (signal->signal_type == SignalType::kBuySignal) ? OrderSide::kBid : OrderSide::kAsk;

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

void PortfolioManager::ProcessFill(const FillEvent& fill) {
    Position& pos = positions_[fill.instrument_id];
    const TradedInstrument* traded_instr_ptr = GetTradedInstr(fill.instrument_id);

    int64_t fill_price = fill.fill_price; 
    int64_t fill_qty_signed = (fill.side == OrderSide::kBid) ? fill.fill_quantity : -static_cast<int64_t>(fill.fill_quantity);

    // Scenario 1: Open / Increase
    if (pos.quantity == 0 || (pos.quantity > 0 && fill_qty_signed > 0) || (pos.quantity < 0 && fill_qty_signed < 0)) {
        // Weighted Average Entry Price (Integer Math)
        // (OldQty * OldPrice) + (NewQty * NewPrice)
        // Note: Use __int128 if there's a risk of overflow with very large quantities * scaled prices
        // Assuming int64_t is sufficient for (Qty * Price) for now.
        
        int64_t current_notional = std::abs(pos.quantity) * pos.avg_entry_price;
        int64_t fill_notional = std::abs(fill_qty_signed) * fill_price;
        
        pos.quantity += fill_qty_signed;
        // Integer division checks out
        pos.avg_entry_price = (current_notional + fill_notional) / std::abs(pos.quantity);
    } 
    // Scenario 2: Close / Reduce
    else {
        int64_t quantity_closed = std::min(std::abs(pos.quantity), std::abs(fill_qty_signed));
        int64_t trade_pnl = 0;
        
        if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
            // Futures PnL = ((Exit - Entry) / TickSize) * TickValue * Qty
            int64_t price_diff = (pos.quantity > 0) ? (fill_price - pos.avg_entry_price) 
                                                    : (pos.avg_entry_price - fill_price);
            
            // Integer division is safe because prices are guaranteed to be tick multiples
            int64_t ticks_captured = price_diff / traded_instr_ptr->tick_size;
            
            trade_pnl = ticks_captured * traded_instr_ptr->tick_value * quantity_closed;

        } else {
            // Stock PnL = (Exit - Entry) * Qty
            int64_t price_diff = (pos.quantity > 0) ? (fill_price - pos.avg_entry_price)
                                                    : (pos.avg_entry_price - fill_price);
            trade_pnl = price_diff * quantity_closed;
        }

        total_realized_pnl_ += trade_pnl;
        current_cash_ += trade_pnl;
        
        pos.quantity += fill_qty_signed; 

        // Scenario 3: Flip
        if (pos.quantity != 0 && ((pos.quantity > 0) != (fill_qty_signed < 0))) { 
             pos.avg_entry_price = fill_price;
        }
        if (pos.quantity == 0) pos.avg_entry_price = 0;
    }

    pos.last_update_ts = fill.timestamp;

    TradeRecord record;
    record.timestamp = fill.timestamp;
    record.instrument_id = fill.instrument_id;
    record.side = fill.side;
    record.price = fill.fill_price;
    record.quantity = fill.fill_quantity;
    record.realized_pnl = (std::abs(pos.quantity) < std::abs(fill_qty_signed)) ? 0 : 0; // Simplified PnL logging
    trade_history_.push_back(record);
}

// =============================================================================
// MARK: Valuation & Metrics
// =============================================================================

int64_t PortfolioManager::GetUnrealizedPnL(const Position& pos, Bbo cur_Bbo) const {
    if (pos.quantity == 0) return 0;
    const TradedInstrument* traded_instr_ptr = GetTradedInstr(pos.instrument_id);

    int64_t pnl = 0;
    
    if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
        int64_t price_diff = (pos.quantity > 0) ? (cur_Bbo.bid_price - pos.avg_entry_price)
                                                : (pos.avg_entry_price - cur_Bbo.ask_price);
        
        int64_t ticks = price_diff / traded_instr_ptr->tick_size;
        pnl = ticks * traded_instr_ptr->tick_value * std::abs(pos.quantity);
    } 
    else {
        // STOCK
        int64_t price_diff = (pos.quantity > 0) ? (cur_Bbo.bid_price - pos.avg_entry_price)
                                                : (pos.avg_entry_price - cur_Bbo.ask_price);
        pnl = price_diff * std::abs(pos.quantity);
    }
    
    return pnl;
}

int64_t PortfolioManager::GetTotalEquity(const std::unordered_map<uint32_t, Bbo>& cur_Bbos) const {
    int64_t unrealized = 0;
    
    if(!positions_.empty()){
        for (const auto& [id, pos] : positions_) {
            if (pos.quantity == 0) continue;
            
            Bbo bbo = {pos.avg_entry_price, pos.avg_entry_price}; // Fallback
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

int64_t PortfolioManager::GetBuyingPowerAvailable(const std::unordered_map<uint32_t, Bbo>& cur_prices) const {
    int64_t total_equity = GetTotalEquity(cur_prices);
    
    int64_t margin_used = 0;
    for (const auto& [id, pos] : positions_) {
        const TradedInstrument* traded_instr_ptr = GetTradedInstr(id);
        if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
            // Margin Req is typically a flat dollar amount (scaled integer) per contract
            margin_used += std::abs(pos.quantity) * traded_instr_ptr->margin_req;
        } else { //TODO
        // Stock logic (Cash account = 0 margin used, but cash is locked. Leveraged = different)
        // Assuming Cash Account: Buying power is purely Cash.
        // Assuming Margin Account: Equity - (PositionValue * MaintenanceRequirement)
            //margin_used = 0; 
        }
    }
    return std::max<int64_t>(0, total_equity - margin_used);
}

int64_t PortfolioManager::GetDelta(uint32_t instrument_id, Bbo cur_Bbo) const {
    auto it = positions_.find(instrument_id);
    if (it == positions_.end()) return 0;
    
    const TradedInstrument* traded_instr_ptr = GetTradedInstr(instrument_id);
    
    int64_t mid_price = ((cur_Bbo.ask_price - cur_Bbo.bid_price) / 2) + 
        cur_Bbo.bid_price;

    if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {   
        int64_t ticks = mid_price / traded_instr_ptr->tick_size;
        int64_t contract_value = ticks * traded_instr_ptr->tick_value;
        
        return it->second.quantity * contract_value;
    }    
    // Stock Dollar Value = Price * Qty
    return it->second.quantity * mid_price;
}

int64_t PortfolioManager::GetTotalPortfolioDelta(const std::unordered_map<uint32_t, Bbo>& cur_prices) const {
    int64_t total_delta = 0;
    
    for (const auto& [id, pos] : positions_) {
        if (pos.quantity == 0) continue;

        Bbo bbo = {pos.avg_entry_price, pos.avg_entry_price,};
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
    // Drawdown is a percentage, so we must cast to double here.
    return (max_equity_seen_ - current_equity) / max_equity_seen_;
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

    if (traded_instr_ptr->instrument_type == InstrumentType::FUT) {
        return quantity * traded_instr_ptr->margin_req;
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