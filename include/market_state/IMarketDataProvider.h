#pragma once
#include <span>
#include <unordered_map>

#include "../core/Types.h"

namespace backtester {
class IMarketDataProvider {
 public:
  virtual ~IMarketDataProvider() = default;

  virtual const std::vector<BidAskPair> GetOBSnapshotByPub(uint32_t instrument_id,
                                                           uint16_t publisher_id,
                                                           std::size_t level_count) const = 0;

  virtual int64_t GetQueueDepth(uint32_t instr_id, OrderSide side, int64_t price) const = 0;
  virtual void GetAggOBBidsSnapshot(uint32_t instrument_id, std::span<PriceLevel> levels) const = 0;
  virtual void GetAggOBAsksSnapshot(uint32_t instrument_id, std::span<PriceLevel> levels) const = 0;
  virtual const std::unordered_map<uint32_t, const MarketSnapshot*>& GetMarketSnapshots() const = 0;
  virtual const MarketSnapshot* GetSnapshotByInstr(uint32_t instr_id) const = 0;
};
}  // namespace backtester
