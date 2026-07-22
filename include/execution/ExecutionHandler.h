#pragma once
#include "../core/Types.h"
#include "../core/EventQueue.h"
#include "../market_state/IMarketDataProvider.h"
#include <unordered_map>

namespace backtester {
    constexpr size_t PENDING_ORDERS_RESERVE = 128;
    constexpr size_t MAX_AGGREGATE_DEPTH = 256;
    constexpr qty_t ZERO_QUANTITY = 0;

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

    enum class OrderState {
        PendingLive,
        Live
    };
    // Represents a strategy order living in the execution handler's shadow book.

    struct PendingOrder {
        int32_t order_id;
        std::string strategy_id;
        uint32_t instrument_id;
        OrderSide side;
        int64_t price;
        qty_t remaining_qty;     // Quantity not yet filled
        timestamp_t submit_ts;   // Timestamp the order was submitted by strategy
        timestamp_t live_ts;     // submit_ts + latency — when order becomes eligible
        int64_t qty_ahead;       // Queue depth: total size resting ahead at placement
        OrderState state = OrderState::PendingLive;

        bool IsLive(uint64_t current_ts) const { return current_ts >= live_ts; }
    };

    struct ConsumeBids {
        static const PriceLevel& Best(const BidAskPair& bbo) { return bbo.bid; }
        static constexpr int64_t kStep = -1;   // walk down from best bid
        static void Aggregate(const IMarketDataProvider& m, uint32_t instr,
            std::span<PriceLevel> s) {
            m.GetAggOBBidsSnapshot(instr, s);
        }
    };
    struct ConsumeAsks {
        static const PriceLevel& Best(const BidAskPair& bbo) { return bbo.ask; }
        static constexpr int64_t kStep = +1;   // walk up from best ask
        static void Aggregate(const IMarketDataProvider& m, uint32_t instr,
            std::span<PriceLevel> s) {
            m.GetAggOBAsksSnapshot(instr, s);
        }
    };

    class ExecutionHandler {
    public:
        ExecutionHandler(EventQueue& event_queue, const AppConfig& config,
            const IMarketDataProvider& market_snapshots);
        ~ExecutionHandler() = default;

        // -------------------------------------------------------------------
        // Called from Backtester::RunLoop when a StrategyOrderEvent is popped.
        // Registers the order in the shadow book as pending live.
        // -------------------------------------------------------------------
        void OnStrategyOrder(const StrategyOrderEvent& order);

        // -------------------------------------------------------------------
        // Called from Backtester::RunLoop on EVERY market event, AFTER the
        // real OrderBook has been updated. Checks all pending orders for
        // fills based on the market activity that just occurred.
        // Generates FillEvents and pushes them onto the EventQueue.
        // -------------------------------------------------------------------
        void OnMarketEvent(const MarketByOrderEvent& mbo_event);

        //void ProcessGoLives(timestamp_t now, const MarketByOrderEvent* trigger);
        void CancelAllPendingOrders();
        // -------------------------------------------------------------------
        // Accessors
        // -------------------------------------------------------------------
        bool HasPendingOrders() const { return !pending_orders_.empty(); }
        size_t PendingOrderCount() const { return pending_orders_.size(); }
        const PendingOrder* GetPendingOrder(int32_t order_id) const;


    private:
        EventQueue& event_queue_;
        const AppConfig& config_;
        const IMarketDataProvider& market_snapshots_;
        timestamp_t latency_ns_;
        FillModel fill_model_;

        std::vector<size_t> filled_idxs_;
        std::vector<PendingOrder> pending_orders_;

        // -------------------------------------------------------------------
        // Order placement handlers
        // -------------------------------------------------------------------
        void HandleAdd(const StrategyOrderEvent& order);
        void HandleCancel(const StrategyOrderEvent& order);
        void HandleModify(const StrategyOrderEvent& order);

        // -------------------------------------------------------------------
        // Fill logic per model
        // -------------------------------------------------------------------
        void CheckFillsQueuePosition(timestamp_t now, const MarketByOrderEvent* mbo_event);
        void CheckFillsTopOfBook(timestamp_t now, const MarketByOrderEvent* mbo_event);

        // -------------------------------------------------------------------
        // Helpers
        // -------------------------------------------------------------------
        void RunFillModel(timestamp_t now, const MarketByOrderEvent* ev);
        bool GoLive(PendingOrder& pending, const MarketByOrderEvent* mbo_event);

        money_t GetCommissionsByInstr(uint32_t instrument_id, qty_t fill_qty);

        void EmitFill(PendingOrder& order, int64_t fill_price,
            qty_t fill_qty, timestamp_t fill_ts);

        template <class Side>
        bool WalkTheBook(PendingOrder& order, const BidAskPair& bbo);
        // bool WalkTheBids(PendingOrder& order, BidAskPair bbo);
        // bool WalkTheAsks(PendingOrder& order, BidAskPair bbo);

        inline size_t CountPriceLevels(int64_t order_price, int64_t bbo_price, int64_t tick_size) {
            price_t price_diff = std::abs(order_price - bbo_price);
            return static_cast<size_t>((price_diff / tick_size) + 1);
        }
    };

}