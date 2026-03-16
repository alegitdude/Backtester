#pragma once
#include "../core/Types.h"
#include <vector>
#include <unordered_map>

namespace backtester {
class IMarketDataProvider {
 public:
    virtual ~IMarketDataProvider() = default;
    virtual const std::vector<BidAskPair> GetOBSnapshot(
        uint32_t instrument_id, uint16_t publisher_id, 
        std::size_t level_count) const = 0;
    virtual const std::unordered_map<uint32_t, MarketSnapshot>& GetMarketSnapshots() const = 0;
};
}

