#include "EventQueue.h"
#include "../data_ingestion/DataReaderManager.h"
#include "../market_state/MarketStateManager.h"
#include "../portfolio/PortfolioManager.h"
#include "../reporting/ReportGenerator.h"
#include "../execution/ExecutionHandler.h"
#include "../strategy/StrategyManager.h"

class Backtester {
 public:
		Backtester(EventQueue& eq, DataReaderManager& drm, MarketStateManager& msm,
							 PortfolioManager& pm, ReportGenerator& rg, ExecutionHandler& eh,
							 StrategyManager& sm
							 ) : event_queue_(eq), data_reader_manager_(drm), 
							 market_state_manager_(msm),portfolio_manager_(pm),
							 report_generator_(rg), execution_handler_(eh), 
							 strategy_manager_(sm) {}

    int RunLoop();

 private:
    EventQueue& event_queue_;
		DataReaderManager& data_reader_manager_;
		MarketStateManager& market_state_manager_; 
		PortfolioManager& portfolio_manager_;
		ReportGenerator& report_generator_;
		ExecutionHandler& execution_handler_;
		StrategyManager& strategy_manager_;
	};