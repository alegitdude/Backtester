#pragma once
#include "../data_ingestion/DataReaderManager.h"
#include "../execution/ExecutionHandler.h"
#include "../market_state/MarketStateManager.h"
#include "../portfolio/PortfolioManager.h"
#include "../reporting/ReportGenerator.h"
#include "../strategy/StrategyManager.h"
#include "EventQueue.h"
#include "SPSCRing.h"
#include "Types.h"

namespace backtester {

class Backtester {
 public:
  Backtester(EventQueue& eq, DataReaderManager& drm, MarketStateManager& msm, PortfolioManager& pm,
             ReportGenerator& rg, ExecutionHandler& eh, StrategyManager& sm,
             const AppConfig& config)
      : event_queue_(eq),
        data_reader_manager_(drm),
        market_state_manager_(msm),
        portfolio_manager_(pm),
        report_generator_(rg),
        execution_handler_(eh),
        strategy_manager_(sm),
        config_(config) {}

  int RunLoopSingleThreaded();

 private:
  struct SourceHead {
    EventUnion event;  // next event from this source (valid unless exhausted)
    uint16_t source_id;
    bool exhausted = false;
  };

  // Min-heap of INDICES into source_heads_, ordered by each head's timestamp.
  // Storing indices (not events) lets the comparator read the live head ts.
  struct SourceGreater {
    const std::vector<SourceHead>* heads;
    bool operator()(uint16_t a, uint16_t b) const {
      return Hdr(heads->at(a).event).timestamp >
             Hdr(heads->at(b).event).timestamp;  // '>' => min-heap on top
    }
  };

  static constexpr size_t kCapacity = 1 << 16;

  bool FillRing();
  void ApplyMarket(const MarketByOrderEvent& mbo);
  void ApplySynthetic(const EventUnion& ev, uint64_t current_time);
  void PrimeSources();
  void EmitClosingOrders(timestamp_t close_ts);
  void RecordSnapshot(timestamp_t current_time);

  EventQueue& event_queue_;
  DataReaderManager& data_reader_manager_;
  MarketStateManager& market_state_manager_;
  PortfolioManager& portfolio_manager_;
  ReportGenerator& report_generator_;
  ExecutionHandler& execution_handler_;
  StrategyManager& strategy_manager_;
  const AppConfig& config_;

  std::vector<SourceHead> source_heads_;  // one slot per configured source
  std::vector<uint16_t> source_heap_;

  SPSCRing<EventUnion, kCapacity> ring_;

  bool backtest_complete_ = false;
};

inline bool isMarketEvent(EventType type) {
  return type == EventType::kMarketOrderAdd || type == EventType::kMarketOrderCancel ||
         type == EventType::kMarketOrderModify || type == EventType::kMarketOrderClear ||
         type == EventType::kMarketTrade || type == EventType::kMarketFill ||
         type == EventType::kMarketHeartbeat;
}
inline bool isStrategySignalEvent(EventType type) { return type == EventType::kStrategySignal; }
inline bool isStrategyOrderEvent(EventType type) {
  return type == EventType::kStrategyOrderAdd || type == EventType::kStrategyOrderModify ||
         type == EventType::kStrategyOrderCancel || type == EventType::kStrategyOrderClear;
}
inline bool isControlEvent(EventType type) {
  return type == EventType::kBacktestControlStart ||
         type == EventType::kBacktestControlEndOfBacktest ||
         type == EventType::kBacktestControlSnapshot || type == EventType::kBacktestControlEndOfDay;
}

}  // namespace backtester