#pragma once
#include <cstdint>
#include <limits>

namespace backtester {

static constexpr auto kUndefPrice = std::numeric_limits<std::int64_t>::max();

struct LevelQueue {
    int64_t price{kUndefPrice};
    std::vector<MarketByOrderEvent> orders;
    uint32_t size{0};
    uint32_t count{0};

    bool IsEmpty() const { return price == kUndefPrice; }
    operator bool() const { return !IsEmpty(); }
};



}