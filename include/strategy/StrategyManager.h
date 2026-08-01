#pragma once
#include "IStrategy.h"
#include "../core/Event.h"
#include "../core/Types.h"
#include "../market_state/IMarketDataProvider.h"
#include <unordered_map>

namespace backtester {

    class StrategyManager {
    public:
        StrategyManager(const AppConfig& config) : config_(config) {};

        ~StrategyManager() = default;

        void InitializeStrategies(const IMarketDataProvider& provider);

        std::vector<EventUnion>& OnMarketEvent(const MarketByOrderEvent& event);

        void OnFillEvent(const StrategyFillEvent& fill);

        void OnRejectionEvent(const StrategyOrderRejectionEvent& msg);

        std::vector<std::string> GetStrategyNames() const {
            std::vector<std::string> names;
            names.reserve(active_strategies_.size());
            for (const auto& s : active_strategies_) names.push_back(s->GetId());
            return names;
        }

    private:
        const AppConfig& config_;
        //const IMarketDataProvider& data_provider_;
        std::vector<std::unique_ptr<IStrategy>> active_strategies_;
        std::vector<EventUnion> collected_signals_;
    };

}