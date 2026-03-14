#include "../../../include/strategy/IStrategy.h"
#include "../../../include/strategy/StrategyRegistry.h"
#include <spdlog/spdlog.h>

namespace backtester {

class MovAvgCross : public IStrategy {
public:
    virtual void Initialize(const Strategy& config) override {
        return;
    }

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event) override {
    
        //spdlog::info("Moving Average strategy processing event...");
    }

    virtual void OnFill(const StrategyFillEvent& fill) override {
        return;
    }

    virtual void OnEndOfDay(uint64_t timestamp) override {
        return; 
    }
};

// ==========================================================
// Required Strategy Registration: Class Name, Strategy Name
// ==========================================================
REGISTER_STRATEGY(MovAvgCross, "SMA_Cross");

}