#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>

namespace backtester {

struct TradeRecord {
    int an_int;
};

struct Position {
    double entryPrice;
    long entryTime;
    int quantity;

    double UnrealizedPnL(double currentPrice){
        return quantity * (currentPrice - entryPrice);
    }
};

class PortfolioManager {
 public:
    PortfolioManager(double cptl) : initial_capital_(cptl), current_cash_(cptl){}
 private:
    double initial_capital_;
    double current_cash_;
    // Map of Symbol -> Position object (Position holds Qty, Avg Entry Price)
    std::unordered_map<std::string, Position> positions_;
    // History of trades for reporting
    std::vector<TradeRecord> trade_history_;
};

}