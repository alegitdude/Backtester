#pragma once
#include "../core/Types.h"
#include "../core/EventQueue.h"
#include <unordered_map>

namespace backtester {

// ==================================================================================
// MARK: Fill Model
// ==================================================================================
// QueuePosition: Tracks queue depth from MBO data. Order fills only when
//   sufficient volume has traded through the price level ahead of our position.
//   Most realistic for passive limit orders.
//
// TopOfBook: Fills immediately when market BBO reaches or crosses the order
//   price. Optimistic assumption — useful as an upper-bound benchmark or for
//   aggressive limit orders that are expected to be at/near TOB.

enum class FillModel {
    QueuePosition,
    TopOfBook
};

// ==================================================================================
// MARK: Pending Order (Shadow Book Entry)
// ==================================================================================
// Represents a strategy order living in the execution handler's shadow book.
// This is NOT placed into the real OrderBook — it exists only here, tracking
// queue position as real market events drain or add size at the price level.

struct PendingOrder {
    int32_t order_id;
    std::string strategy_id;
    uint32_t instrument_id;
    OrderSide side;
    int64_t price;
    uint32_t remaining_qty;     // Quantity not yet filled
    uint64_t submit_ts;         // Timestamp the order was submitted by strategy
    uint64_t live_ts;           // submit_ts + latency — when order becomes eligible
    int64_t qty_ahead;          // Queue depth: total size resting ahead at placement

    bool IsLive(uint64_t current_ts) const { return current_ts >= live_ts; }
};

class ExecutionHandler {
public:
    ExecutionHandler(EventQueue& event_queue, const AppConfig& config);
    ~ExecutionHandler() = default;

    // -------------------------------------------------------------------
    // Called from Backtester::RunLoop when a StrategyOrderEvent is popped.
    // Registers the order in the shadow book as pending.
    // For marketable orders (crossing the spread), fills immediately.
    // -------------------------------------------------------------------
    void OnStrategyOrder(const StrategyOrderEvent& order, const BidAskPair& current_bbo,
        int64_t queue_depth_at_level);

    // -------------------------------------------------------------------
    // Called from Backtester::RunLoop on EVERY market event, AFTER the
    // real OrderBook has been updated. Checks all pending orders for
    // fills based on the market activity that just occurred.
    // Generates FillEvents and pushes them onto the EventQueue.
    // -------------------------------------------------------------------
    void OnMarketEvent(const MarketByOrderEvent& mbo_event, const BidAskPair& current_bbo);

    // -------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------
    bool HasPendingOrders() const { return !pending_orders_.empty(); }
    size_t PendingOrderCount() const { return pending_orders_.size(); }
    const PendingOrder* GetPendingOrder(int32_t order_id) const;
    std::vector<std::unique_ptr<StrategyFillEvent>> CancelAllPendingOrders(
    uint64_t cancel_ts);

 private:
    EventQueue& event_queue_;
    const AppConfig& config_; 
    uint64_t latency_ns_;
    FillModel fill_model_;

    // Shadow book: order_id -> PendingOrder
    std::unordered_map<int32_t, PendingOrder> pending_orders_;

    // -------------------------------------------------------------------
    // Order placement handlers
    // -------------------------------------------------------------------
    void HandleAdd(const StrategyOrderEvent& order, const BidAskPair& current_bbo,
        int64_t queue_depth_at_level);
    void HandleCancel(const StrategyOrderEvent& order);
    void HandleModify(const StrategyOrderEvent& order, const BidAskPair& current_bbo,
        int64_t queue_depth_at_level);

    // -------------------------------------------------------------------
    // Fill logic per model
    // -------------------------------------------------------------------
    void CheckFillsQueuePosition(const MarketByOrderEvent& mbo_event);
    void CheckFillsTopOfBook(const MarketByOrderEvent& mbo_event,
        const BidAskPair& current_bbo);

    // -------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------
    bool IsMarketable(OrderSide side, int64_t order_price,
        const BidAskPair& current_bbo) const;

    void EmitFill(PendingOrder& order, int64_t fill_price,
        uint32_t fill_qty, uint64_t fill_ts);
};

}