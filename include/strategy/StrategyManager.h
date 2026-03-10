#pragma once
#include "IStrategy.h"
#include "../core/Event.h"
#include "../core/Types.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace backtester {

class StrategyManager {
 public:
    StrategyManager(const AppConfig& config);
   // StrategyManager(const AppConfig& strategies);
    
    ~StrategyManager() = default;

    void InitiailizeStrategies();

    std::vector<std::unique_ptr<StrategySignalEvent>> OnMarketEvent(
    const MarketByOrderEvent& event, 
    const std::vector<BidAskPair>& ob_snapshot);
    
    void OnFillEvent(const StrategyFillEvent& fill);

 private: 
    const AppConfig& config_;
    std::vector<std::unique_ptr<IStrategy>> active_strategies_;
    std::unordered_map<std::string, IStrategy*> strategy_lookup_;
};

}