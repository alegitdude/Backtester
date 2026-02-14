#pragma once
#include "../core/Types.h"
#include "../core/Event.h"
#include "../market_state/OBTypes.h"
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>

namespace backtester {

class PortfolioManager {
 public:
    PortfolioManager(const AppConfig& config);
    ~PortfolioManager() = default;

    // =========================================================================
    // MARK: Core Signal & Order Handling
    // =========================================================================

    std::unique_ptr<StrategyOrderEvent> RequestOrder(
        const StrategySignalEvent* signal, 
        const std::unordered_map<uint32_t, Bbo>& latest_prices);

    void ProcessFill(const FillEvent& fill);

    // =========================================================================
    // MARK: Metrics & PnL Accessors (All return Scaled int64_t)
    // =========================================================================

    // Equity = Cash + Unrealized PnL
    int64_t GetTotalEquity(const std::unordered_map<uint32_t, Bbo>& latest_prices) const;
    
    // Buying Power = Equity - Margin Used
    int64_t GetBuyingPowerAvailable(const std::unordered_map<uint32_t, Bbo>& latest_prices) const;
    
    // Returns PnL for a specific position object against a current price
    int64_t GetUnrealizedPnL(const Position& pos, Bbo current_price) const;

    // Sum of Dollar Deltas for all positions
    int64_t GetTotalPortfolioDelta(const std::unordered_map<uint32_t, Bbo>& latest_prices) const;

    // Dollar/Currency Delta for a specific instrument
    // Futures Dollar Value = (Price / TickSize) * TickValue
    int64_t GetDelta(uint32_t instrument_id, Bbo current_Bbo) const;

    // Returns a ratio (0.0 to 1.0)
    int64_t GetCurrentDrawdown(int64_t current_equity) const;

    // =========================================================================
    // MARK: Simple Accessors
    // =========================================================================
    
    // Returns the Position object (copy or const ref)
    const Position& GetPositionByInstrId(uint32_t instrument_id) const;
    
    // Returns signed quantity
    int64_t GetPositionQty(uint32_t instrument_id) const;
    
    // Returns true if quantity != 0
    bool HasPosition(uint32_t instrument_id) const;

    int64_t GetCash() const { return current_cash_; }
    int64_t GetRealizedPnL() const { return total_realized_pnl_; }
    int64_t GetMaxEquitySeen() const { return max_equity_seen_; }
    
    const std::vector<TradeRecord>& GetTradeHistory() const { return trade_history_; }

 private:
    // =========================================================================
    // MARK: Internal Logic Handlers
    // =========================================================================

    std::unique_ptr<StrategyOrderEvent> HandleAddRequest(
        const StrategySignalEvent* signal, 
        const std::unordered_map<uint32_t, Bbo>& latest_prices);

    std::unique_ptr<StrategyOrderEvent> HandleModifyRequest(const StrategySignalEvent* signal);
    std::unique_ptr<StrategyOrderEvent> HandleCancelRequest(const StrategySignalEvent* signal);

    // =========================================================================
    // MARK: Helper Utilities
    // =========================================================================

    // Calculates required margin/cash for a specific quantity and price
    int64_t CalculateMarginRequirement(uint32_t instrument_id, int64_t quantity, 
        int64_t price) const;
    
    // Validates that a price is a valid multiple of the tick size (Integer Modulo)
    bool IsValidTick(uint32_t instrument_id, int64_t price) const;

    inline const TradedInstrument* GetTradedInstr(uint32_t instrument_id) const{
        for (auto& instr : config_.traded_instruments) {
            if (instr.instrument_id == instrument_id) return &instr;
        } 
        return nullptr;
    }

    // =========================================================================
    // MARK: Member Variables
    // =========================================================================

    int64_t initial_capital_;
    int64_t current_cash_;
    int64_t total_realized_pnl_ = 0;
    int64_t max_equity_seen_ = 0; 

    const AppConfig& config_; 
    
    std::unordered_map<uint32_t, Position> positions_;
    std::vector<TradeRecord> trade_history_;
};

}