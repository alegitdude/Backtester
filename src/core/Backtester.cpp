#include "../../include/core/Backtester.h"
#include "spdlog/spdlog.h"
#include "../../include/core/Types.h"

namespace backtester {

int Backtester::RunLoop(const AppConfig& config) {
   
    spdlog::info("Starting backtest loop...");
    u_int64_t current_time = config.start_time;

    while(!event_queue_.IsEmpty() && 
           event_queue_.ReadTopEvent().timestamp <= config.end_time){     
       
        auto current_event = event_queue_.PopTopEvent(); 
        uint64_t current_time = current_event->timestamp;
        EventType eventType = current_event->type;

        if (isMarketEvent(eventType)) {
            const MarketByOrderEvent* market_event = 
            static_cast<const MarketByOrderEvent*>(current_event.get());

            market_state_manager_.OnMarketEvent(*market_event);

            auto signals = strategy_manager_.OnMarketEvent(*market_event);

            if(signals.size() > 0){
                for(int i = 0; i < signals.size(); i++){
                    event_queue_.PushEvent(std::move(signals[i]));
                }
            }

            BidAskPair bbo = market_state_manager_.GetInstrumentBbo(market_event->instrument_id);
            execution_handler_.OnMarketEvent(*market_event, bbo);
            
            data_reader_manager_.LoadNextEventFromSource(market_event->data_source);
            continue;
        }            

        if(isStrategySignalEvent(eventType)){
            const StrategySignalEvent* signal_event = 
                static_cast<const StrategySignalEvent*>(current_event.get());

            auto current_prices = market_state_manager_.GetTradedInstrsBbo();
            auto order_event = portfolio_manager_.RequestOrder(signal_event, 
                current_prices);

            if(order_event){
                event_queue_.PushEvent(std::move(order_event)); 
                spdlog::debug("Queued order from signal at ts={}", current_time);
            } else {
                spdlog::warn("Signal rejected by portfolio at ts={}", current_time);
            }    
            continue;      
        }
        
        if(isStrategyOrderEvent(eventType)){
            const StrategyOrderEvent* order_event = 
                static_cast<const StrategyOrderEvent*>(current_event.get());
            const BidAskPair cur_bbo = market_state_manager_.GetInstrumentBbo(
                order_event->instrument_id);
            int64_t queue_depth = market_state_manager_.GetQueueDepth(
                order_event->instrument_id, order_event->price);

            execution_handler_.OnStrategyOrder(*order_event, cur_bbo, queue_depth); // Execution handler will push FillEvent to queue
            continue;
        } 
    
        if(eventType == EventType::kStrategyOrderFill){
            const StrategyFillEvent* fill_event = 
                 static_cast<const StrategyFillEvent*>(current_event.get());

            portfolio_manager_.ProcessFill(*fill_event);
            strategy_manager_.OnFillEvent(*fill_event); 

            continue;
        }

        if (isControlEvent(eventType)) {
            continue;  
        }        
        
    }
      

    spdlog::info("Backtest loop finished.");

    ///  Final Reporting & Cleanup 
    // report_generator.generate_daily_report(current_time_ns); 
    // portfolio_manager.get_current_pnl();  

    spdlog::info("Backtester shutting down.");

    return 0; 
}

}