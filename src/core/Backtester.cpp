#include "../../include/core/Backtester.h"
#include "spdlog/spdlog.h"
#include "../../include/core/Types.h"

namespace backtester {

    int Backtester::RunLoop(const AppConfig& config) {

        spdlog::info("Starting backtest loop...");
        u_int64_t current_time;
        uint64_t last_snapshot_ts_ = 0;
        u_int64_t event_tally = 0;
        while (!event_queue_.IsEmpty() &&
            event_queue_.ReadTopEvent().timestamp <= config.end_time) {

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

                data_reader_manager_.LoadNextEventFromSource(market_event->data_source);

                if (event_queue_.IsEmpty() ||
                    event_queue_.ReadTopEvent().timestamp > config.end_time) {
                    event_queue_.PushEvent(std::make_unique<Event>(current_time,
                        EventType::kBacktestControlEndOfBacktest));
                }
                if (current_time - last_snapshot_ts_ >= config.snapshot_interval_ns) {
                    RecordSnapshot(current_time);
                    last_snapshot_ts_ = current_time;
                }
                continue;
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

                if (current_time - last_snapshot_ts_ >= config.snapshot_interval_ns) {
                    RecordSnapshot(current_time);
                    last_snapshot_ts_ = current_time;
                }
                continue;
            }

            if (isStrategyOrderEvent(eventType)) {
                const StrategyOrderEvent* order_event =
                    static_cast<const StrategyOrderEvent*>(current_event.get());
                const BidAskPair cur_bbo = market_state_manager_.GetInstrumentBbo(
                    order_event->instrument_id);
                int64_t queue_depth = market_state_manager_.GetQueueDepth(
                    order_event->instrument_id, order_event->price);

                execution_handler_.OnStrategyOrder(*order_event, cur_bbo, queue_depth); // Execution handler will push FillEvent to queue

                if (current_time - last_snapshot_ts_ >= config.snapshot_interval_ns) {
                    RecordSnapshot(current_time);
                    last_snapshot_ts_ = current_time;
                }
                continue;
            }

            if (eventType == EventType::kStrategyOrderFill) {
                const StrategyFillEvent* fill_event =
                    static_cast<const StrategyFillEvent*>(current_event.get());

                portfolio_manager_.ProcessFill(*fill_event);
                strategy_manager_.OnFillEvent(*fill_event);

                if (current_time - last_snapshot_ts_ >= config.snapshot_interval_ns) {
                    RecordSnapshot(current_time);
                    last_snapshot_ts_ = current_time;
                }
                continue;
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
                    // Close all open positions
                    CloseAllPositions(current_time);
                    // Notify strategies
                        // strategy_manager_.OnEndOfDay(current_time);
                    // Take final snapshot
                    RecordSnapshot(current_time);
                    break; // Exit the loop cleanly
                }

                if (current_time - last_snapshot_ts_ >= config.snapshot_interval_ns) {
                    RecordSnapshot(current_time);
                    last_snapshot_ts_ = current_time;
                }
                continue;
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

    void Backtester::CloseAllPositions(uint64_t close_ts) {
        auto current_prices = market_state_manager_.GetTradedInstrsBbo();

        for (const auto& pos : portfolio_manager_.GetPositions()) {
            if (pos.quantity == 0) continue;

            auto it = current_prices.find(pos.instrument_id);
            if (it == current_prices.end()) continue;

            OrderSide close_side = pos.quantity > 0 ? OrderSide::kAsk : OrderSide::kBid;
            int64_t close_price = pos.quantity > 0 ?
                it->second.bid.price : it->second.ask.price;

            if (close_price == 0 || close_price == kUndefPrice) {
                spdlog::warn("CloseAllPositions: No valid price for instrument {}, "
                    "using avg entry price", pos.instrument_id);
                close_price = pos.avg_entry_price;
            }

            StrategyFillEvent close_fill(
                close_ts,
                pos.last_order_id,
                pos.instrument_id,
                close_side,
                close_price,
                static_cast<uint32_t>(std::abs(pos.quantity)),
                pos.strategy_id
            );

            portfolio_manager_.ProcessFill(close_fill);
        }
    }

    void Backtester::RecordSnapshot(u_int64_t current_time) {
        auto current_prices = market_state_manager_.GetTradedInstrsBbo();
        int64_t equity = portfolio_manager_.GetTotalEquity(current_prices);
        int64_t cash = portfolio_manager_.GetCash();
        int64_t realized = portfolio_manager_.GetRealizedPnL();
        int64_t unrealized = equity - cash;
        int64_t drawdown = portfolio_manager_.GetMaxEquitySeen() - equity;

        report_generator_.RecordEquitySnapshot(current_time, equity, cash,
            realized, unrealized, drawdown);
    }

}