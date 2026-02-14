#include "../../include/strategy/StrategyManager.h"
#include "../../include/market_state/OBTypes.h"
#include <vector>
namespace backtester {

std::vector<std::unique_ptr<StrategySignalEvent>> StrategyManager::OnMarketEvent(
	const std::unique_ptr<Event>& event, const std::vector<BidAskPair> ob_snapshot) {
	
}



}