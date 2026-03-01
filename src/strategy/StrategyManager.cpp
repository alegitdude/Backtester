#include "../../include/strategy/StrategyManager.h"
#include "../../include/core/Types.h"
#include "../../include/market_state/OBTypes.h"
#include "spdlog/spdlog.h"
#include <vector>
namespace backtester {

StrategyManager::StrategyManager(const AppConfig& config) 
    : config_(config) {};


// void StrategyManager::InitiailizeStrategies() {
//     active_strategies_.clear();
//     strategy_lookup_.clear();

//     for (const auto& strat_config : config_.strategies) {
//        std::unique_ptr<IStrategy> strategy = nullptr;

//         // --- Strategy Factory Logic ---
//         // In the future, you can move this switch/if logic to a separate Factory class
//         if (strat_config.name == "MovAvgCrossMin") {
//             strategy = std::make_unique<MovAvgCrossMin>();
//         } 
//         // else if (strat_config.name == "HftScalper") { ... }
//         else {
//             spdlog::warn("Unknown strategy name in config: {}", strat_config.name);
//             continue;
//         }

//         if (strategy) {
//             strategy->Initialize(strat_config);
//             // Index by ID for fast fill routing
//             strategy_lookup_[strategy->GetId()] = strategy.get();
//             active_strategies_.push_back(std::move(strategy));
//         }
//     }
// }

// std::vector<std::unique_ptr<StrategySignalEvent>> StrategyManager::OnMarketEvent(
//     const MarketByOrderEvent& event, 
//     const std::vector<BidAskPair>& ob_snapshot) {

//     std::vector<std::unique_ptr<StrategySignalEvent>> collected_signals;

//     // Check if the event is actually a market event before casting
//     // In Types.h you likely have a helper or you cast based on type range
//     if (event->type < EventType::kMarketOrderAdd || event->type > EventType::kMarketHeartbeat) {
//         return collected_signals; 
//     }

//     // Cast to MBO event safely
//     const MarketByOrderEvent* mbo_event = static_cast<const MarketByOrderEvent*>(event.get());

//     // Loop through all active strategies
//     for (auto& strategy : active_strategies_) {
//         // You might want to filter strategies by symbol here if they are symbol-specific
        
//         auto signal = strategy->OnMarketEvent(*mbo_event, ob_snapshot);
        
//         if (signal) {
//             // Signal generated! Add to collection.
//             collected_signals.push_back(std::move(signal));
//         }
//     }

//     return collected_signals;
// }

void StrategyManager::OnFillEvent(const StrategyFillEvent& fill) {
    auto it = strategy_lookup_.find(fill.strategy_id);
    if (it != strategy_lookup_.end()) {
        it->second->OnFill(fill);
    }    
}

}