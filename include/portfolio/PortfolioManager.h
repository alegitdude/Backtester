#pragma once
#include <algorithm>
#include <unordered_map>

#include "../core/Event.h"
#include "../core/Types.h"
#include "../market_state/IMarketDataProvider.h"
#include "../market_state/OBTypes.h"

namespace backtester {

class PortfolioManager {
 public:
  PortfolioManager(const AppConfig& config, const IMarketDataProvider& market_snapshots);
  ~PortfolioManager() = default;

  // =========================================================================
  // MARK: Core Signal & Order Handling
  // =========================================================================

  EventUnion RequestOrder(const StrategySignalEvent& signal);

  void ProcessFill(const StrategyFillEvent& fill);

  void OpenOrIncrease(Position& pos, const TradedInstrument* instr, const StrategyFillEvent& fill,
                      int64_t fill_qty_signed);

  int64_t CloseOrReduce(Position& pos, const TradedInstrument* instr, const StrategyFillEvent& fill,
                        int64_t fill_qty_signed);

  void CancelAllPendingOrders();

  inline void UpdateMaxEquity() {
    max_equity_seen_ = std::max(max_equity_seen_, GetTotalEquity());
  };

  // =========================================================================
  // MARK: Metrics & PnL Accessors (All return Scaled int64_t)
  // =========================================================================

  // Equity = Cash + Unrealized PnL
  int64_t GetTotalEquity() const;

  // Buying Power = Equity - Margin Used
  money_t GetBuyingPower(InstrumentType instr_type) const;

  // Returns PnL for a specific position object against a current price
  money_t GetUnrealizedPnL(const Position& pos, const BidAskPair& current_price) const;

  // Sum of Dollar Deltas for all positions
  int64_t GetTotalPortfolioDelta() const;

  // Dollar/Currency Delta for a specific instrument
  int64_t GetInstrPosDelta(uint32_t instrument_id, BidAskPair current_Bbo) const;

  // Returns a ratio (0.0 to 1.0)
  money_t GetCurrentDrawdown(money_t current_equity) const;

  // =========================================================================
  // MARK: Simple Accessors
  // =========================================================================

  // Returns the Position object (copy or const ref)
  const Position& GetPositionByInstrId(uint32_t instrument_id) const;
  const std::vector<Position>& GetPositions() const { return positions_; }
  // Returns signed quantity
  int64_t GetPositionQty(uint32_t instrument_id) const;

  bool HasAnyOpenPosition() const { return !positions_.empty(); }

  int64_t GetCash() const { return current_cash_; }
  int64_t GetRealizedPnL() const { return total_realized_pnl_; }
  money_t GetMaxEquitySeen() const { return max_equity_seen_; }

  const std::vector<TradeRecord>& GetTradeHistory() const { return trade_history_; }

 private:
  struct PortfolioPendingOrder {
    order_id_t order_id;
    uint32_t instrument_id;
    qty_t remaining_qty;  // can be negative for shorts
    money_t per_qty_margin;
    money_t per_qty_com;

    PortfolioPendingOrder(order_id_t id, uint32_t in_id, qty_t qty, money_t qty_marg,
                          money_t qty_com)
        : order_id(id),
          instrument_id(in_id),
          remaining_qty(qty),
          per_qty_margin(qty_marg),
          per_qty_com(qty_com) {}
  };

  // =========================================================================
  // MARK: Internal Logic Handlers
  // =========================================================================

  EventUnion HandleAddRequest(const StrategySignalEvent& signal);
  EventUnion HandleModifyRequest(const StrategySignalEvent& signal);
  EventUnion HandleCancelRequest(const StrategySignalEvent& signal);

  // =========================================================================
  // MARK: Helper Utilities
  // =========================================================================
  inline Position* FindPosition(uint32_t instrument_id, uint16_t strategy_id) {
    for (auto& pos : positions_) {
      if (pos.instrument_id == instrument_id && pos.strategy_id == strategy_id) {
        return &pos;
      }
    }
    return nullptr;
  }

  inline const Position* FindPosition(uint32_t instrument_id, uint16_t strategy_id) const {
    for (const auto& pos : positions_) {
      if (pos.instrument_id == instrument_id && pos.strategy_id == strategy_id) {
        return &pos;
      }
    }
    return nullptr;
  }

  // Calculates required margin/cash for a specific quantity and price
  money_t CalcPerUnitMarginReq(uint32_t instrument_id, price_t price) const;

  money_t GetCommissionsByInstr(uint32_t instrument_id, qty_t fill_qty);

  // Validates that a price is a valid multiple of the tick size (Integer Modulo)
  inline bool IsValidTick(const TradedInstrument& instr, price_t price) const {
    int64_t tick_size = instr.tick_size;
    if (tick_size == 0) return true;  // Safety
    return (price % tick_size) == 0;
  }

  inline const TradedInstrument* GetTradedInstr(uint32_t instrument_id) const {
    auto it = std::find_if(config_.traded_instruments.begin(), config_.traded_instruments.end(),
                           [instrument_id](const TradedInstrument& traded_instr) {
                             return traded_instr.instrument_id == instrument_id;
                           });
    if (it == config_.traded_instruments.end()) {
      return nullptr;
    }
    return &(*it);
  }

  // =========================================================================
  // MARK: Member Variables
  // =========================================================================

  const AppConfig& config_;
  const IMarketDataProvider& market_snapshots_;
  money_t initial_capital_;
  money_t current_cash_;
  money_t total_realized_pnl_ = 0;
  money_t max_equity_seen_ = 0;
  money_t maintenance_margin_used_ = 0;
  money_t reserved_margin_used_ = 0;

  std::vector<PortfolioPendingOrder> pending_orders_;
  std::vector<Position> positions_;
  std::vector<TradeRecord> trade_history_;
};

}  // namespace backtester