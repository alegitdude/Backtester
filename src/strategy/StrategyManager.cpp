#include "../../include/strategy/StrategyManager.h"
#include "../../include/core/Types.h"
#include "../../include/market_state/OBTypes.h"
#include "../../include/strategy/StrategyRegistry.h"
#include "spdlog/spdlog.h"
#include <vector>
namespace backtester {

    StrategyManager::StrategyManager(const AppConfig& config) : config_(config) {

        // 1. Iterate through the strategies parsed from JSON
        for (const auto& strat_config : config.strategies) {
            spdlog::info("Loading strategy: {} with {} parameters",
                strat_config.name, strat_config.params.size());

            // 2. Ask the global registry to create it by its string name
            auto strategy = StrategyRegistry::Create(strat_config.name);

            // 3. Fail-fast if the strategy wasn't found
            if (!strategy) {
                throw std::runtime_error(fmt::format(
                    "StrategyManager Error: Strategy '{}' not found in registry. "
                    "Check your JSON config spelling, or ensure the .cpp file has REGISTER_STRATEGY.",
                    strat_config.name
                ));
            }

            // 4. Pass the parsed JSON parameters into the strategy
            strategy->Initialize(strat_config);

            // 5. Store the unique_ptr in our active list
            active_strategies_.push_back(std::move(strategy));
        }

        if (active_strategies_.empty()) {
            spdlog::warn("StrategyManager: No strategies loaded. The backtester will consume data but not trade.");
        }
    }

    std::vector<std::unique_ptr<StrategySignalEvent>> StrategyManager::OnMarketEvent(
        const MarketByOrderEvent& mbo_event, 
        const std::vector<BidAskPair>& ob_snapshot) {

        std::vector<std::unique_ptr<StrategySignalEvent>> collected_signals;

        for (auto& strategy : active_strategies_) {
            // You might want to filter strategies by symbol here if they are symbol-specific

            auto signal = strategy->OnMarketEvent(mbo_event, ob_snapshot);

            if (signal) {
                // Signal generated! Add to collection.
                collected_signals.push_back(std::move(signal));
            }
        }

        return collected_signals;
    }

    void StrategyManager::OnFillEvent(const StrategyFillEvent& fill) {
        auto it = strategy_lookup_.find(fill.strategy_id);
        if (it != strategy_lookup_.end()) {
            it->second->OnFill(fill);
        }
    }

}