#include "../../../include/strategy/IStrategy.h"
#include "../../../include/strategy/StrategyRegistry.h"
#include <spdlog/spdlog.h>

namespace {

class MyMovingAverage : IStrategy {
public:
    void MyMovingAverage::OnMarketEvent(const MarketByOrderEvent& event, const OrderBookSnapshot& ob) override {
        // ... Their custom trading logic ...
        spdlog::info("Moving Average strategy processing event...");
    }
};

// ==========================================================
// Required Strategy Registration: Class Name, Strategy Name
// ==========================================================
REGISTER_STRATEGY(MyMovingAverage, "SMA_Cross");

}