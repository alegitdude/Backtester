#pragma once
#include <optional>

#include "../core/Event.h"
#include "../core/Types.h"
#include "../market_state/IMarketDataProvider.h"

namespace backtester {

class IStrategy {
 public:
  virtual ~IStrategy() = default;
  virtual void Initialize(const Strategy& config) = 0;
  virtual std::optional<StrategySignalEvent> OnMarketEvent(const MarketByOrderEvent& event) = 0;
  virtual void OnFill(const StrategyFillEvent& fill) = 0;
  virtual void OnRejection(const StrategyOrderRejectionEvent& msg) = 0;
  virtual void OnEndOfDay(uint64_t timestamp) = 0;

  void SetIndex(uint16_t i) noexcept { strategy_idx_ = i; }

  uint16_t GetIndex() const noexcept { return strategy_idx_; }
  std::string GetId() const { return strategy_id_; }

 protected:
  IStrategy(const std::string strategy_id, const IMarketDataProvider& market_data)
      : strategy_id_(strategy_id), market_data_(market_data) {}

  StrategySignalEvent MakeSignal(SignalType signal_type, uint32_t instrument_id, int64_t price,
                                 qty_t quantity, uint64_t timestamp) {
    return StrategySignalEvent{
        .header = {.timestamp = timestamp, .type = EventType::kStrategySignal},
        .strategy_id = strategy_idx_,
        .signal_id = next_signal_id_++,
        .instrument_id = instrument_id,
        .signal_type = signal_type,
        .price = price,
        .quantity = quantity};
  }

  uint16_t strategy_idx_ = 0;
  std::string strategy_id_;
  order_id_t next_signal_id_ = 1;
  const IMarketDataProvider& market_data_;
};

}  // namespace backtester