#include "core/Backtester.h"

#include "core/Types.h"
#include "spdlog/spdlog.h"

namespace backtester {

// MARK: Run Loop
int Backtester::RunLoopSingleThreaded() {
  // Prime the ring with the first market events.
  PrimeSources();
  FillRing();

  uint64_t current_time = 0;
  uint64_t last_snapshot_ts_ = 0;
  uint64_t event_tally = 0;
  backtest_complete_ = false;
  bool end_of_bt_pushed = false;

  const auto t0 = std::chrono::steady_clock::now();
  spdlog::info("Starting backtest loop...");
  // Main Loop
  while (ring_.PeekRead() != nullptr || !event_queue_.IsEmpty()) {
    const EventUnion* mkt_ev = ring_.PeekRead();
    const bool synth_ev = !event_queue_.IsEmpty();

    bool take_market;
    if (mkt_ev && synth_ev) {
      take_market = Hdr(*mkt_ev).timestamp <= Hdr(event_queue_.ReadTopEvent()).timestamp;
    } else {
      take_market = (mkt_ev != nullptr);
    }

    if (take_market) {
      const MarketByOrderEvent mbo = mkt_ev->mbo;  // copy out before CommitRead
      current_time = mbo.header.timestamp;
      ring_.CommitRead();  // free slot AFTER copying out

      ApplyMarket(mbo);

      if (!backtest_complete_) FillRing();
    } else {
      EventUnion ev = event_queue_.PopTopEvent();
      current_time = Hdr(ev).timestamp;
      ApplySynthetic(ev, current_time);
    }
    ++event_tally;

    // EO_BT trigger
    const bool streams_dry = (ring_.PeekRead() == nullptr) && event_queue_.IsEmpty();
    if (((streams_dry || current_time > config_.end_time) && !backtest_complete_) &&
        !end_of_bt_pushed) {
      event_queue_.PushEvent(
          EventUnion{.control_ev = {.header = {.timestamp = current_time,
                                               .type = EventType::kBacktestControlEndOfBacktest}}});
      end_of_bt_pushed = true;
    }

    if (current_time >= config_.start_time &&
        current_time - last_snapshot_ts_ >= config_.snapshot_interval_ns) {
      RecordSnapshot(current_time);
      last_snapshot_ts_ = current_time;
    }
  }

  const auto elapsed = std::chrono::steady_clock::now() - t0;
  const double secs = std::chrono::duration<double>(elapsed).count();
  spdlog::info("Loop: {} events  {:.3f}s  {:.2f} M evt/s", event_tally, secs,
               static_cast<double>(event_tally) / secs / 1e6);

  report_generator_.GenerateReport(portfolio_manager_, strategy_manager_.GetStrategyNames());
  return 0;
}

// MARK: Fill Ring
bool Backtester::FillRing() {
  if (source_heap_.empty()) return false;  // every source drained

  EventUnion* slot = ring_.PrepareWrite();
  if (!slot) return true;  // ring full this turn (not EOF)

  // Earliest-timestamp source sits at the heap top.
  std::pop_heap(source_heap_.begin(), source_heap_.end(), SourceGreater{&source_heads_});
  const uint16_t idx = source_heap_.back();
  SourceHead& head = source_heads_[idx];

  *slot = head.event;  // publish earliest event
  ring_.CommitWrite();

  // Advance that one source to its next event.
  head.exhausted = !data_reader_manager_.LoadNextEventFromSource(head.source_id, head.event.mbo);

  if (head.exhausted) {
    source_heap_.pop_back();  // drop drained source
  } else {
    std::push_heap(source_heap_.begin(), source_heap_.end(),
                   SourceGreater{&source_heads_});  // re-insert w/ new ts
  }
  return true;
}

// MARK: Apply Market
void Backtester::ApplyMarket(const MarketByOrderEvent& mbo) {
  market_state_manager_.OnMarketEvent(mbo);

  const uint64_t current_time = mbo.header.timestamp;
  if (current_time >= config_.start_time) {
    auto signals = strategy_manager_.OnMarketEvent(mbo);
    for (size_t i = 0; i < signals.size(); ++i) {
      event_queue_.PushEvent(signals[i]);
    }
    execution_handler_.OnMarketEvent(mbo);
    if (portfolio_manager_.HasAnyOpenPosition()) {
      portfolio_manager_.UpdateMaxEquity();
    }
  }
}

// MARK: Apply Sythentic
void Backtester::ApplySynthetic(const EventUnion& ev, uint64_t current_time) {
  const EventType type = Hdr(ev).type;

  if (isStrategySignalEvent(type)) {
    auto order_event = portfolio_manager_.RequestOrder(ev.strat_signal_ev);
    const EventType t = Hdr(order_event).type;
    if (isStrategyOrderEvent(t) || t == EventType::kStrategyOrderRejection) {
      event_queue_.PushEvent(order_event);
    }
  } else if (isStrategyOrderEvent(type)) {
    execution_handler_.OnStrategyOrder(ev.strat_order_ev);
  } else if (type == EventType::kStrategyOrderFill) {
    portfolio_manager_.ProcessFill(ev.strat_fill_ev);
    strategy_manager_.OnFillEvent(ev.strat_fill_ev);
  } else if (type == EventType::kStrategyOrderRejection) {
    strategy_manager_.OnRejectionEvent(ev.strat_rej_ev);
  } else if (isControlEvent(type)) {
    if (type == EventType::kBacktestControlEndOfBacktest && !backtest_complete_) {
      execution_handler_.CancelAllPendingOrders();
      portfolio_manager_.CancelAllPendingOrders();
      EmitClosingOrders(current_time);
      backtest_complete_ = true;
    }
  }
}

// MARK: Emit Closing Orders
void Backtester::EmitClosingOrders(timestamp_t close_ts) {
  auto current_prices = market_state_manager_.GetTradedInstrsBbo();

  for (const auto& pos : portfolio_manager_.GetPositions()) {
    if (pos.quantity == 0) continue;

    auto it = current_prices.find(pos.instrument_id);
    if (it == current_prices.end()) continue;

    SignalType signal_type = pos.quantity > 0 ? SignalType::kSellSignal : SignalType::kBuySignal;
    int64_t close_price = pos.quantity > 0 ? it->second.bid.price : it->second.ask.price;

    if (close_price == 0 || close_price == kUndefPrice) {
      spdlog::warn("EmitClosingOrders: No valid price for instrument {}, using avg entry price",
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

// MARK: Record Snapshot
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

// MARK: Prime Sources
void Backtester::PrimeSources() {
  source_heads_.clear();
  source_heap_.clear();
  spdlog::info("Populating initial events from data sources...");
  for (const auto& dc : config_.data_configs) {
    SourceHead h;
    h.source_id = dc.data_source_id;
    h.exhausted = !data_reader_manager_.LoadNextEventFromSource(h.source_id, h.event.mbo);
    const uint16_t idx = static_cast<uint16_t>(source_heads_.size());
    source_heads_.push_back(h);
    if (!h.exhausted) source_heap_.push_back(idx);
  }
  std::make_heap(source_heap_.begin(), source_heap_.end(), SourceGreater{&source_heads_});
}

}  // namespace backtester