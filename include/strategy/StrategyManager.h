#pragma once

#include "../core/Event.h"
#include <memory>

namespace backtester {

class IStrategy {
 public:
    virtual ~IStrategy() = default;

    virtual void Initialize(const Strategy config);

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event,
        const std::vector<BidAskPair> ob_snapshot) ;  // return signal if generated, else nullptr

    // Called when your order fills
    virtual void OnFill(const FillEvent& fill) ;  // update internal state (e.g., position)

    // Optional: End-of-day or shutdown metrics
    virtual void OnEndOfDay(uint64_t timestamp) ;

};

class StrategyManager {
 public:
    StrategyManager(ExecutionHandler& executionHandler_, 
      const std::vector<Strategy> strategies);
    
    void InitiailizeStrategies();

    std::vector<std::unique_ptr<StrategySignalEvent>> OnMarketEvent(
    const std::unique_ptr<Event>& event, 
    const std::vector<BidAskPair> ob_snapshot);

 private: 
    ExecutionHandler executionHandler_;
    std::vector<IStrategy*> active_strategies_;
};

}