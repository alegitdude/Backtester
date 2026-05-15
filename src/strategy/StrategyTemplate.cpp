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
        const MarketByOrderEvent& event) override {
        // ... Your custom trading logic ...
        // helper function MakeSignal returns unique_ptr<StrategySignalEvent> and takes
        // signal_type, instr, price, qty, timestamp - Example:
        // return MakeSignal(kBuySignal, 294973, data[294973].bbo.ask.price, 1, event.timestamp);
        return nullptr;
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
REGISTER_STRATEGY(Template, "Blank_Template");

}