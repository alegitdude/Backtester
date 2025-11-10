#pragma once
#include "../core/Event.h"
#include <iostream>

namespace backtester {

class MarketStateManager{
 public:
	MarketStateManager() {};

    void process_market_event(const MarketByOrderEvent* event) {};
    void get_OB_snapshot() {};
};

}