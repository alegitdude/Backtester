#include "../core/Event.h"
#include <cstdint>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iostream>
#include "OBTypes.h"

namespace backtester {

class OrderBook {
 public:
    inline std::pair<PriceLevel, PriceLevel> Bbo() const {
        return {GetBidLevel(), GetAskLevel()};
    }

    PriceLevel GetBidLevel(std::size_t idx = 0) const;
    PriceLevel GetAskLevel(std::size_t idx = 0) const;
    PriceLevel GetBidLevelByPx(int64_t price) const;
    PriceLevel GetAskLevelByPx(int64_t price) const;
    const MarketByOrderEvent& GetOrder(uint64_t order_id);
    uint32_t GetQueuePos(uint64_t order_id);

    std::vector<BidAskPair> GetSnapshot(std::size_t level_count = 1) const ;
    void OnEvent(const MarketByOrderEvent& mbo) {Apply(mbo);};
    void Apply(const MarketByOrderEvent& mbo);

private:
    ///Level orders -> SideLevels -> bids or offers: map of price with vector or mbo msgs
    //using LevelOrders = std::vector<MarketByOrderEvent>;
    using SideLevels = std::map<int64_t, LevelQueue>;

    SideLevels offers_;
    SideLevels bids_;

    struct PriceAndSide {
        int64_t price;
        OrderSide side;
    };
    using Orders = std::unordered_map<uint64_t, PriceAndSide>;

    Orders orders_by_id_;

    const uint8_t F_TOB = 64; // The numerical value for F_TOB
    inline bool IsTOB(uint8_t flags_value) {
        return (flags_value & F_TOB) != 0;
    }   

    /////////////////////////////////////////////////////////
    /////////////////// Methods /////////////////////////////
    /////////////////////////////////////////////////////////
    inline PriceLevel GetPriceLevel(const LevelQueue& level) const {
        return PriceLevel{
                level.price,        
                level.size,  
                level.count  
      };    
    }

    inline LevelQueue& GetOrInsertLevel(OrderSide side, int64_t price) {
        SideLevels &levels = GetSideLevels(side);
        /// auto creates key if doesn't exist
        return levels[price];
    }

    inline SideLevels& GetSideLevels(OrderSide side){
      switch (side) {
          case OrderSide::kAsk: {
              return offers_;
          } 
          case OrderSide::kBid: {
              return bids_;
          }
          case OrderSide::kNone:
          default: {
              throw std::invalid_argument{"Invalid side"};
          }
      }
    }

    LevelQueue& GetLevel(OrderSide side, int64_t price);

    //static PriceLevel GetPriceLevel(int64_t price, const LevelOrders& level);

    static std::vector<MarketByOrderEvent>::iterator GetLevelOrder(
        std::vector<MarketByOrderEvent>& level,
        uint64_t order_id);

    inline void RemoveLevel(OrderSide side, int64_t price) {
        SideLevels &levels = GetSideLevels(side);
        levels.erase(price);
    }

    ///////////////////////////////////////////////////////////////////
    //////////////////// OrderBook Operations /////////////////////////
    ///////////////////////////////////////////////////////////////////

    void Clear();

    void Add(MarketByOrderEvent mbo);

    void Cancel(MarketByOrderEvent mbo);

    void Modify(MarketByOrderEvent mbo); 
    
};

}
