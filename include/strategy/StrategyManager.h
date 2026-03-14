#pragma once
#include "IStrategy.h"
#include "../core/Event.h"
#include "../core/Types.h"
#include "../market_state/IMarketDataProvider"
#include <memory>
#include <vector>
#include <unordered_map>

namespace backtester {

class StrategyManager {
 public:
     StrategyManager(const AppConfig& config) : config_(config) {};
    
    ~StrategyManager() = default;

    void InitiailizeStrategies(const IMarketDataProvider& provider);

    std::vector<std::unique_ptr<StrategySignalEvent>> OnMarketEvent(
    const MarketByOrderEvent& event);
    
    void OnFillEvent(const StrategyFillEvent& fill);

 private: 
    const AppConfig& config_;
    //const IMarketDataProvider& data_provider_;
    std::vector<std::unique_ptr<IStrategy>> active_strategies_;
    std::unordered_map<std::string, IStrategy*> strategy_lookup_;
};

}