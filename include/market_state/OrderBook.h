#pragma once
#include "../core/Event.h"
#include "../core/Types.h"
#include <cstdint>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "OBTypes.h"

namespace backtester {
    struct BidPriceLess {  
        bool operator()(const std::pair<int64_t, LevelQueue>& p, int64_t price) const {
            return p.first <= price;
        }
    };
    struct AskPriceGreater {  
        bool operator()(const std::pair<int64_t, LevelQueue>& p, int64_t price) const {
            return p.first >= price;
        }
    };

    class OrderBook {
    public:
        inline const BidAskPair GetBbo() { return bbo_cache_; }
        int64_t GetMidPrice() const;

        PriceLevel GetBidLevel(std::size_t idx = 0) const;
        PriceLevel GetAskLevel(std::size_t idx = 0) const;
        PriceLevel GetLevelByPx(OrderSide side, int64_t price) const;
        //uint32_t GetQueuePos(uint64_t order_id); TODO

        const std::vector<BidAskPair> GetSnapshot(std::size_t level_count = 1) const;
        void OnEvent(const MarketByOrderEvent& mbo) { Apply(mbo); };
        void Apply(const MarketByOrderEvent& mbo);

    private:
        [[no_unique_address]] BidPriceLess    bids_compare_;
        [[no_unique_address]] AskPriceGreater asks_compare_;

        BidAskPair bbo_cache_;
        using SideLevels = std::vector<std::pair<int64_t, LevelQueue>>;

        SideLevels offers_;
        SideLevels bids_;

        struct PriceSideSize {
            int64_t price;
            OrderSide side;
            uint32_t size;
        };
        using Orders = std::unordered_map<uint64_t, PriceSideSize>;

        Orders orders_by_id_;

        const uint8_t F_TOB = 64; // The numerical value for F_TOB
        inline bool IsTOB(uint8_t flags_value) {
            return (flags_value & F_TOB) != 0;
        }

        /////////////////////////////////////////////////////////
        /////////////////// Methods /////////////////////////////
        /////////////////////////////////////////////////////////

        template <class Levels, class Compare>
        inline auto GetLevelIt(Levels& levels, int64_t price, Compare comp) const {
            return std::find_if(levels.rbegin(), levels.rend(),
                [price, comp](const auto& p) {
                    return comp(p, price);
                });
        }

        template <class Levels, class Compare>
        inline LevelQueue& GetOrInsertLevel(Levels& levels, int64_t price, Compare comp) {

            // auto it = std::find_if(levels.begin(), levels.end(), [comp, price](std::pair<int64_t, LevelQueue>& p) {
            //     return comp(p, price);
            //     });
            auto rit = std::find_if(levels.rbegin(), levels.rend(),
                [price, comp](const auto& p) {
                    return comp(p, price);
                });

            if (rit != levels.rend() && rit->first == price) /*[[__builtin_expect]] */ {
                return rit->second;
            }
            else {
                return levels.insert(rit.base(), { price, LevelQueue{} })->second;
            }
        }

        void UpdateBboCache();

        static std::vector<MarketByOrderEvent>::iterator GetLevelOrder(
            std::vector<MarketByOrderEvent>& level,
            uint64_t order_id);

        ///////////////////////////////////////////////////////////////////
        //////////////////// OrderBook Operations /////////////////////////
        ///////////////////////////////////////////////////////////////////

        void Clear();

        void Add(const MarketByOrderEvent& mbo);

        template <class Compare>
        void Add(SideLevels& level, Compare comp, const MarketByOrderEvent& mbo);

        void Cancel(const MarketByOrderEvent& mbo);

        template <class Compare>
        void Cancel(SideLevels& levels, Compare comp, const MarketByOrderEvent& mbo);

        void Modify(const MarketByOrderEvent& mbo);

        template <class Compare>
        void Modify(SideLevels& levels, Compare comp, const MarketByOrderEvent& mbo, OrderBook::Orders::iterator prev_price);

    };

}
