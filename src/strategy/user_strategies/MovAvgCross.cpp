#include "../../../include/strategy/IStrategy.h"
#include "../../../include/strategy/StrategyRegistry.h"
#include "../../../include/utils/TimeUtils.h"
#include <spdlog/spdlog.h>

namespace backtester {

class MovAvgCross : public IStrategy {
public:
    time::TimeOfDay time; 
    bool filled_signal = false;
    MovAvgCross(const IMarketDataProvider& market_data) : IStrategy(market_data) {}
    
    virtual void Initialize(const Strategy& config) override {
        return;
    }

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event) override {
        time = backtester::time::GetTimeOfDay(event.timestamp);
        if(time.hour == 15 && time.minute == 30 && !filled_signal){
            auto data = market_data_.GetMarketSnapshots();
            return MakeSignal(kBuySignal, 294973, data[294973].bbo.ask.price, 1, event.timestamp);
        }
        return nullptr;
    }

    virtual void OnFill(const StrategyFillEvent& fill) override {
        filled_signal = true;
    }

    virtual void OnEndOfDay(uint64_t timestamp) override {
        return; 
    }
 private: 
    //const IMarketDataProvider market_data_
};

// ==========================================================
// Required Strategy Registration: Class Name, Strategy Name
// ==========================================================
REGISTER_STRATEGY(MovAvgCross, "MovAvgCrossMin");

}