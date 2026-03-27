#include "../../../include/strategy/IStrategy.h"
#include "../../../include/strategy/StrategyRegistry.h"
#include "../../../include/utils/TimeUtils.h"
#include <spdlog/spdlog.h>

namespace backtester {

class MovAvgCross : public IStrategy {
public:
    time::TimeOfDay time; 
    bool sent_signal = false;
    MovAvgCross(const IMarketDataProvider& market_data_) : IStrategy(market_data_) {}
    
    virtual void Initialize(const Strategy& config) override {
        return;
    }

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event) override {
        time = backtester::time::GetTimeOfDay(event.timestamp);
        if(time.hour == 20 && !sent_signal){
            std::unordered_map data = market_data_.GetMarketSnapshots();
            sent_signal = true;
            return MakeSignal(kBuySignal, 294973, data[294973].bbo.ask.price, 1, event.timestamp);
        }
        return nullptr;
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
REGISTER_STRATEGY(MovAvgCross, "MovAvgCrossMin");

}