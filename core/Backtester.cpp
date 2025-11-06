#include "Backtester.h"
#include "spdlog/spdlog.h"
#include "Types.h"

namespace backtester {
    
int Backtester::RunLoop(AppConfig& config) {
   
    spdlog::info("Starting backtest loop...");
    long long current_time_ns = config.start_time;

    while(!event_queue_.is_empty() && 
           event_queue_.top_event().get_timestamp() <= config.end_time){     
       
        auto current_event = event_queue_.pop_top_event(); 
        long long current_time_ns = current_event->get_timestamp();
        EventType eventType = current_event->get_type();

        //  Event Dispatching 
        if(eventType < EventType::kStrategySignal){
            market_state_manager_.process_market_event(current_event);

            strategy_manager_.on_market_event(current_event, 
                market_state_manager_.get_OB_snapshot());

            // After market update, check if any pending strategy orders can be filled
            execution_handler_.check_pending_strategy_orders_for_fills(
                market_state_manager.get_OB_snapshot());
        }            

        if(eventType >= EventType::kStrategySignal && 
            eventType <= EventType::kStrategyOrderModify){
            // Strategy has generated a signal, convert to an OrderEvent
            order_event = strategy_manager.convert_signal_to_order(current_event)
            IF order_event IS NOT NULL THEN
                execution_handler.place_order(order_event) // Execution handler will push FillEvent to queue
            END IF
        } 
      
        if(eventType >= EventType::kStrategyOrderFill){
            // Your strategy's order got filled
            portfolio_manager.process_fill(current_event)
            strategy_manager.on_fill_event(current_event) 

            // Log the trade for reporting
            report_generator.record_fill(current_event, portfolio_manager.get_current_pnl())

        }
       
        if(eventType >= EventType::kBacktestControlEndOfBacktest){
            break;  
        }
           
          // load next event from the source that provided the just-processed event
        data_reader_manager_.try_load_next_event_for_source(event_queue, 
            current_event.source_symbol_id)
    }
      

    spdlog::info("Backtest loop finished.");

    ///  Final Reporting & Cleanup 
    report_generator.generate_daily_report(current_time_ns); 
    portfolio_manager.get_current_pnl();  

    spdlog::info("Backtester shutting down.");

    return 0; 
}



////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

}