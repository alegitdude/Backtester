#pragma once
#include <cstdint>
#include <limits>

static constexpr auto kUndefPrice = std::numeric_limits<std::int64_t>::max();

struct LevelQueue {
    int64_t price{kUndefPrice};
    std::vector<MarketByOrderEvent> orders;
    uint32_t size{0};
    uint32_t count{0};

    bool IsEmpty() const { return price == kUndefPrice; }
    operator bool() const { return !IsEmpty(); }
};

struct PriceLevel {
    int64_t price;
    uint32_t size;
    uint32_t count;
};

struct BidAskPair {
    std::int64_t bid_px;
    std::int64_t ask_px;
    std::uint32_t bid_sz;
    std::uint32_t ask_sz;
    std::uint32_t bid_ct;
    std::uint32_t ask_ct;
};