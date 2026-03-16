#pragma once
#include "../core/Event.h"
#include "../core/Types.h"
#include "../market_state/IMarketDataProvider.h"
#include <memory>
#include <vector>

namespace backtester {

class IStrategy {
 public:
    virtual ~IStrategy() = default;

    virtual void Initialize(const Strategy& config) = 0;

    virtual std::unique_ptr<StrategySignalEvent> OnMarketEvent(
        const MarketByOrderEvent& event) = 0;

    virtual void OnFill(const StrategyFillEvent& fill) = 0;

    virtual void OnEndOfDay(uint64_t timestamp) = 0;

    std::string GetId() const { return strategy_id_; }

 protected:
    IStrategy(const IMarketDataProvider& market_data)
        : market_data_(market_data) {}

    std::unique_ptr<StrategySignalEvent> MakeSignal(
        SignalType signal_type,
        uint32_t instrument_id,
        int64_t price,
        uint32_t quantity,
        uint64_t timestamp) {
    
        return std::make_unique<StrategySignalEvent>(
            timestamp,
            EventType::kStrategySignal,
            next_signal_id_++,
            strategy_id_,
            instrument_id,
            signal_type,
            price,
            quantity
        );
    }

    std::string strategy_id_;
    int32_t next_signal_id_ = 0;
    const IMarketDataProvider& market_data_;
};

}