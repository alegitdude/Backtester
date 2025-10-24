#pragma once

#include "../core/Event.h"
#include <memory>

class StrategyManager {
 public:
    StrategyManager() {};
		void on_market_event(std::unique_ptr<Event>& event) {};
};