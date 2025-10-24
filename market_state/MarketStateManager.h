#pragma once
#include "../core/Event.h"
#include <iostream>

class MarketStateManager{
 public:
	MarketStateManager() {};

    void process_market_event(std::unique_ptr<Event>& event) {};
    void get_OB_snapshot() {};
};