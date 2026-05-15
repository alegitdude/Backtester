#include "../../include/strategy/StrategyManager.h"
#include "../../include/core/Types.h"
#include "../../include/market_state/OBTypes.h"
#include "../../include/strategy/StrategyRegistry.h"
#include "spdlog/spdlog.h"
#include <vector>
namespace backtester {

void StrategyManager::InitiailizeStrategies(const IMarketDataProvider& provider) {
    // 1. Iterate through the strategies parsed from JSON
    for (const auto& strat_config : config_.strategies) {
        spdlog::info("Loading strategy: {} with {} parameters",
            strat_config.name, strat_config.params.size());

        auto strategy = StrategyRegistry::Create(strat_config.name, provider);

        if (!strategy) {
            throw std::runtime_error(fmt::format(
                "StrategyManager Error: Strategy '{}' not found in registry. "
                "Check your JSON config spelling, or ensure the .cpp file has REGISTER_STRATEGY.",
                strat_config.name
            ));
        }
        
        strategy->Initialize(strat_config);

        active_strategies_.push_back(std::move(strategy));
    }

    if (active_strategies_.empty()) {
        spdlog::warn("StrategyManager: No strategies loaded. The backtester will consume data but not trade.");
    }
}

std::vector<std::unique_ptr<StrategySignalEvent>> StrategyManager::OnMarketEvent(
    const MarketByOrderEvent& mbo_event) {

    std::vector<std::unique_ptr<StrategySignalEvent>> collected_signals;

    for (auto& strategy : active_strategies_) {
        // You might want to filter strategies by symbol here if they are symbol-specific
        auto signal = strategy->OnMarketEvent(mbo_event);

        if (signal) {
            collected_signals.push_back(std::move(signal));
        }
    }

    return collected_signals;
}

void StrategyManager::OnFillEvent(const StrategyFillEvent& fill) {
    for (auto& strategy : active_strategies_) {
        if (strategy->GetId() == fill.strategy_id) {
            strategy->OnFill(fill);
            return;
        }
    }
    spdlog::warn("StrategyManager: Fill received for unknown strategy_id '{}'", fill.strategy_id);
}

}