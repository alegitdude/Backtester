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
#include <array>
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

    template<size_t Capacity = 65536>  // Power of 2 
    class OrderTable {
    public:
        struct Order {
            uint64_t order_id = 0;  // 0 = empty slot
            int64_t price = 0;
            OrderSide side = kNone;
            uint32_t size = 0;
        };

        static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be power of 2");
        static constexpr uint64_t MASK = Capacity - 1;

        bool Insert(uint64_t order_id, int64_t price, OrderSide side, uint32_t size) {
            uint64_t slot = order_id & MASK;
            // Linear probe
            for (size_t i = 0; i < Capacity; ++i) {
                Order& e = entries_[(slot + i) & MASK];
                if (e.order_id == 0) {  // empty slot
                    e = { order_id, price, side, size };
                    return true;
                }
                if (e.order_id == order_id) return false;  // duplicate
            }
            return false;  // full — should never happen with right capacity
        }

        Order* Find(uint64_t order_id) {
            uint64_t slot = order_id & MASK;
            for (size_t i = 0; i < Capacity; ++i) {
                Order& e = entries_[(slot + i) & MASK];
                if (e.order_id == order_id) return &e;
                if (e.order_id == 0) return nullptr;  // empty slot means not found
            }
            return nullptr;
        }

        bool Erase(uint64_t order_id) {
            uint64_t slot = order_id & MASK;
            for (size_t i = 0; i < Capacity; ++i) {
                Order& e = entries_[(slot + i) & MASK];
                if (e.order_id == order_id) {
                    e.order_id = 0; 
                    Backshift((slot + i) & MASK);
                    return true;
                }
                if (e.order_id == 0) return false;
            }
            return false;
        }

        void Clear() {
            entries_.fill({});
        }

    private:
        std::array<Order, Capacity> entries_{};

        void Backshift(size_t hole) {
            size_t current = hole;

            while (true) {
                size_t next = (current + 1) & MASK;
                Order& e = entries_[next];

                if (e.order_id == 0) break;

                uint64_t natural = e.order_id & MASK;

                if (((next - natural) & MASK) >= ((hole - natural) & MASK)) {

                    entries_[hole] = e;
                    e.order_id = 0;

                    hole = next;
                }
                current = next;
            }
        }
    };

    class OrderBook {
    public:
        OrderBook(uint16_t publisher_id);
        uint16_t publisher_id;
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

        OrderTable<65536> orders_by_id_;
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

            auto rit = std::find_if(levels.rbegin(), levels.rend(),
                [price, comp](const auto& p) {
                    return comp(p, price);
                });

            if (LIKELY(rit != levels.rend() && rit->first == price)) {
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
        void Modify(SideLevels& levels, Compare comp, const MarketByOrderEvent& mbo, backtester::OrderTable<>::Order* prev_price);

    };

}
