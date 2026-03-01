#include "../../include/execution/ExecutionHandler.h"
#include "../../include/core/EventQueue.h"
#include "../../include/core/Event.h"
#include "../../include/core/Types.h"
#include "../../include/market_state/OBTypes.h"
#include <gtest/gtest.h>
#include <memory>

namespace backtester {

// =============================================================================
// MARK: Test Fixture
// =============================================================================

class ExecutionHandlerTest : public ::testing::Test {
 protected:
    static constexpr uint32_t kInstrId = 294973;
    static constexpr int64_t kTickSize = 250000000;       // 0.25 in 1e9
    static constexpr int64_t kTickValue = 12'500000000;    // 12.50 in 1e9
    static constexpr uint64_t kLatencyMs = 500;
    static constexpr uint64_t kLatencyNs = kLatencyMs * 1'000'000ULL;

    EventQueue event_queue_;
    AppConfig config_;

    void SetUp() override {
        config_.execution_latency_ms = kLatencyMs;
        config_.initial_cash = 100000;
        config_.traded_instruments = {{
            .instrument_id = kInstrId,
            .instrument_type = InstrumentType::FUT,
            .tick_size = kTickSize,
            .tick_value = kTickValue,
            .margin_req = 16500'000000000
        }};
    }

    // -------------------------------------------------------------------
    // Factory: StrategyOrderEvent
    // -------------------------------------------------------------------
    StrategyOrderEvent MakeOrderAdd(int32_t order_id, OrderSide side,
        int64_t price, uint32_t qty, uint64_t ts = 1000) {

        return StrategyOrderEvent(
            ts, EventType::kStrategyOrderAdd,
            order_id, kInstrId, side, price, qty, "TestStrat"
        );
    }

    StrategyOrderEvent MakeOrderCancel(int32_t order_id, uint64_t ts = 2000) {
        return StrategyOrderEvent(
            ts, EventType::kStrategyOrderCancel,
            order_id, kInstrId, OrderSide::kBid, 0, 0, "TestStrat"
        );
    }

    StrategyOrderEvent MakeOrderModify(int32_t order_id, OrderSide side,
        int64_t price, uint32_t qty, uint64_t ts = 2000) {

        return StrategyOrderEvent(
            ts, EventType::kStrategyOrderModify,
            order_id, kInstrId, side, price, qty, "TestStrat"
        );
    }

    // -------------------------------------------------------------------
    // Factory: MarketByOrderEvent
    // -------------------------------------------------------------------
    MarketByOrderEvent MakeMboTrade(int64_t price, uint32_t size,
        uint64_t ts, OrderSide side = OrderSide::kNone) {

        return MarketByOrderEvent(
            ts, EventType::kMarketTrade,
            ts, 1, kInstrId, side, price, size,
            0, 0, 0, 0, "ESZ5", "ES"
        );
    }

    MarketByOrderEvent MakeMboCancel(OrderSide side, int64_t price,
        uint32_t size, uint64_t ts, uint64_t order_id = 99999) {

        return MarketByOrderEvent(
            ts, EventType::kMarketOrderCancel,
            ts, 1, kInstrId, side, price, size,
            order_id, 0, 0, 0, "ESZ5", "ES"
        );
    }

    MarketByOrderEvent MakeMboAdd(OrderSide side, int64_t price,
        uint32_t size, uint64_t ts, uint64_t order_id = 88888) {

        return MarketByOrderEvent(
            ts, EventType::kMarketOrderAdd,
            ts, 1, kInstrId, side, price, size,
            order_id, 0, 0, 0, "ESZ5", "ES"
        );
    }

    MarketByOrderEvent MakeMboFill(int64_t price, uint32_t size,
        uint64_t ts, OrderSide side = OrderSide::kNone) {

        return MarketByOrderEvent(
            ts, EventType::kMarketFill,
            ts, 1, kInstrId, side, price, size,
            0, 0, 0, 0, "ESZ5", "ES"
        );
    }

    // Different instrument
    MarketByOrderEvent MakeMboTradeOtherInstr(int64_t price, uint32_t size,
        uint64_t ts) {

        return MarketByOrderEvent(
            ts, EventType::kMarketTrade,
            ts, 1, 999999, OrderSide::kNone, price, size,
            0, 0, 0, 0, "NQZ5", "NQ"
        );
    }

    // -------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------
    // Bbo MakeBbo(int64_t bid, int64_t ask) {
    //     return Bbo{bid, ask};
    // }

    // Pop all StrategyFillEvents from the queue and return them
    std::vector<std::unique_ptr<Event>> DrainFills() {
        std::vector<std::unique_ptr<Event>> fills;
        while (!event_queue_.IsEmpty()) {
            fills.push_back(event_queue_.PopTopEvent());
        }
        return fills;
    }

    const StrategyFillEvent* AsFill(const std::unique_ptr<Event>& e) {
        return static_cast<const StrategyFillEvent*>(e.get());
    }
};

// =============================================================================
// MARK: Construction & Initial State
// =============================================================================

TEST_F(ExecutionHandlerTest, InitialState_NoPendingOrders) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);

    EXPECT_FALSE(eh.HasPendingOrders());
    EXPECT_EQ(eh.PendingOrderCount(), 0);
    EXPECT_EQ(eh.GetPendingOrder(1), nullptr);
}

// =============================================================================
// MARK: Passive Order Placement
// =============================================================================

TEST_F(ExecutionHandlerTest, PassiveAdd_RegistersPendingOrder) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Bid at 5000 is passive (at the bid, not crossing ask at 5025)
    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
    eh.OnStrategyOrder(order, bbo, 100);

    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_EQ(eh.PendingOrderCount(), 1);

    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->order_id, 1);
    EXPECT_EQ(pending->instrument_id, kInstrId);
    EXPECT_EQ(pending->side, OrderSide::kBid);
    EXPECT_EQ(pending->price, 5000);
    EXPECT_EQ(pending->remaining_qty, 3);
    EXPECT_EQ(pending->submit_ts, 1000);
    EXPECT_EQ(pending->live_ts, 1000 + kLatencyNs);
    EXPECT_EQ(pending->qty_ahead, 100);

    // No fill should be emitted
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, PassiveAdd_AskSide) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Ask at 5025 is passive (at the ask, not crossing bid at 5000)
    auto order = MakeOrderAdd(2, OrderSide::kAsk, 5025, 5, 2000);
    eh.OnStrategyOrder(order, bbo, 200);

    const PendingOrder* pending = eh.GetPendingOrder(2);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->side, OrderSide::kAsk);
    EXPECT_EQ(pending->price, 5025);
    EXPECT_EQ(pending->qty_ahead, 200);
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, PassiveAdd_BidDeepInBook) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Bid at 4950 — well below the current bid, deeply passive
    auto order = MakeOrderAdd(3, OrderSide::kBid, 4950, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 500);

    const PendingOrder* pending = eh.GetPendingOrder(3);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->price, 4950);
    EXPECT_EQ(pending->qty_ahead, 500);
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, PassiveAdd_ZeroQueueDepth) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Bid at a new price level nobody else is at — queue depth is 0
    auto order = MakeOrderAdd(4, OrderSide::kBid, 5010, 2, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    const PendingOrder* pending = eh.GetPendingOrder(4);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->qty_ahead, 0);
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, DuplicateOrderId_Rejected) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
    eh.OnStrategyOrder(order1, bbo, 100);
    EXPECT_EQ(eh.PendingOrderCount(), 1);

    // Same order_id again
    auto order2 = MakeOrderAdd(1, OrderSide::kBid, 4990, 5, 2000);
    eh.OnStrategyOrder(order2, bbo, 50);

    // Should still be just the first order
    EXPECT_EQ(eh.PendingOrderCount(), 1);
    EXPECT_EQ(eh.GetPendingOrder(1)->price, 5000);
}

// =============================================================================
// MARK: Marketable Orders (Crossing the Spread)
// =============================================================================

TEST_F(ExecutionHandlerTest, MarketableOrder_BidAtAsk_FillsImmediately) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Bid at ask price = marketable
    auto order = MakeOrderAdd(1, OrderSide::kBid, 5025, 2, 1000);
    eh.OnStrategyOrder(order, bbo, 100);

    // Should NOT be pending — was filled immediately
    EXPECT_FALSE(eh.HasPendingOrders());

    // Should have emitted a fill
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);

    const StrategyFillEvent* fill = AsFill(fills[0]);
    EXPECT_EQ(fill->order_id, 1);
    EXPECT_EQ(fill->fill_price, 5025);  // Filled at the resting ask
    EXPECT_EQ(fill->fill_quantity, 2);
    EXPECT_EQ(fill->side, OrderSide::kBid);
    EXPECT_EQ(fill->timestamp, 1000 + kLatencyNs);  // Fill at live_ts
}

TEST_F(ExecutionHandlerTest, MarketableOrder_BidAboveAsk) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Bid above ask price = clearly marketable
    auto order = MakeOrderAdd(2, OrderSide::kBid, 5050, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_price, 5025);  // Still fills at resting ask
}

TEST_F(ExecutionHandlerTest, MarketableOrder_AskAtBid_FillsImmediately) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Ask at bid price = marketable sell
    auto order = MakeOrderAdd(3, OrderSide::kAsk, 5000, 4, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);

    const StrategyFillEvent* fill = AsFill(fills[0]);
    EXPECT_EQ(fill->fill_price, 5000);  // Filled at resting bid
    EXPECT_EQ(fill->fill_quantity, 4);
    EXPECT_EQ(fill->side, OrderSide::kAsk);
}

TEST_F(ExecutionHandlerTest, MarketableOrder_AskBelowBid) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(4, OrderSide::kAsk, 4975, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_price, 5000);
}

TEST_F(ExecutionHandlerTest, MarketableOrder_ZeroBbo_NotMarketable) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {0, 0, 0, 0, 0, 0}; // No market yet

    auto order = MakeOrderAdd(5, OrderSide::kBid, 5025, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    // With zero BBO, IsMarketable returns false — order goes passive
    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, MarketableOrder_OneSideZeroBbo) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 0, 0, 0}; // No ask

    auto order = MakeOrderAdd(6, OrderSide::kBid, 5050, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    // Can't determine marketability without both sides
    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

// =============================================================================
// MARK: Latency Gating
// =============================================================================

TEST_F(ExecutionHandlerTest, LatencyGating_OrderNotLiveBeforeLatency) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Place order at ts=1000, latency=500ms=500,000,000ns, live at 500,001,000
    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 10);

    // Trade at our price BEFORE we're live — should NOT fill
    auto trade = MakeMboTrade(5000, 20, 1000 + kLatencyNs - 1);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, LatencyGating_OrderLiveExactlyAtLatency) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);  // qty_ahead = 0, front of queue

    // Trade exactly at live_ts — should fill
    uint64_t live_ts = 1000 + kLatencyNs;
    auto trade = MakeMboTrade(5000, 5, live_ts);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
}

TEST_F(ExecutionHandlerTest, LatencyGating_OrderLiveAfterLatency) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    // Trade well after live_ts
    auto trade = MakeMboTrade(5000, 5, 1000 + kLatencyNs + 1'000'000'000ULL);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    EXPECT_FALSE(event_queue_.IsEmpty());
}

// =============================================================================
// MARK: Queue Position — Cancel Draining
// =============================================================================

TEST_F(ExecutionHandlerTest, QueueDrain_CancelReducesQtyAhead) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Cancel 20 from our level on our side
    auto cancel = MakeMboCancel(OrderSide::kBid, 5000, 20, after_live);
    eh.OnMarketEvent(cancel, bbo);

    // Still pending — cancels don't fill
    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());

    // qty_ahead should now be 30
    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->qty_ahead, 30);
}

TEST_F(ExecutionHandlerTest, QueueDrain_CancelBeyondQueueGoesNegative) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 10);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Cancel more than was ahead
    auto cancel = MakeMboCancel(OrderSide::kBid, 5000, 25, after_live);
    eh.OnMarketEvent(cancel, bbo);

    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_LT(pending->qty_ahead, 0);  // Negative is fine

    // Still no fill — only trades fill
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, QueueDrain_CancelOnWrongSide_NoEffect) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Cancel on ask side at our price — shouldn't affect our bid
    auto cancel = MakeMboCancel(OrderSide::kAsk, 5000, 20, after_live);
    eh.OnMarketEvent(cancel, bbo);

    EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 50);
}

TEST_F(ExecutionHandlerTest, QueueDrain_CancelAtDifferentPrice_NoEffect) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Cancel at different price on our side
    auto cancel = MakeMboCancel(OrderSide::kBid, 4975, 20, after_live);
    eh.OnMarketEvent(cancel, bbo);

    EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 50);
}

// =============================================================================
// MARK: Queue Position — Trade Fills
// =============================================================================

TEST_F(ExecutionHandlerTest, TradeFill_QueueFullyDrainedThenFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 2, 1000);
    eh.OnStrategyOrder(order, bbo, 30);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade 35 at our level: 30 drains queue, 5 remain, 2 fill us
    auto trade = MakeMboTrade(5000, 35, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 2);
    EXPECT_EQ(AsFill(fills[0])->fill_price, 5000);
}

TEST_F(ExecutionHandlerTest, TradeFill_ExactQueueDepthTrade_NoFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 2, 1000);
    eh.OnStrategyOrder(order, bbo, 30);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade exactly 30 — drains queue but no volume left for us
    auto trade = MakeMboTrade(5000, 30, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 0);
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, TradeFill_PartialFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Order for 10 contracts, 5 ahead
    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 10, 1000);
    eh.OnStrategyOrder(order, bbo, 5);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade 8: 5 drain queue, 3 fill us (partial — we wanted 10)
    auto trade = MakeMboTrade(5000, 8, after_live);
    eh.OnMarketEvent(trade, bbo);

    // Should still be pending with 7 remaining
    EXPECT_TRUE(eh.HasPendingOrders());

    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->remaining_qty, 7);
    EXPECT_EQ(pending->qty_ahead, 0);

    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 3);
}

TEST_F(ExecutionHandlerTest, TradeFill_PartialThenComplete) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 10, 1000);
    eh.OnStrategyOrder(order, bbo, 5);

    uint64_t t1 = 1000 + kLatencyNs + 1;
    uint64_t t2 = t1 + 1000;

    // First trade: 5 drain + 3 fill = partial
    auto trade1 = MakeMboTrade(5000, 8, t1);
    eh.OnMarketEvent(trade1, bbo);
    EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 7);

    // Second trade: 7 fill the rest (queue already 0)
    auto trade2 = MakeMboTrade(5000, 10, t2);
    eh.OnMarketEvent(trade2, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 2);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 3);
    EXPECT_EQ(AsFill(fills[1])->fill_quantity, 7);
}

TEST_F(ExecutionHandlerTest, TradeFill_QueueZero_ImmediateFillOnTrade) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Place at front of queue
    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    auto trade = MakeMboTrade(5000, 5, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 3);
}

TEST_F(ExecutionHandlerTest, TradeFill_AskSide) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kAsk, 5025, 2, 1000);
    eh.OnStrategyOrder(order, bbo, 10);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade at the ask level drains queue then fills us
    auto trade = MakeMboTrade(5025, 15, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 2);
    EXPECT_EQ(AsFill(fills[0])->side, OrderSide::kAsk);
}

TEST_F(ExecutionHandlerTest, TradeFill_CancelsThenTrade) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    uint64_t t = 1000 + kLatencyNs + 1;

    // Cancels drain 40 from queue
    eh.OnMarketEvent(MakeMboCancel(OrderSide::kBid, 5000, 25, t), bbo);
    eh.OnMarketEvent(MakeMboCancel(OrderSide::kBid, 5000, 15, t + 1), bbo);

    EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 10);

    // Trade 15: 10 drain remaining queue, 5 left, 1 fills us
    eh.OnMarketEvent(MakeMboTrade(5000, 15, t + 2), bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 1);
}

TEST_F(ExecutionHandlerTest, TradeFill_MarketStrategyFillEventType_AlsoFills) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // kMarketFill should be treated same as kMarketTrade
    auto fill = MakeMboFill(5000, 5, after_live);
    eh.OnMarketEvent(fill, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    EXPECT_FALSE(event_queue_.IsEmpty());
}

// =============================================================================
// MARK: Trade-Through Detection
// =============================================================================

TEST_F(ExecutionHandlerTest, TradeThrough_BidFilledWhenTradeBelow) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Resting bid at 5000 with lots of queue ahead
    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 5, 1000);
    eh.OnStrategyOrder(order, bbo, 999);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade at 4975 — below our bid, market traded through us
    auto trade = MakeMboTrade(4975, 1, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_price, 5000);  // Fill at our price, not trade price
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 5);   // Full fill regardless of queue
}

TEST_F(ExecutionHandlerTest, TradeThrough_AskFilledWhenTradeAbove) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kAsk, 5025, 3, 1000);
    eh.OnStrategyOrder(order, bbo, 999);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade at 5050 — above our ask
    auto trade = MakeMboTrade(5050, 1, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_price, 5025);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 3);
}

TEST_F(ExecutionHandlerTest, TradeThrough_TradeAtExactPrice_NotTradeThrough) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 999);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade exactly at our price — NOT a trade-through, uses queue logic
    auto trade = MakeMboTrade(5000, 2, after_live);
    eh.OnMarketEvent(trade, bbo);

    // 999 ahead, only 2 traded — still pending
    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 997);
}

TEST_F(ExecutionHandlerTest, TradeThrough_WrongDirection_NoFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Bid at 5000
    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade ABOVE our bid — doesn't affect us (that's the ask side trading)
    auto trade = MakeMboTrade(5050, 100, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

// =============================================================================
// MARK: Cancel Strategy Orders
// =============================================================================

TEST_F(ExecutionHandlerTest, CancelOrder_RemovesPending) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
    eh.OnStrategyOrder(order, bbo, 50);
    EXPECT_EQ(eh.PendingOrderCount(), 1);

    auto cancel = MakeOrderCancel(1, 2000);
    eh.OnStrategyOrder(cancel, bbo, 0);

    EXPECT_FALSE(eh.HasPendingOrders());
    EXPECT_EQ(eh.GetPendingOrder(1), nullptr);
}

TEST_F(ExecutionHandlerTest, CancelOrder_UnknownId_NoEffect) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    auto cancel = MakeOrderCancel(999, 2000);
    eh.OnStrategyOrder(cancel, bbo, 0);

    // Original order still pending
    EXPECT_EQ(eh.PendingOrderCount(), 1);
    EXPECT_NE(eh.GetPendingOrder(1), nullptr);
}

// =============================================================================
// MARK: Modify Strategy Orders
// =============================================================================

TEST_F(ExecutionHandlerTest, ModifyOrder_PriceChange_LosesPriority) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
    eh.OnStrategyOrder(order, bbo, 20);

    // Modify to new price — loses priority
    auto modify = MakeOrderModify(1, OrderSide::kBid, 4975, 3, 5000);
    eh.OnStrategyOrder(modify, bbo, 80);

    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->price, 4975);
    EXPECT_EQ(pending->qty_ahead, 80);  // Reset to new queue depth
    EXPECT_EQ(pending->live_ts, 5000 + kLatencyNs);  // New latency window
}

TEST_F(ExecutionHandlerTest, ModifyOrder_SizeIncrease_LosesPriority) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
    eh.OnStrategyOrder(order, bbo, 20);

    // Increase size at same price — loses priority
    auto modify = MakeOrderModify(1, OrderSide::kBid, 5000, 5, 5000);
    eh.OnStrategyOrder(modify, bbo, 50);

    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->remaining_qty, 5);
    EXPECT_EQ(pending->qty_ahead, 50);  // Reset
    EXPECT_EQ(pending->live_ts, 5000 + kLatencyNs);
}

TEST_F(ExecutionHandlerTest, ModifyOrder_SizeDecrease_RetainsPriority) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 5, 1000);
    eh.OnStrategyOrder(order, bbo, 20);

    uint64_t original_live_ts = eh.GetPendingOrder(1)->live_ts;

    // Decrease size at same price — retains priority
    auto modify = MakeOrderModify(1, OrderSide::kBid, 5000, 3, 5000);
    eh.OnStrategyOrder(modify, bbo, 100);  // New queue depth passed but should be ignored

    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->remaining_qty, 3);
    EXPECT_EQ(pending->qty_ahead, 20);  // Unchanged
    EXPECT_EQ(pending->live_ts, original_live_ts);  // Unchanged
}

TEST_F(ExecutionHandlerTest, ModifyOrder_UnknownId_NoEffect) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto modify = MakeOrderModify(999, OrderSide::kBid, 5000, 3, 2000);
    eh.OnStrategyOrder(modify, bbo, 50);

    EXPECT_FALSE(eh.HasPendingOrders());
}

// =============================================================================
// MARK: No-Fill Scenarios
// =============================================================================

TEST_F(ExecutionHandlerTest, NoFill_NoPendingOrders_MarketEventIgnored) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto trade = MakeMboTrade(5000, 100, 5000);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, NoFill_TradeOnDifferentInstrument) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade on a DIFFERENT instrument
    auto trade = MakeMboTradeOtherInstr(5000, 100, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, NoFill_TradeAtDifferentPrice) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade at a different price on the same instrument
    auto trade = MakeMboTrade(5025, 100, after_live);
    eh.OnMarketEvent(trade, bbo);

    // Price 5025 is above our bid at 5000 — no trade-through (wrong direction),
    // not at our price level — no effect
    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, NoFill_InsufficientVolumeToDrainQueue) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 100);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Small trade doesn't drain enough
    auto trade = MakeMboTrade(5000, 10, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 90);
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, NoFill_OrderAddAtLevel_DoesNotAffectQueue) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 50);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // New market order added at our level — should NOT affect qty_ahead
    auto add = MakeMboAdd(OrderSide::kBid, 5000, 10, after_live);
    eh.OnMarketEvent(add, bbo);

    EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 50);
}

// =============================================================================
// MARK: Multiple Pending Orders
// =============================================================================

TEST_F(ExecutionHandlerTest, MultiplePending_IndependentQueueTracking) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Two orders at different prices
    auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    auto order2 = MakeOrderAdd(2, OrderSide::kBid, 4975, 1, 1000);
    eh.OnStrategyOrder(order1, bbo, 30);
    eh.OnStrategyOrder(order2, bbo, 50);

    EXPECT_EQ(eh.PendingOrderCount(), 2);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade at 5000 drains order1's queue but not order2's
    auto trade = MakeMboTrade(5000, 35, after_live);
    eh.OnMarketEvent(trade, bbo);

    // Order 1 should fill (30 ahead, trade=35, 5 overflow fills our 1)
    // Order 2 is at 4975 — different price, untouched
    EXPECT_EQ(eh.PendingOrderCount(), 1);
    EXPECT_EQ(eh.GetPendingOrder(1), nullptr);
    EXPECT_NE(eh.GetPendingOrder(2), nullptr);
    EXPECT_EQ(eh.GetPendingOrder(2)->qty_ahead, 50);
}

TEST_F(ExecutionHandlerTest, MultiplePending_BothFilledByTradeThrough) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 2, 1000);
    auto order2 = MakeOrderAdd(2, OrderSide::kBid, 4975, 3, 1000);
    eh.OnStrategyOrder(order1, bbo, 100);
    eh.OnStrategyOrder(order2, bbo, 200);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade at 4950 — below both orders, both get traded through
    auto trade = MakeMboTrade(4950, 1, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_FALSE(eh.HasPendingOrders());
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 2);
}

TEST_F(ExecutionHandlerTest, MultiplePending_SamePriceSameSide) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Two orders at same price, different queue positions
    auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    auto order2 = MakeOrderAdd(2, OrderSide::kBid, 5000, 1, 1500);
    eh.OnStrategyOrder(order1, bbo, 10);
    eh.OnStrategyOrder(order2, bbo, 20);

    uint64_t after_live = 1500 + kLatencyNs + 1;

    // Trade 15: drains 10 ahead of order1 and fills it.
    // For order2: drains 15 from 20 ahead, leaving 5.
    auto trade = MakeMboTrade(5000, 15, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_EQ(eh.GetPendingOrder(1), nullptr);  // Filled
    EXPECT_NE(eh.GetPendingOrder(2), nullptr);
    EXPECT_EQ(eh.GetPendingOrder(2)->qty_ahead, 5);
}

TEST_F(ExecutionHandlerTest, MultiplePending_BidAndAsk) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto bid_order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    auto ask_order = MakeOrderAdd(2, OrderSide::kAsk, 5025, 1, 1000);
    eh.OnStrategyOrder(bid_order, bbo, 0);
    eh.OnStrategyOrder(ask_order, bbo, 0);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade at bid price fills the bid but not the ask
    auto trade = MakeMboTrade(5000, 5, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_EQ(eh.GetPendingOrder(1), nullptr);   // Bid filled
    EXPECT_NE(eh.GetPendingOrder(2), nullptr);    // Ask still pending
}

// =============================================================================
// MARK: Fill Event Correctness
// =============================================================================

TEST_F(ExecutionHandlerTest, StrategyFillEvent_HasCorrectFields) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(42, OrderSide::kAsk, 5025, 7, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t fill_ts = 1000 + kLatencyNs + 500;
    auto trade = MakeMboTrade(5025, 10, fill_ts);
    eh.OnMarketEvent(trade, bbo);

    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);

    const StrategyFillEvent* fill = AsFill(fills[0]);
    EXPECT_EQ(fill->type, EventType::kStrategyOrderFill);
    EXPECT_EQ(fill->timestamp, fill_ts);
    EXPECT_EQ(fill->order_id, 42);
    EXPECT_EQ(fill->instrument_id, kInstrId);
    EXPECT_EQ(fill->side, OrderSide::kAsk);
    EXPECT_EQ(fill->fill_price, 5025);
    EXPECT_EQ(fill->fill_quantity, 7);
    EXPECT_EQ(fill->strategy_id, "TestStrat");
}

TEST_F(ExecutionHandlerTest, StrategyFillEvent_MarketableFillTimestamp) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5030, 1, 3000);
    eh.OnStrategyOrder(order, bbo, 0);

    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);

    // Marketable fill timestamp should be submit_ts + latency
    EXPECT_EQ(AsFill(fills[0])->timestamp, 3000 + kLatencyNs);
}

// =============================================================================
// MARK: Top-of-Book Fill Model
// =============================================================================
// These tests require access to fill_model_ which is private. 
// Testing via the public interface: we construct a separate config or 
// add a setter. For now, we test the default (QueuePosition) extensively
// above and add TOB tests assuming a way to switch the model.
// If fill_model_ becomes configurable via AppConfig, these tests are ready.

// NOTE: If you add a SetFillModel() public method or make it configurable,
// uncomment and use these tests:

// TEST_F(ExecutionHandlerTest, TOB_BidFillsWhenAskDropsToPrice) {
//     ExecutionHandler eh(event_queue_, config_);
//     eh.SetFillModel(FillModel::TopOfBook);
//     Bbo bbo = MakeBbo(5000, 5025);
//
//     auto order = MakeOrderAdd(1, OrderSide::kBid, 4975, 1, 1000);
//     eh.OnStrategyOrder(order, bbo, 100);
//
//     uint64_t after_live = 1000 + kLatencyNs + 1;
//
//     // Ask drops to our bid price
//     Bbo new_bbo = MakeBbo(4950, 4975);
//     auto mbo = MakeMboTrade(4975, 1, after_live);
//     eh.OnMarketEvent(mbo, new_bbo);
//
//     EXPECT_FALSE(eh.HasPendingOrders());
//     auto fills = DrainFills();
//     ASSERT_EQ(fills.size(), 1);
//     EXPECT_EQ(AsFill(fills[0])->fill_price, 4975);
// }
//
// TEST_F(ExecutionHandlerTest, TOB_AskFillsWhenBidRisesToPrice) {
//     ExecutionHandler eh(event_queue_, config_);
//     eh.SetFillModel(FillModel::TopOfBook);
//     Bbo bbo = MakeBbo(5000, 5025);
//
//     auto order = MakeOrderAdd(1, OrderSide::kAsk, 5050, 1, 1000);
//     eh.OnStrategyOrder(order, bbo, 100);
//
//     uint64_t after_live = 1000 + kLatencyNs + 1;
//
//     Bbo new_bbo = MakeBbo(5050, 5075);
//     auto mbo = MakeMboTrade(5050, 1, after_live);
//     eh.OnMarketEvent(mbo, new_bbo);
//
//     EXPECT_FALSE(eh.HasPendingOrders());
//     auto fills = DrainFills();
//     ASSERT_EQ(fills.size(), 1);
// }
//
// TEST_F(ExecutionHandlerTest, TOB_NoFillWhenBboDoesntReachPrice) {
//     ExecutionHandler eh(event_queue_, config_);
//     eh.SetFillModel(FillModel::TopOfBook);
//     Bbo bbo = MakeBbo(5000, 5025);
//
//     auto order = MakeOrderAdd(1, OrderSide::kBid, 4900, 1, 1000);
//     eh.OnStrategyOrder(order, bbo, 100);
//
//     uint64_t after_live = 1000 + kLatencyNs + 1;
//
//     // Ask drops but not to our price
//     Bbo new_bbo = MakeBbo(4950, 4975);
//     auto mbo = MakeMboTrade(4975, 1, after_live);
//     eh.OnMarketEvent(mbo, new_bbo);
//
//     EXPECT_TRUE(eh.HasPendingOrders());
// }
//
// TEST_F(ExecutionHandlerTest, TOB_ZeroBbo_NoFill) {
//     ExecutionHandler eh(event_queue_, config_);
//     eh.SetFillModel(FillModel::TopOfBook);
//     Bbo bbo = MakeBbo(5000, 5025);
//
//     auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
//     eh.OnStrategyOrder(order, bbo, 0);
//
//     uint64_t after_live = 1000 + kLatencyNs + 1;
//
//     Bbo zero_bbo = MakeBbo(0, 0);
//     auto mbo = MakeMboTrade(5000, 1, after_live);
//     eh.OnMarketEvent(mbo, zero_bbo);
//
//     EXPECT_TRUE(eh.HasPendingOrders());
// }

// =============================================================================
// MARK: Edge Cases & Stress
// =============================================================================

TEST_F(ExecutionHandlerTest, EdgeCase_UnhandledOrderType_NoSideEffect) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // kStrategyOrderClear — not explicitly handled, falls to default
    StrategyOrderEvent clear_order(
        1000, EventType::kStrategyOrderClear,
        1, kInstrId, OrderSide::kBid, 5000, 1, "TestStrat"
    );
    eh.OnStrategyOrder(clear_order, bbo, 0);

    EXPECT_FALSE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, EdgeCase_MarketOrderAdd_NoFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Market order ADD should not trigger a fill
    auto add = MakeMboAdd(OrderSide::kBid, 5000, 10, after_live);
    eh.OnMarketEvent(add, bbo);

    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_TRUE(event_queue_.IsEmpty());
}

TEST_F(ExecutionHandlerTest, EdgeCase_LargeOrderSmallTrades_IncrementalFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 100, 1000);
    eh.OnStrategyOrder(order, bbo, 0);  // Front of queue

    uint64_t t = 1000 + kLatencyNs + 1;

    // 10 small trades of 5 each
    for (int i = 0; i < 10; ++i) {
        auto trade = MakeMboTrade(5000, 5, t + i);
        eh.OnMarketEvent(trade, bbo);
    }

    // Should have 50 filled (10 * 5), 50 remaining
    EXPECT_TRUE(eh.HasPendingOrders());
    EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 50);

    auto fills = DrainFills();
    EXPECT_EQ(fills.size(), 10);

    uint32_t total_filled = 0;
    for (auto& f : fills) {
        total_filled += AsFill(f)->fill_quantity;
    }
    EXPECT_EQ(total_filled, 50);
}

TEST_F(ExecutionHandlerTest, EdgeCase_CancelAfterPartialFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 10, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Partial fill of 3
    auto trade = MakeMboTrade(5000, 3, after_live);
    eh.OnMarketEvent(trade, bbo);
    EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 7);

    // Cancel the remaining
    auto cancel = MakeOrderCancel(1, after_live + 1000);
    eh.OnStrategyOrder(cancel, bbo, 0);

    EXPECT_FALSE(eh.HasPendingOrders());

    // Should have one partial fill event only
    auto fills = DrainFills();
    ASSERT_EQ(fills.size(), 1);
    EXPECT_EQ(AsFill(fills[0])->fill_quantity, 3);
}

TEST_F(ExecutionHandlerTest, EdgeCase_ModifyAfterPartialFill) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 10, 1000);
    eh.OnStrategyOrder(order, bbo, 0);

    uint64_t t1 = 1000 + kLatencyNs + 1;

    // Partial fill of 4
    auto trade = MakeMboTrade(5000, 4, t1);
    eh.OnMarketEvent(trade, bbo);
    EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 6);

    // Modify: decrease remaining from 6 to 3 (retains priority since decrease)
    auto modify = MakeOrderModify(1, OrderSide::kBid, 5000, 3, t1 + 1000);
    eh.OnStrategyOrder(modify, bbo, 50);

    const PendingOrder* pending = eh.GetPendingOrder(1);
    ASSERT_NE(pending, nullptr);
    EXPECT_EQ(pending->remaining_qty, 3);
    EXPECT_EQ(pending->qty_ahead, 0);  // Retained original (was 0)
}

TEST_F(ExecutionHandlerTest, EdgeCase_ManyOrders_CancelSome_FillRest) {
    ExecutionHandler eh(event_queue_, config_.execution_latency_ms);
    BidAskPair bbo = {5000, 1, 1, 5025, 1, 1};

    // Place 5 orders at different prices
    for (int i = 1; i <= 5; ++i) {
        auto order = MakeOrderAdd(i, OrderSide::kBid, 5000 - (i - 1) * 25,
            1, 1000);
        eh.OnStrategyOrder(order, bbo, 10);
    }
    EXPECT_EQ(eh.PendingOrderCount(), 5);

    // Cancel orders 2 and 4
    eh.OnStrategyOrder(MakeOrderCancel(2, 2000), bbo, 0);
    eh.OnStrategyOrder(MakeOrderCancel(4, 2000), bbo, 0);
    EXPECT_EQ(eh.PendingOrderCount(), 3);

    uint64_t after_live = 1000 + kLatencyNs + 1;

    // Trade through all remaining (trade at 4850 is below all resting bids)
    auto trade = MakeMboTrade(4850, 1, after_live);
    eh.OnMarketEvent(trade, bbo);

    EXPECT_EQ(eh.PendingOrderCount(), 0);
    auto fills = DrainFills();
    EXPECT_EQ(fills.size(), 3);
}

}