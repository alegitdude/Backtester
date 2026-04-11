#include "../../include/core/Backtester.h"
#include "spdlog/spdlog.h"
#include "../../include/core/Types.h"

namespace backtester {

    int Backtester::RunLoop(const AppConfig& config) {
        
        spdlog::info("Populating initial events from data sources...");
        for (const DataSourceConfig& source : config.data_configs) {
            auto event_ptr = data_reader_manager_.LoadNextEventFromSource(
                source.data_source_name);
            if(event_ptr){
                event_queue_.PushEvent(std::move(event_ptr));
            } else {
                spdlog::warn("Symbol "+ source.data_source_name + " has no events.");
            }   
        }

        spdlog::info("Starting backtest loop...");
        u_int64_t current_time;
        uint64_t last_snapshot_ts_ = 0;
        u_int64_t event_tally = 0;
        bool backtest_complete = false;

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
                        for (int i = 0; i < signals.size(); i++) {
                            event_queue_.PushEvent(std::move(signals[i]));
                        }
                    }

                    BidAskPair bbo = market_state_manager_.GetInstrumentBbo(market_event->instrument_id);
                    execution_handler_.OnMarketEvent(*market_event, bbo);
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

                auto current_prices = market_state_manager_.GetTradedInstrsBbo();
                auto order_event = portfolio_manager_.RequestOrder(signal_event,
                    current_prices);

                if (order_event) {
                    event_queue_.PushEvent(std::move(order_event));
                    spdlog::debug("Queued order from signal at ts={}", current_time);
                }
                else {
                    spdlog::warn("Signal rejected by portfolio at ts={}", current_time);
                }
            }

            if (isStrategyOrderEvent(eventType)) {
                const StrategyOrderEvent* order_event =
                    static_cast<const StrategyOrderEvent*>(current_event.get());
                const BidAskPair cur_bbo = market_state_manager_.GetInstrumentBbo(
                    order_event->instrument_id);
                int64_t queue_depth = market_state_manager_.GetQueueDepth(
                    order_event->instrument_id, order_event->price);

                execution_handler_.OnStrategyOrder(*order_event, cur_bbo, queue_depth); // Execution handler will push FillEvent to queue

            }

            if (eventType == EventType::kStrategyOrderFill) {
                const StrategyFillEvent* fill_event =
                    static_cast<const StrategyFillEvent*>(current_event.get());

                portfolio_manager_.ProcessFill(*fill_event);
                strategy_manager_.OnFillEvent(*fill_event);
            }

            if (isControlEvent(eventType)) {
                // if (eventType == EventType::kBacktestControlEndOfDay) {
                //     strategy_manager_.OnEndOfDay(current_time);
                //     // Mark to market for futures — credit unrealized gains to cash
                //     MarkFuturesToMarket(current_time);
                // }

                if (eventType == EventType::kBacktestControlEndOfBacktest) {
                    // Cancel all pending orders first
                    auto cancel_fills = execution_handler_.CancelAllPendingOrders(current_time);
                    for (auto& fill : cancel_fills) {
                        portfolio_manager_.ProcessFill(*fill);
                    }
                    // Emit EOD orders
                    EmitClosingOrders(current_time);
                    // Notify strategies
                        // strategy_manager_.OnEndOfDay(current_time);
                    backtest_complete = true;
                }
            }

            if ((event_queue_.IsEmpty() || current_time > config.end_time) &&
                !backtest_complete) {
                event_queue_.PushEvent(std::make_unique<Event>(current_time,
                    EventType::kBacktestControlEndOfBacktest));
            }
            if (current_time >= config.start_time &&
                current_time - last_snapshot_ts_ >= config.snapshot_interval_ns) {
                RecordSnapshot(current_time);
                last_snapshot_ts_ = current_time;
            }
        }

        spdlog::info("Event tally: {}", event_tally);
        spdlog::info("Backtest loop finished.");
        spdlog::info("Starting report generator.");
        report_generator_.GenerateReport(portfolio_manager_);
        spdlog::info("Report finished.");

        spdlog::info("Backtester shutting down.");

        return 0;
    }

    void Backtester::EmitClosingOrders(uint64_t close_ts) {
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
                EventType::kStrategySignal,
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

    void Backtester::RecordSnapshot(u_int64_t current_time) {
        auto current_prices = market_state_manager_.GetTradedInstrsBbo();
        int64_t equity = portfolio_manager_.GetTotalEquity(current_prices);
        int64_t cash = portfolio_manager_.GetCash();
        int64_t realized = portfolio_manager_.GetRealizedPnL();
        int64_t unrealized = equity - cash;
        int64_t drawdown = portfolio_manager_.GetMaxEquitySeen() - equity;
        bool has_position = portfolio_manager_.HasAnyOpenPosition();

        report_generator_.RecordEquitySnapshot(current_time, equity, cash,
            realized, unrealized, drawdown, has_position);
    }

}