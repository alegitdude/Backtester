#pragma once
#include <cstdint>

namespace backtester {

struct LevelQueue {
    int64_t price{kUndefPrice};
    std::vector<MarketByOrderEvent> orders;
    uint32_t size{0};
    uint32_t count{0};

    bool IsEmpty() const { return price == kUndefPrice; }
    operator bool() const { return !IsEmpty(); }
};



}