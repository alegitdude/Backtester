#include <iostream>
#include <string>
#include "OrderBook.h"
#include <databento/record.hpp>
#include <databento/dbn.hpp>
#include <databento/enums.hpp>
#include <databento/pretty.hpp> 

using namespace databento;

class OrderBook
{
public:
    std::pair<PriceLevel, PriceLevel> Bbo() const
    {
        return {GetBidLevel(), GetAskLevel()};
    }

    PriceLevel GetBidLevel(std::size_t idx = 0) const
    {
        if (bids_.size() > idx)
        {
            // Reverse iterator to get highest bid prices first
            auto level_it = bids_.rbegin();
            std::advance(level_it, idx);
            return GetPriceLevel(level_it->first, level_it->second);
        }
        return PriceLevel{};
    }

    PriceLevel GetAskLevel(std::size_t idx = 0) const
    {
        if (offers_.size() > idx)
        {
            auto level_it = offers_.begin();
            std::advance(level_it, idx);
            return GetPriceLevel(level_it->first, level_it->second);
        }
        return PriceLevel{};
    }

    void Apply(const MboMsg &mbo)
    {
        switch (mbo.action)
        {
        case Action::Clear:
        {
            Clear();
            break;
        }
        case Action::Add:
        {
            Add(mbo);
            break;
        }
        case Action::Cancel:
        {
            Cancel(mbo);
            break;
        }
        case Action::Modify:
        {
            Modify(mbo);
            break;
        }
        case Action::Trade:
        case Action::Fill:
        case Action::None:
        {
            break;
        }
        default:
        {
            throw std::invalid_argument{std::string{"Unknown action: "} +
                                        ToString(mbo.action)};
        }
        }
    }

private:
    /// Level orders -> SideLevels -> bids or offers: map of price with vector or mbo msgs
    using LevelOrders = std::vector<MboMsg>;
    using SideLevels = std::map<int64_t, LevelOrders>;

    SideLevels offers_;
    SideLevels bids_;

    struct PriceAndSide
    {
        int64_t price;
        Side side;
    };
    using Orders = std::unordered_map<uint64_t, PriceAndSide>;

    Orders orders_by_id_;

    /////////////////////////////////////////////////////////
    /////////////////// Methods /////////////////////////
    /////////////////////////////////////////////////////////

    static PriceLevel GetPriceLevel(int64_t price, const LevelOrders level)
    {
        PriceLevel res{price};
        for (const auto &order : level)
        {
            if (!order.flags.IsTob())
            {
                ++res.count;
            }
            res.size += order.size;
        }
        return res;
    }

    LevelOrders &GetOrInsertLevel(Side side, int64_t price)
    {
        SideLevels &levels = GetSideLevels(side);
        /// auto creates key if doesn't exist
        return levels[price];
    }

    SideLevels &GetSideLevels(Side side)
    {
        switch (side)
        {
        case Side::Ask:
        {
            return offers_;
        }
        case Side::Bid:
        {
            return bids_;
        }
        case Side::None:
        default:
        {
            throw std::invalid_argument{"Invalid side"};
        }
        }
    }

    LevelOrders &GetLevel(Side side, int64_t price)
    {
        SideLevels &levels = GetSideLevels(side);
        auto level_it = levels.find(price);
        if (level_it == levels.end())
        {
            throw std::invalid_argument{
                std::string{"Received event for unknown level "} + ToString(side) +
                " " + databento::pretty::PxToString(price)};
        }
        return level_it->second;
    }

    static LevelOrders::iterator GetLevelOrder(LevelOrders &level,
                                               uint64_t order_id)
    {
        auto order_it = std::find_if(
            level.begin(), level.end(),
            [order_id](const MboMsg &order)
            { return order.order_id == order_id; });
        if (order_it == level.end())
        {
            throw std::invalid_argument{"No order with ID " +
                                        std::to_string(order_id)};
        }
        return order_it;
    }

    void RemoveLevel(Side side, int64_t price)
    {
        SideLevels &levels = GetSideLevels(side);
        levels.erase(price);
    }

    ///////////////////////////////////////////////////////////////////
    //////////////////// OrderBook Operations /////////////////////////
    ///////////////////////////////////////////////////////////////////

    void Clear()
    {
        orders_by_id_.clear();
        offers_.clear();
        bids_.clear();
    }

    void Add(MboMsg mbo)
    {
        /////////////////// For use with Top-of-book publishers, likely not applicable to futures mbo data
        if (mbo.flags.IsTob())
        {
            SideLevels &levels = GetSideLevels(mbo.side);
            levels.clear();
            // kUndefPrice indicates the side's book should be cleared
            // and doesn't represent an order that should be added
            if (mbo.price != kUndefPrice)
            {
                LevelOrders level = {mbo};
                levels.emplace(mbo.price, level);
            }
        }
        else
        {
            LevelOrders &level = GetOrInsertLevel(mbo.side, mbo.price);
            level.emplace_back(mbo);
            ///// Insert order: key = orderId, value = price,side
            auto res = orders_by_id_.emplace(mbo.order_id,
                                             PriceAndSide{mbo.price, mbo.side});
            if (!res.second)
            {
                throw std::invalid_argument{"Received duplicated order ID " +
                                            std::to_string(mbo.order_id)};
            }
        }
    }

    void Cancel(MboMsg mbo)
    {
        LevelOrders &level = GetLevel(mbo.side, mbo.price);
        auto order_it = GetLevelOrder(level, mbo.order_id);
        if (order_it->size < mbo.size)
        {
            throw std::logic_error{
                "Tried to cancel more size than existed for order ID " +
                std::to_string(mbo.order_id)};
        }
        order_it->size -= mbo.size;
        if (order_it->size == 0)
        {
            orders_by_id_.erase(mbo.order_id);
            level.erase(order_it);
            if (level.empty())
            {
                RemoveLevel(mbo.side, mbo.price);
            }
        }
    }

    void Modify(MboMsg mbo)
    {
        auto price_side_it = orders_by_id_.find(mbo.order_id);
        if (price_side_it == orders_by_id_.end())
        {
            // If order not found, treat it as an add
            Add(mbo);
            return;
        }
        if (price_side_it->second.side != mbo.side)
        {
            throw std::logic_error{"Order " + std::to_string(mbo.order_id) +
                                   " changed side"};
        }
        auto prev_price = price_side_it->second.price;
        LevelOrders &prev_level = GetLevel(mbo.side, prev_price);
        auto level_order_it = GetLevelOrder(prev_level, mbo.order_id);
        if (prev_price != mbo.price)
        {
            price_side_it->second.price = mbo.price;
            prev_level.erase(level_order_it);
            if (prev_level.empty())
            {
                RemoveLevel(mbo.side, prev_price);
            }
            LevelOrders &level = GetOrInsertLevel(mbo.side, mbo.price);
            // Changing price loses priority
            level.emplace_back(mbo);
        }
        else if (level_order_it->size < mbo.size)
        {
            LevelOrders &level = prev_level;
            // Increasing size loses priority
            level.erase(level_order_it);
            level.emplace_back(mbo);
        }
        else
        {
            level_order_it->size = mbo.size;
        }
    }
};