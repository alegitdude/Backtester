#include <vector>

void PortfolioManager::process_fill(const FillEvent& fill) {
    // Find or create the position for the symbol
    Position& pos = positions[fill.symbol];

    IF fill.side IS BUY THEN
        // Calculate new cash and average price for a BUY fill
        this->apply_buy_fill(pos, fill);
    ELSE IF fill.side IS SELL THEN
        // Calculate realized PnL and new average price for a SELL fill
        this->apply_sell_fill(pos, fill);
    END IF

    // Record the trade for end-of-backtest reporting
    trade_history.add(TradeRecord{fill.timestamp, fill.symbol, fill.side, fill.fill_price, fill.fill_quantity});

    // Log the PnL and position change
    Logger::trade("FILL processed for " + fill.symbol + ". New Qty: " + pos.total_quantity + ", Cash: " + current_cash);
};

double PortfolioManager::calculate_unrealized_pnl(const Map<string, double>& latest_prices) const {
    double unrealized = 0.0;
    FOR EACH (symbol, pos) IN positions:
        IF pos.total_quantity != 0 AND latest_prices.has(symbol) THEN
            double market_price = latest_prices[symbol];
            double pnl_per_share = market_price - pos.average_entry_price;
            unrealized += pnl_per_share * pos.total_quantity;
    return unrealized;
};

    // Method to get current equity (total wealth)
    double get_current_equity(const Map<string, double>& latest_prices) const {
        return current_cash_ + calculate_unrealized_pnl(latest_prices);
    }

    double get_current_cash() const { return current_cash_; }
    long get_position_quantity(const string& symbol) const { return positions[symbol].total_quantity; }
     
private:
  

    struct Position {
        long total_quantity = 0;
        double average_entry_price = 0.0; 
        double realized_pnl = 0.0;    
    }
}

// Private Helper Methods (Implementation details for PnL)

PRIVATE VOID apply_buy_fill(Position& pos, CONST FillEvent& fill) {
    long old_qty = pos.total_quantity;
    long new_qty = old_qty + fill.fill_quantity;

    // 1. Update Average Entry Price (AEP)
    double total_cost_old = old_qty * pos.average_entry_price;
    double total_cost_new_shares = fill.fill_quantity * fill.fill_price;

    pos.average_entry_price = (total_cost_old + total_cost_new_shares) / new_qty;

    // 2. Update Quantity
    pos.total_quantity = new_qty;

    // 3. Update Cash
    current_cash -= total_cost_new_shares;
}

PRIVATE VOID apply_sell_fill(Position& pos, CONST FillEvent& fill) {
    // Note: This logic assumes long-only or closing short positions.
    // Full short-selling requires more complex accounting.

    long sold_qty = fill.fill_quantity;

    // 1. Calculate Realized PnL (assuming a long position is being reduced)
    double realized_gain_loss = sold_qty * (fill.fill_price - pos.average_entry_price);
    pos.realized_pnl += realized_gain_loss;

    // 2. Update Cash
    current_cash += sold_qty * fill.fill_price;

    // 3. Update Quantity (must be done after PnL calculation)
    pos.total_quantity -= sold_qty;

    // 4. Update AEP (If the position is closed, AEP should be reset to 0)
    IF pos.total_quantity == 0 THEN
        pos.average_entry_price = 0.0;
    // If partial sell, AEP remains the same (FIFO/LIFO requires more complexity)
    END IF
}