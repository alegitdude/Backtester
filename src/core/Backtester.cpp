#include "core/Backtester.h"

#include "core/Types.h"
#include "spdlog/spdlog.h"

namespace backtester {

int Backtester::RunLoop(const AppConfig& config) {
  EventUnion mbo_event;

  spdlog::info("Populating initial events from data sources...");
  for (const DataSourceConfig& source : config.data_configs) {
    if (data_reader_manager_.LoadNextEventFromSource(source.data_source_id, mbo_event.mbo)) {
      event_queue_.PushEvent(mbo_event);
    } else {
      spdlog::warn("Symbol " + source.data_source_name + " has no events.");
    }
  }

  spdlog::info("Starting backtest loop...");
  uint64_t current_time;
  uint64_t last_snapshot_ts_ = 0;
  uint64_t event_tally = 0;
  bool backtest_complete = false;
  bool end_of_bt_pushed = false;

  const auto t0 = std::chrono::steady_clock::now();

  while (!event_queue_.IsEmpty()) {
    auto current_event = event_queue_.PopTopEvent();
    current_time = Hdr(current_event).timestamp;
    EventType eventType = Hdr(current_event).type;
    event_tally++;

    if (isMarketEvent(eventType)) {
      const MarketByOrderEvent& mbo = current_event.mbo;
      market_state_manager_.OnMarketEvent(mbo);

      if (current_time >= config.start_time) {
        auto signals = strategy_manager_.OnMarketEvent(mbo);

        if (signals.size() > 0) {
          for (size_t i = 0; i < signals.size(); i++) {
            event_queue_.PushEvent(signals[i]);
          }
        }

        execution_handler_.OnMarketEvent(mbo);
        if (portfolio_manager_.HasAnyOpenPosition()) {
          portfolio_manager_.UpdateMaxEquity();
        }
      }

      if (!backtest_complete) {
        if (data_reader_manager_.LoadNextEventFromSource(mbo.data_source_id, mbo_event.mbo)) {
          event_queue_.PushEvent(mbo_event);
        }
      }
    }

    if (isStrategySignalEvent(eventType)) {
      const StrategySignalEvent& signal_event = current_event.strat_signal_ev;

      auto order_event = portfolio_manager_.RequestOrder(signal_event);
      const EventType t = Hdr(order_event).type;

      if (isStrategyOrderEvent(t) || t == EventType::kStrategyOrderRejection) {
        event_queue_.PushEvent(std::move(order_event));
        spdlog::debug("Queued order from signal at ts={}", current_time);
      }
    }

    if (isStrategyOrderEvent(eventType)) {
      const StrategyOrderEvent& order_event = current_event.strat_order_ev;

      execution_handler_.OnStrategyOrder(order_event);
    }

    if (eventType == EventType::kStrategyOrderFill) {
      const StrategyFillEvent& fill_event = current_event.strat_fill_ev;

      portfolio_manager_.ProcessFill(fill_event);
      strategy_manager_.OnFillEvent(fill_event);
    }

    if (eventType == EventType::kStrategyOrderRejection) {
      const StrategyOrderRejectionEvent& rejection = current_event.strat_rej_ev;
      strategy_manager_.OnRejectionEvent(rejection);
    }

    if (isControlEvent(eventType)) {
      if (eventType == EventType::kBacktestControlEndOfBacktest && !backtest_complete) {
        // Cancel all pending orders first
        execution_handler_.CancelAllPendingOrders();
        portfolio_manager_.CancelAllPendingOrders();
        // Emit EOD orders
        EmitClosingOrders(current_time);
        // Notify strategies
        // strategy_manager_.OnEndOfDay(current_time);
        backtest_complete = true;
      }
    }

    if (((event_queue_.IsEmpty() || current_time > config.end_time) && !backtest_complete) &&
        !end_of_bt_pushed) {
      event_queue_.PushEvent(
          EventUnion{.control_ev = {.header = {.timestamp = current_time,
                                               .type = EventType::kBacktestControlEndOfBacktest}}});
      end_of_bt_pushed = true;
    }
    if (current_time >= config.start_time &&
        current_time - last_snapshot_ts_ >= config.snapshot_interval_ns) {
      RecordSnapshot(current_time);
      last_snapshot_ts_ = current_time;
    }
  }

  const auto elapsed = std::chrono::steady_clock::now() - t0;
  const double secs = std::chrono::duration<double>(elapsed).count();
  spdlog::info("Loop: {} events  {:.3f}s  {:.2f} M evt/s", event_tally, secs,
               static_cast<double>(event_tally) / secs / 1e6);

  spdlog::info("Event tally: {}", event_tally);
  spdlog::info("Backtest loop finished.");
  spdlog::info("Starting report generator.");
  report_generator_.GenerateReport(portfolio_manager_, strategy_manager_.GetStrategyNames());
  spdlog::info("Report finished.");

  spdlog::info("Backtester shutting down.");

  return 0;
}

void Backtester::EmitClosingOrders(timestamp_t close_ts) {
  auto current_prices = market_state_manager_.GetTradedInstrsBbo();

  for (const auto& pos : portfolio_manager_.GetPositions()) {
    if (pos.quantity == 0) continue;

    auto it = current_prices.find(pos.instrument_id);
    if (it == current_prices.end()) continue;

    SignalType signal_type = pos.quantity > 0 ? SignalType::kSellSignal : SignalType::kBuySignal;
    int64_t close_price = pos.quantity > 0 ? it->second.bid.price : it->second.ask.price;

    if (close_price == 0 || close_price == kUndefPrice) {
      spdlog::warn(
          "EmitClosingOrders: No valid price for instrument {}, "
          "using avg entry price",
          pos.instrument_id);
      close_price = pos.avg_entry_price;
    }

    auto signal =
        StrategySignalEvent{.header = {.timestamp = close_ts, .type = EventType::kStrategySignal},
                            .strategy_id = pos.strategy_id,
                            .signal_id = -1,
                            .instrument_id = pos.instrument_id,
                            .signal_type = signal_type,
                            .price = close_price,
                            .quantity = std::abs(pos.quantity)};

    event_queue_.PushEvent(EventUnion{.strat_signal_ev = signal});
    spdlog::info("EmitClosingOrders: strategy={} instrument={} qty={} price={}", pos.strategy_id,
                 pos.instrument_id, pos.quantity, close_price);
  }
}

void Backtester::RecordSnapshot(timestamp_t current_time) {
  auto current_prices = market_state_manager_.GetTradedInstrsBbo();
  money_t equity = portfolio_manager_.GetTotalEquity();
  money_t cash = portfolio_manager_.GetCash();
  money_t realized = portfolio_manager_.GetRealizedPnL();
  money_t unrealized = equity - cash;
  money_t drawdown = portfolio_manager_.GetMaxEquitySeen() - equity;
  bool has_position = portfolio_manager_.HasAnyOpenPosition();

  report_generator_.RecordEquitySnapshot(current_time, equity, cash, realized, unrealized, drawdown,
                                         has_position);
}

}  // namespace backtester