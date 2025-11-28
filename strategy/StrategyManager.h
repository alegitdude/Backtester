#pragma once

#include "../core/Event.h"
#include <memory>

namespace backtester {

class StrategyManager {
 public:
    StrategyManager() {};
		void OnMarketEvent(std::unique_ptr<Event>& event) {};
};

}