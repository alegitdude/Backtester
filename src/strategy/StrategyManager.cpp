#include "strategy/StrategyManager.h"

#include "core/Types.h"
#include "market_state/OBTypes.h"
#include "spdlog/spdlog.h"
#include "strategy/StrategyRegistry.h"

namespace backtester {

void StrategyManager::InitializeStrategies(const IMarketDataProvider& provider) {
  // 1. Iterate through the strategies parsed from JSON
  for (const auto& strat_config : config_.strategies) {
    spdlog::info("Loading strategy: {} with {} parameters", strat_config.name,
                 strat_config.params.size());

    auto strategy = StrategyRegistry::Create(strat_config.name, provider);

    if (!strategy) {
      throw std::runtime_error(fmt::format(
          "StrategyManager Error: Strategy '{}' not found in registry. "
          "Check your JSON config spelling, or ensure the .cpp file has REGISTER_STRATEGY.",
          strat_config.name));
    }

    uint16_t idx = static_cast<uint16_t>(active_strategies_.size());
    strategy->Initialize(strat_config);
    strategy->SetIndex(idx);
    active_strategies_.push_back(std::move(strategy));
  }

  if (active_strategies_.empty()) {
    spdlog::warn(
        "StrategyManager: No strategies loaded. The backtester will consume data but not trade.");
  }
}

std::vector<EventUnion>& StrategyManager::OnMarketEvent(const MarketByOrderEvent& mbo_event) {
  collected_signals_.clear();

  for (auto& strategy : active_strategies_) {
    auto signal = strategy->OnMarketEvent(mbo_event);

    if (signal) {
      collected_signals_.push_back(EventUnion{.strat_signal_ev = *signal});
    }
  }

  return collected_signals_;
}

void StrategyManager::OnFillEvent(const StrategyFillEvent& fill) {
  for (auto& strategy : active_strategies_) {
    if (strategy->GetIndex() == fill.strategy_id) {
      strategy->OnFill(fill);
      return;
    }
  }
  spdlog::warn("StrategyManager: Fill received for unknown strategy_id '{}'", fill.strategy_id);
}

void StrategyManager::OnRejectionEvent(const StrategyOrderRejectionEvent& msg) {
  for (auto& strategy : active_strategies_) {
    if (strategy->GetIndex() == msg.strategy_id) {
      strategy->OnRejection(msg);
      return;
    }
  }
  spdlog::warn("StrategyManager: Fill received for unknown strategy_id '{}'", msg.strategy_id);
};

}  // namespace backtester