#include "../../../include/strategy/IStrategy.h"
#include "../../../include/strategy/StrategyRegistry.h"
#include <spdlog/spdlog.h>

namespace backtester {

class Template : public IStrategy {
public:
    virtual void Initialize(const Strategy& config) override {
        return;
    }

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event,
        const std::vector<BidAskPair>& ob_snapshot) override {
        // ... Your custom trading logic ...
        spdlog::info("Template strategy processing event...");
    }

    virtual void OnFill(const StrategyFillEvent& fill) override {
        return;
    }

    virtual void OnEndOfDay(uint64_t timestamp) override {
        return; 
    }

    std::string GetId() const { return strategy_id_; }
 protected:
    std::string strategy_id_;  
};

// ==========================================================
// Required Strategy Registration: Class Name, Strategy Name
// ==========================================================
REGISTER_STRATEGY(Template, "Blank_Template");

}