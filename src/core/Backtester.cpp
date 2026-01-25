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

            // strategy_manager_.on_market_event(current_event, 
            //     market_state_manager_.get_OB_snapshot());

            // execution_handler_.check_pending_strategy_orders_for_fills(
            //     market_state_manager.get_OB_snapshot());
            
            data_reader_manager_.LoadNextEventFromSource(market_event->data_source);
        }            

        if(isStrategyEvent(eventType)){

             if(eventType >= EventType::kStrategySignal && 
                eventType <= EventType::kStrategyOrderModify){
                // Strategy has generated a signal, convert to an OrderEvent
                // order_event = strategy_manager.convert_signal_to_order(current_event)
                // IF order_event IS NOT NULL THEN
                //     execution_handler.place_order(order_event) // Execution handler will push FillEvent to queue
            } 
      
            if(eventType >= EventType::kStrategyOrderFill){
                // Your strategy's order got filled
                // portfolio_manager.process_fill(current_event)
                // strategy_manager.on_fill_event(current_event) 

                // // Log the trade for reporting
                // report_generator.record_fill(current_event, portfolio_manager.get_current_pnl())
            }
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