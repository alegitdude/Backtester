#include "../../include/market_state/OrderBook.h"
#include "../../include/core/Event.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <string>
#include <algorithm>


namespace backtester {
  int64_t OrderBook::GetMidPrice() const {
    return ((bbo_cache_.ask.price - bbo_cache_.bid.price) / 2) + bbo_cache_.bid.price;
  }

  ///////////////////////////////////////////////////////////////////
  /////////////////////////// Getters ///////////////////////////////
  ///////////////////////////////////////////////////////////////////
  // MARK: Getters
  PriceLevel OrderBook::GetBidLevel(std::size_t idx) const {
    if (bids_.size() > idx) {
      auto it = bids_.rbegin();
      std::advance(it, idx);

      return PriceLevel{
          it->first,        // Price
          it->second.size,  // Size
          it->second.count  // Count
      };
    }
    return PriceLevel{};
  }

  PriceLevel OrderBook::GetAskLevel(std::size_t idx) const {
    if (offers_.size() > idx) {
      auto it = offers_.rbegin();
      std::advance(it, idx);

      return PriceLevel{
          it->first,        // Price
          it->second.size,  // Size
          it->second.count  // Count
      };
    }
    return PriceLevel{};
  }

  PriceLevel OrderBook::GetLevelByPx(OrderSide side, int64_t price) const {
    if (side == OrderSide::kAsk) {
      auto rit = GetLevelIt(offers_, price, asks_compare_);
      if (rit == offers_.rend() || rit->first != price) {
        return PriceLevel{};
      }
      else {
        return { rit->first, rit->second.size, rit->second.count };
      }
    }
    else {
      auto rit = GetLevelIt(bids_, price, bids_compare_);
      if (rit == bids_.rend() || rit->first != price) {
        return PriceLevel{};
      }
      else {
        return { rit->first, rit->second.size, rit->second.count };
      }
    }
  }

  // MARK: GETSNAPSHOT
  const std::vector<BidAskPair> OrderBook::GetSnapshot(std::size_t level_count) const {
    std::vector<BidAskPair> res;
    for (size_t i = 0; i < level_count; ++i) {
      BidAskPair ba_pair;
      auto bid = GetBidLevel(i);
      if (bid.price) {
        ba_pair.bid.price = bid.price;
        ba_pair.bid.size = bid.size;
        ba_pair.bid.count = bid.count;
      }
      auto ask = GetAskLevel(i);
      if (ask.price) {
        ba_pair.ask.price = ask.price;
        ba_pair.ask.size = ask.size;
        ba_pair.ask.count = ask.count;
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
      throw std::invalid_argument{ std::string{"Unknown action: "} +
                                  std::to_string(mbo.type) };
    }
    }

    if (mbo.flags & 0x80) {// F_LAST
      UpdateBboCache();
    }
  }
  /////////// Private
  void OrderBook::UpdateBboCache() {
    BidAskPair prev_bbo = bbo_cache_;
    if (!bids_.empty()) {
      PriceLevel bid_level = GetBidLevel();
      bbo_cache_.bid = { bid_level.price, bid_level.size, bid_level.count };
    }
    else {
      bbo_cache_.bid = {};
    }

    if (!offers_.empty()) {
      PriceLevel ask_level = GetAskLevel();
      bbo_cache_.ask = { ask_level.price, ask_level.size, ask_level.count };
    }
    else {
      bbo_cache_.ask = {};
    }
    if (bbo_cache_.bid.price != kUndefPrice && bbo_cache_.bid.price > bbo_cache_.ask.price) {
      //throw std::logic_error("bid price is higher than ask price?");
      bbo_cache_ = prev_bbo;
    }
  }

  std::vector<MarketByOrderEvent>::iterator OrderBook::GetLevelOrder(
    std::vector<MarketByOrderEvent>& level_orders, uint64_t order_id) {

    auto order_it = std::find_if(level_orders.begin(), level_orders.end(),
      [order_id](const MarketByOrderEvent& order) {
        return order.order_id == order_id;
      });
    if (order_it == level_orders.end()) {
      throw std::invalid_argument{ "No order with ID " + std::to_string(order_id) };
    }
    return order_it;
  }

  ///////////////////////////////////////////////////////////////////
  //////////////////// OrderBook Operations /////////////////////////
  ///////////////////////////////////////////////////////////////////

  // MARK: Clear
  void OrderBook::Clear() {
    orders_by_id_.clear();
    offers_.clear();
    bids_.clear();
  }

  // MARK: Add
  void OrderBook::Add(const MarketByOrderEvent& mbo) {
    if (mbo.side == OrderSide::kBid)
      Add(bids_, bids_compare_, mbo);
    else
      Add(offers_, asks_compare_, mbo);
  }

  template <class Compare>
  void OrderBook::Add(SideLevels& levels, Compare comp, const MarketByOrderEvent& mbo) {
    //Not using normalized/aggregate sets so should not encounter TOB flags  
    auto [it, inserted] = orders_by_id_.try_emplace(mbo.order_id,
      PriceSideSize{ mbo.price, mbo.side, mbo.size });
    if (!inserted) [[unlikely]] {
      throw std::invalid_argument{ "Received duplicated order ID " +
                                 std::to_string(mbo.order_id) };
    }
    else {
      orders_by_id_[mbo.order_id] = { mbo.price, mbo.side, mbo.size };
    }
    LevelQueue& level = GetOrInsertLevel(levels, mbo.price, comp);
    level.count++;
    level.size += mbo.size;
  }

  // MARK: Cancel
  void OrderBook::Cancel(const MarketByOrderEvent& mbo) {
    if (mbo.side == OrderSide::kBid)
      Cancel(bids_, bids_compare_, mbo);
    else
      Cancel(offers_, asks_compare_, mbo);
  }

  template <class Compare>
  void OrderBook::Cancel(SideLevels& levels, Compare comp, const MarketByOrderEvent& mbo) {
    auto order_it = orders_by_id_.find(mbo.order_id);
    if (order_it == orders_by_id_.end()) [[unlikely]] {
      throw std::invalid_argument{ "Received cancel order ID " +
        std::to_string(mbo.order_id) };
    } //TODO
    auto level_it = GetLevelIt(levels, order_it->second.price, comp);
    if (level_it == levels.rend()) [[unlikely]] {
      throw std::invalid_argument{ "Received cancel order ID " +
        std::to_string(mbo.order_id) };
    } //TODO

    level_it->second.size -= mbo.size;

    order_it->second.size -= mbo.size;
    if (order_it->second.size == 0) {
      orders_by_id_.erase(mbo.order_id);
      level_it->second.count--;
      if (level_it->second.count == 0) {     
        levels.erase(std::next(level_it).base());
      }
    }
  }

  // MARK: Modify
  void OrderBook::Modify(const MarketByOrderEvent& mbo) {
    auto orders_it = orders_by_id_.find(mbo.order_id);
    if (orders_it == orders_by_id_.end()) [[unlikely]] {
      Add(mbo);
      return;
    }
    if (orders_it->second.side != mbo.side) [[unlikely]] {
      [&] () __attribute__((noinline, cold)) {
        throw std::logic_error{ "Order " + std::to_string(mbo.order_id) + " changed side" };
      }();
    }

    if (mbo.side == OrderSide::kBid)
      Modify(bids_, bids_compare_, mbo, orders_it);
    else
      Modify(offers_, asks_compare_, mbo, orders_it);
  }

  template <class Compare>
  void OrderBook::Modify(SideLevels& levels, Compare comp, const MarketByOrderEvent& mbo, OrderBook::Orders::iterator prev_order_it) {
    auto prev_lvl_it = GetLevelIt(levels, prev_order_it->second.price, comp);
    // TODO check for prev_lvl_it != rend()
    LevelQueue& prev_level = prev_lvl_it->second;
    if (prev_order_it->second.price != mbo.price) {
      // delete from old level
      prev_level.count--;
      prev_level.size -= prev_order_it->second.size;
      if (prev_level.count == 0) {   
        levels.erase(std::next(prev_lvl_it).base());
      }

      // insert into new level
      LevelQueue& new_level = GetOrInsertLevel(levels, mbo.price, comp);
      new_level.count++;
      new_level.size += mbo.size;
    }
    else if (prev_order_it->second.size < mbo.size) { // increase — lose priority
      prev_level.size += (mbo.size - prev_order_it->second.size);
    }
    else { // decrease — keep priority
      prev_level.size -= (prev_order_it->second.size - mbo.size);
    }
    prev_order_it->second = { mbo.price, mbo.side, mbo.size };
  }

};

