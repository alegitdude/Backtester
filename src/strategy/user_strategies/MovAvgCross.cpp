#include "../../../include/strategy/IStrategy.h"
#include "../../../include/strategy/StrategyRegistry.h"
#include "../../../include/utils/TimeUtils.h"
#include <spdlog/spdlog.h>

namespace backtester {

class MovAvgCross : public IStrategy {
public:
    time::TimeOfDay time; 
    bool filled_signal = false;
    MovAvgCross(const std::string& strategy_id, const IMarketDataProvider& market_data) : 
        IStrategy(strategy_id, market_data) {}
    
    virtual void Initialize(const Strategy& config) override {
        return;
    }

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event) override {
        time = backtester::time::GetTimeOfDay(event.timestamp);
        if(time.hour >= 15 && time.minute == 30 && !filled_signal){
            auto data = market_data_.GetMarketSnapshots();
            if(data[294973].last_trade.price != 0){
                return MakeSignal(kBuySignal, 294973, data[294973].bbo.ask.price, 1, event.timestamp);
            }
            //return MakeSignal(kBuySignal, 294973, data[294973].bbo.ask.price, 1, event.timestamp);
        }
        return nullptr;
    }

    virtual void OnFill(const StrategyFillEvent& fill) override {
        filled_signal = true;
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