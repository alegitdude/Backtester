#include "core/Backtester.h"
#include "core/Types.h"
#include "spdlog/spdlog.h"

namespace backtester {

    int Backtester::RunLoop(const AppConfig& config) {

        spdlog::info("Populating initial events from data sources...");
        for (const DataSourceConfig& source : config.data_configs) {
            auto event_ptr = data_reader_manager_.LoadNextEventFromSource(
                source.data_source_name);
            if (event_ptr) {
                event_queue_.PushEvent(std::move(event_ptr));
            }
            else {
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
            current_time = current_event->timestamp;
            EventType eventType = current_event->type;
            event_tally++;

            if (isMarketEvent(eventType)) {
                const MarketByOrderEvent* market_event =
                    static_cast<const MarketByOrderEvent*>(current_event.get());

                market_state_manager_.OnMarketEvent(*market_event);

                if (current_time >= config.start_time) {
                    auto signals = strategy_manager_.OnMarketEvent(*market_event);

                    if (signals.size() > 0) {
                        for (size_t i = 0; i < signals.size(); i++) {
                            event_queue_.PushEvent(std::move(signals[i]));
                        }
                    }

                    execution_handler_.OnMarketEvent(*market_event);
                    if (portfolio_manager_.HasAnyOpenPosition()) {
                        portfolio_manager_.UpdateMaxEquity();
                    }
                }

                if (!backtest_complete) {
                    auto event_ptr = data_reader_manager_.LoadNextEventFromSource(
                        market_event->data_source);
                    if (event_ptr) {
                        event_queue_.PushEvent(std::move(event_ptr));
                    }
                }
            }

            if (isStrategySignalEvent(eventType)) {
                const StrategySignalEvent* signal_event =
                    static_cast<const StrategySignalEvent*>(current_event.get());

                auto order_event = portfolio_manager_.RequestOrder(signal_event);

                if (order_event) {
                    event_queue_.PushEvent(std::move(order_event));
                    spdlog::debug("Queued order from signal at ts={}", current_time);
                }
            }

            if (isStrategyOrderEvent(eventType)) {
                const StrategyOrderEvent* order_event =
                    static_cast<const StrategyOrderEvent*>(current_event.get());

                execution_handler_.OnStrategyOrder(*order_event);

            }

            if (eventType == EventType::kStrategyOrderFill) {
                const StrategyFillEvent* fill_event =
                    static_cast<const StrategyFillEvent*>(current_event.get());

                portfolio_manager_.ProcessFill(*fill_event);
                strategy_manager_.OnFillEvent(*fill_event);
            }

            if (eventType == EventType::kStrategyOrderRejection) {
                const StrategyOrderRejectionEvent* rejection =
                    static_cast<const StrategyOrderRejectionEvent*>(current_event.get());
                strategy_manager_.OnRejectionEvent(*rejection);
            }

            if (isControlEvent(eventType)) {
                if (eventType == EventType::kBacktestControlEndOfBacktest
                    && !backtest_complete) {
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

            if (((event_queue_.IsEmpty() || current_time > config.end_time) &&
                !backtest_complete) && !end_of_bt_pushed) {
                event_queue_.PushEvent(std::make_unique<Event>(current_time,
                    EventType::kBacktestControlEndOfBacktest));
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
        spdlog::info("Loop: {} events  {:.3f}s  {:.2f} M evt/s",
            event_tally, secs, event_tally / secs / 1e6);

        spdlog::info("Event tally: {}", event_tally);
        spdlog::info("Backtest loop finished.");
        spdlog::info("Starting report generator.");
        report_generator_.GenerateReport(portfolio_manager_);
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

            SignalType signal_type = pos.quantity > 0 ?
                SignalType::kSellSignal : SignalType::kBuySignal;
            int64_t close_price = pos.quantity > 0 ?
                it->second.bid.price : it->second.ask.price;

            if (close_price == 0 || close_price == kUndefPrice) {
                spdlog::warn("EmitClosingOrders: No valid price for instrument {}, "
                    "using avg entry price", pos.instrument_id);
                close_price = pos.avg_entry_price;
            }

            auto signal = std::make_unique<StrategySignalEvent>(
                close_ts,
                -1,  // use negative IDs to distinguish from strategy signals
                pos.strategy_id,
                pos.instrument_id,
                signal_type,
                close_price,
                static_cast<uint32_t>(std::abs(pos.quantity))
            );

            event_queue_.PushEvent(std::move(signal));
            spdlog::info("EmitClosingOrders: strategy={} instrument={} qty={} price={}",
                pos.strategy_id, pos.instrument_id, pos.quantity, close_price);
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

        report_generator_.RecordEquitySnapshot(current_time, equity, cash,
            realized, unrealized, drawdown, has_position);
    }

}