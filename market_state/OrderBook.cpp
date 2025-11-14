#include <iostream>
#include <string>
#include <algorithm>
#include "OrderBook.h"
#include "../core/Event.h"

namespace backtester {
/*  LevelOrders = std::vector<MarketByOrderEvent>; vector of orders
    SideLevels = std::map<int64_t, LevelOrders>; price , all the orders
    PriceLevel = price, size, count

    SideLevels offers_; all the offers prices and orders
    SideLevels bids_; all the bids prices and orders 
    */
///////////////////////////////////////////////////////////////////
/////////////////////////// Getters ///////////////////////////////
///////////////////////////////////////////////////////////////////
// MARK: Getters

PriceLevel OrderBook::GetBidLevel(std::size_t idx = 0) const {
    if (bids_.size() > idx) {
        // Reverse iterator to get highest bid prices first
        auto level_it = bids_.rbegin();
        std::advance(level_it, idx);
        return GetPriceLevel(level_it->first, level_it->second);
    }
    return PriceLevel{};
}

PriceLevel OrderBook::GetAskLevel(std::size_t idx = 0) const {
    if (offers_.size() > idx) {
        auto level_it = offers_.begin();
        std::advance(level_it, idx);
        return GetPriceLevel(level_it->first, level_it->second);
    }
    return PriceLevel{};
}

PriceLevel OrderBook::GetBidLevelByPx(int64_t px) const {
    auto level_it = bids_.find(px);
    if (level_it == bids_.end()) {
        std::string str = std::to_string(px);
        throw std::invalid_argument{"No bid level at " + str};
    }
    return GetPriceLevel(px, level_it->second);
}

PriceLevel OrderBook::GetAskLevelByPx(int64_t price) const {
    auto level_it = offers_.find(price);
    if (level_it == offers_.end()) {
        throw std::invalid_argument{"No ask level at " + std::to_string(price)};
    }
    return GetPriceLevel(price, level_it->second);
}

const MarketByOrderEvent& OrderBook::GetOrder(uint64_t order_id) {
    auto order_it = orders_by_id_.find(order_id);
    if (order_it == orders_by_id_.end()) {
        throw std::invalid_argument{"No order with ID " +
                                    std::to_string(order_id)};
    }
    auto& level = GetLevel(order_it->second.side, order_it->second.price);
    return *GetLevelOrder(level, order_id);
}

uint32_t OrderBook::GetQueuePos(uint64_t order_id) {
    auto order_it = orders_by_id_.find(order_id);
    if (order_it == orders_by_id_.end()) {
        throw std::invalid_argument{"No order with ID " +
                                    std::to_string(order_id)};
    }
    const auto& level_it = GetLevel(order_it->second.side, order_it->second.price);
    uint32_t prior_size = 0;
    for (const auto& order : level_it) {
        if (order.order_id == order_id) {
        break;
        }
        prior_size += order.size;
    }
    return prior_size;
}
//// TODO THIS SHOULD BE WHOLE BOOK 
std::vector<BidAskPair> OrderBook::GetSnapshot(std::size_t level_count = 1) const {
    std::vector<BidAskPair> res;
    for (size_t i = 0; i < level_count; ++i) {
        BidAskPair ba_pair{kUndefPrice, kUndefPrice, 0, 0, 0, 0};
        auto bid = GetBidLevel(i);
        if (bid) {
        ba_pair.bid_px = bid.price;
        ba_pair.bid_sz = bid.size;
        ba_pair.bid_ct = bid.count;
        }
        auto ask = GetAskLevel(i);
        if (ask) {
        ba_pair.ask_px = ask.price;
        ba_pair.ask_sz = ask.size;
        ba_pair.ask_ct = ask.count;
        }
        res.emplace_back(ba_pair);
    }
    return res;
}

// MARK: Apply

void OrderBook::Apply(const MarketByOrderEvent& mbo) {
    switch (mbo.type) {
        case EventType::kMarketOrderClear: {
            Clear();
            break;
        }
        case EventType::kMarketOrderAdd: {
            Add(mbo);
            break;
        }
        case EventType::kMarketOrderCancel: {
            Cancel(mbo);
            break;
        }
        case EventType::kMarketOrderModify: {
            Modify(mbo);
            break;
        }
        case EventType::kMarketTrade:
        case EventType::kMarketFill:
        case EventType::kMarketNone: {
            break;
        }
        default: {
        throw std::invalid_argument{std::string{"Unknown action: "} +
                                    std::to_string(mbo.type)};
        }
    }
}
/////////// Private

//using Orders = std::unordered_map<uint64_t, PriceAndSide>;

PriceLevel OrderBook::GetPriceLevel(int64_t price, const LevelOrders level) {
  PriceLevel res{price};
  // only using pure mbo messages so should not encounter TOB flag
  for (const auto& order : level) {
    //if (!IsTOB(order.flags)) {
    ++res.count;
    //}
    res.size += order.size;
  }
  return res;
}

using LevelOrders = std::vector<MarketByOrderEvent>;

LevelOrders::iterator OrderBook::GetLevelOrder(LevelOrders& level,
                                            uint64_t order_id) {
  auto order_it = std::find_if(level.begin(), level.end(),
                                [order_id](const MarketByOrderEvent& order) {
                                  return order.order_id == order_id;
                                });
  if (order_it == level.end()) {
    throw std::invalid_argument{"No order with ID " + std::to_string(order_id)};
  }
  return order_it;
}

LevelOrders& OrderBook::GetLevel(OrderSide side, int64_t price) {
  SideLevels& levels = GetSideLevels(side);
  auto level_it = levels.find(price);
  if (level_it == levels.end()) {
    throw std::invalid_argument{
        std::string{"Received event for unknown level "} +
        std::to_string(side) + " " + std::to_string(price)};
  }
  return level_it->second;
}

  ///////////////////////////////////////////////////////////////////
  //////////////////// OrderBook Operations /////////////////////////
  ///////////////////////////////////////////////////////////////////
void OrderBook::Clear() {
  orders_by_id_.clear();
  offers_.clear();
  bids_.clear();
}

void OrderBook::Add(MarketByOrderEvent mbo) { 
//Not using normalized/aggregate sets so should not encounter TOB flags
// if (mbo.flags.IsTob()) {
//   SideLevels& levels = GetSideLevels(mbo.side);
//   levels.clear();
//   // kUndefPrice indicates the side's book should be cleared
//   // and doesn't represent an order that should be added
//   if (mbo.price != kUndefPrice) {
//     LevelOrders level = {mbo};
//     levels.emplace(mbo.price, level);
//   }   
    LevelOrders& level = GetOrInsertLevel(mbo.side, mbo.price);
    level.emplace_back(mbo);
    auto res = orders_by_id_.emplace(mbo.order_id,
                                    PriceAndSide{mbo.price, mbo.side});
    if (!res.second) {
    throw std::invalid_argument{"Received duplicated order ID " +
                                std::to_string(mbo.order_id)};
    }
}
  

  void OrderBook::Cancel(MarketByOrderEvent mbo) {
    LevelOrders& level = GetLevel(mbo.side, mbo.price);
    auto order_it = GetLevelOrder(level, mbo.order_id);
    if (order_it->size < mbo.size) {
      throw std::logic_error{
          "Tried to cancel more size than existed for order ID " +
          std::to_string(mbo.order_id)};
    }
    order_it->size -= mbo.size;
    if (order_it->size == 0) {
      orders_by_id_.erase(mbo.order_id);
      level.erase(order_it);
      if (level.empty()) {
        RemoveLevel(mbo.side, mbo.price);
      }
    }
  }

  void OrderBook::Modify(MarketByOrderEvent mbo) {
    auto price_side_it = orders_by_id_.find(mbo.order_id);
    if (price_side_it == orders_by_id_.end()) {
      // If order not found, treat it as an add
      Add(mbo);
      return;
    }
    if (price_side_it->second.side != mbo.side) {
      throw std::logic_error{"Order " + std::to_string(mbo.order_id) +
                             " changed side"};
    }
    auto prev_price = price_side_it->second.price;
    LevelOrders& prev_level = GetLevel(mbo.side, prev_price);
    auto level_order_it = GetLevelOrder(prev_level, mbo.order_id);
    if (prev_price != mbo.price) {
      price_side_it->second.price = mbo.price;
      prev_level.erase(level_order_it);
      if (prev_level.empty()) {
        RemoveLevel(mbo.side, prev_price);
      }
      LevelOrders& level = GetOrInsertLevel(mbo.side, mbo.price);
      // Changing price loses priority
      level.emplace_back(mbo);
    } else if (level_order_it->size < mbo.size) {
      LevelOrders& level = prev_level;
      // Increasing size loses priority
      level.erase(level_order_it);
      level.emplace_back(mbo);
    } else {
      level_order_it->size = mbo.size;
    }
  }

};

