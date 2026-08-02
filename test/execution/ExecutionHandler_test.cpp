#include "execution/ExecutionHandler.h"
#include "core/EventQueue.h"
#include "core/Event.h"
#include "core/Types.h"
#include "market_state/MarketStateManager.h"
#include "market_state/OBTypes.h"
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
        static constexpr uint64_t kLatencyMs = 20;
        static constexpr uint64_t kLatencyNs = kLatencyMs * 1'000'000ULL;
        static constexpr uint16_t kDataSourceId = 1;

        EventQueue event_queue_;
        AppConfig config_;
        MarketStateManager m_state_manager;


        void SetUp() override {
            config_.execution_latency_ms = kLatencyMs;
            config_.initial_cash = 100000;
            config_.traded_instruments = { {
                kInstrId,
                InstrumentType::FUT,
                kTickSize,
                kTickValue,
                16500'000000000,
                16500'000000000
            } };
            config_.commission_struct.fut_per_contract = 2'170'000'000;
            m_state_manager.Initialize({ kInstrId });
            auto bid_event = MakeMboAdd(OrderSide::kBid, 5000, 1, 1, 1);
            m_state_manager.OnMarketEvent(bid_event);
            auto ask_event = MakeMboAdd(OrderSide::kAsk, 5025, 1, 2, 2);
            m_state_manager.OnMarketEvent(ask_event);

        }

        // -------------------------------------------------------------------
        // Factory: StrategyOrderEvent
        // -------------------------------------------------------------------
        StrategyOrderEvent MakeOrderAdd(int32_t order_id, OrderSide side,
            int64_t price, uint32_t qty, uint64_t ts) {
            return StrategyOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kStrategyOrderAdd
                },
                .strategy_id = 1,
                .order_id = order_id,
                .instrument_id = kInstrId,
                .side = side,
                .price = price * 1'000'000'000,
                .quantity = qty
            };
        }

        StrategyOrderEvent MakeOrderCancel(int32_t order_id, uint64_t ts) {
            return StrategyOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kStrategyOrderCancel
                },
                .strategy_id = 1,
                .order_id = order_id,
                .instrument_id = kInstrId,
                .side = OrderSide::kBid,
                .price = 0,
                .quantity = 0
            };
        }

        StrategyOrderEvent MakeOrderModify(int32_t order_id, OrderSide side,
            int64_t price, uint32_t qty, uint64_t ts) {
            return StrategyOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kStrategyOrderModify
                },
                .strategy_id = 1,
                .order_id = order_id,
                .instrument_id = kInstrId,
                .side = side,
                .price = price * 1'000'000'000,
                .quantity = qty
            };
        }

        // -------------------------------------------------------------------
        // Factory: MarketByOrderEvent
        // -------------------------------------------------------------------
        MarketByOrderEvent MakeMboTrade(int64_t price, uint32_t size,
            uint64_t ts, OrderSide side) {
            return MarketByOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kMarketTrade
                },
                .ts_recv = ts,
                .order_id = 1, 
                .price = price * 1'000'000'000,
                .size = size,
                .sequence = 1,
                .instrument_id = kInstrId,
                .ts_in_delta = 0 ,
                .data_source_id = kDataSourceId,
                .publisher_id = 1,
                .side = side,
                .flags = 0x80
            };
        }

        MarketByOrderEvent MakeMboCancel(OrderSide side, int64_t price,
            uint32_t size, uint64_t ts, uint64_t order_id = 99999) {
            return MarketByOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kMarketOrderCancel
                },
                .ts_recv = ts,
                .order_id = order_id, 
                .price = price * 1'000'000'000,
                .size = size,
                .sequence = 1,
                .instrument_id = kInstrId,
                .ts_in_delta = 0 ,
                .data_source_id = kDataSourceId,
                .publisher_id = 1,
                .side = side,
                .flags = 0x80
            };
        }

        MarketByOrderEvent MakeMboAdd(OrderSide side, int64_t price,
            uint32_t size, uint64_t ts, uint64_t order_id = 88888) {
             return MarketByOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kMarketOrderAdd
                },
                .ts_recv = ts,
                .order_id = order_id, 
                .price = price * 1'000'000'000,
                .size = size,
                .sequence = 1,
                .instrument_id = kInstrId,
                .ts_in_delta = 0,
                .data_source_id = kDataSourceId,
                .publisher_id = 1,
                .side = side,
                .flags = 0x80
            };
        }

        MarketByOrderEvent MakeMboAddPub(OrderSide side, int64_t price,
            uint32_t size, uint64_t ts, uint16_t pub_id, uint64_t order_id = 88888) {
            return MarketByOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kMarketOrderAdd
                },
                .ts_recv = ts,
                .order_id = order_id, 
                .price = price * 1'000'000'000,
                .size = size,
                .sequence = 1,
                .instrument_id = kInstrId,
                .ts_in_delta = 0,
                .data_source_id = kDataSourceId,
                .publisher_id = pub_id,
                .side = side,
                .flags = 0x80
            };    
        }

        MarketByOrderEvent MakeMboFill(int64_t price, uint32_t size,
            uint64_t ts, OrderSide side) {
            return MarketByOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kMarketFill
                },
                .ts_recv = ts,
                .order_id = 1, 
                .price = price * 1'000'000'000,
                .size = size,
                .sequence = 1,
                .instrument_id = kInstrId,
                .ts_in_delta = 0,
                .data_source_id = kDataSourceId,
                .publisher_id = 1,
                .side = side,
                .flags = 0x80
            };     
        }

        // Different instrument
        MarketByOrderEvent MakeMboFillOtherInstr(int64_t price, uint32_t size,
            uint64_t ts) {
            return MarketByOrderEvent {
                .header = {
                    .timestamp = ts,
                    .type = EventType::kMarketTrade
                },
                .ts_recv = ts,
                .order_id = 1, 
                .price = price * 1'000'000'000,
                .size = size,
                .sequence = 1,
                .instrument_id = 2,
                .ts_in_delta = 0,
                .data_source_id = kDataSourceId,
                .publisher_id = 1,
                .side = OrderSide::kNone,
                .flags = 0x80
            };     
        }

        // -------------------------------------------------------------------
        // Helpers
        // -------------------------------------------------------------------

        void SeedDepth(OrderSide side, uint16_t publisher_id,
            std::initializer_list<std::pair<int64_t, uint32_t>> px_size) {
            static uint64_t oid = 100000;
            for (auto [px, sz] : px_size) {
                auto e = MakeMboAddPub(side, px, sz, /*ts*/10, publisher_id, oid++);
                m_state_manager.OnMarketEvent(e);
            }
        }

        // Pop all StrategyFillEvents from the queue and return them
        std::vector<EventUnion> DrainFills() {
            std::vector<EventUnion> fills;
            while (!event_queue_.IsEmpty()) {
                fills.push_back(event_queue_.PopTopEvent());
            }
            return fills;
        }

        const StrategyFillEvent* AsFill(const EventUnion& e) {
            return &e.strat_fill_ev;
        }
    };

    // =============================================================================
    // MARK: Construction & Initial State
    // =============================================================================

    TEST_F(ExecutionHandlerTest, InitialState_NoPendingOrders) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        EXPECT_FALSE(eh.HasPendingOrders());
        EXPECT_EQ(eh.PendingOrderCount(), 0);
        EXPECT_EQ(eh.GetPendingOrder(1), nullptr);
    }

    // =============================================================================
    // MARK: Passive Order Placement
    // =============================================================================

    TEST_F(ExecutionHandlerTest, PassiveAdd_RegistersPendingOrder) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Bid at 5000 is passive (at the bid, not crossing ask at 5025)
        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
        eh.OnStrategyOrder(order);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_EQ(eh.PendingOrderCount(), 1);

        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->order_id, 1);
        EXPECT_EQ(pending->instrument_id, kInstrId);
        EXPECT_EQ(pending->side, OrderSide::kBid);
        EXPECT_EQ(pending->price, 5000'000'000'000);
        EXPECT_EQ(pending->remaining_qty, 3);
        EXPECT_EQ(pending->submit_ts, 1000);
        EXPECT_EQ(pending->live_ts, 1000 + kLatencyNs);
        EXPECT_EQ(pending->qty_ahead, 0); // 0 because we calculate after live
        EXPECT_EQ(pending->state, OrderState::PendingLive); // TS has not reached live

        // No fill should be emitted
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, PassiveAdd_AskSide) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Ask at 5025 is passive (at the ask, not crossing bid at 5000)
        auto order = MakeOrderAdd(2, OrderSide::kAsk, 5025, 5, 1000);
        eh.OnStrategyOrder(order);

        // Market order TS pushes strategy order live
        auto add_event = MakeMboAdd(OrderSide::kBid, 5000, 1, 1001 + kLatencyNs, 5);
        m_state_manager.OnMarketEvent(add_event);
        eh.OnMarketEvent(add_event);

        const PendingOrder* pending = eh.GetPendingOrder(2);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->side, OrderSide::kAsk);
        EXPECT_EQ(pending->price, 5025'000'000'000);
        EXPECT_EQ(pending->qty_ahead, 1);
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, PassiveAdd_BidDeepInBook) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Market order before strategy order - adds to qty ahead
        auto add_event = MakeMboAdd(OrderSide::kBid, 4950, 500, 999, 5);
        m_state_manager.OnMarketEvent(add_event);

        // Strategy order - bid at 4950 — passive
        auto order = MakeOrderAdd(3, OrderSide::kBid, 4950, 1, 1000);
        eh.OnStrategyOrder(order);

        // Market order to nudge TS - strategy order is now live
        auto add_event2 = MakeMboAdd(OrderSide::kBid, 4950, 4, 1005 + kLatencyNs, 6);
        m_state_manager.OnMarketEvent(add_event2);
        eh.OnMarketEvent(add_event2);

        const PendingOrder* pending = eh.GetPendingOrder(3);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->price, 4950'000'000'000);
        EXPECT_EQ(pending->qty_ahead, 500);
        EXPECT_EQ(pending->state, OrderState::Live);
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, PassiveAdd_ZeroQueueDepth) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Bid at a new price level nobody else is at — queue depth is 0
        auto order = MakeOrderAdd(4, OrderSide::kBid, 5010, 2, 1000);
        eh.OnStrategyOrder(order);

        const PendingOrder* pending = eh.GetPendingOrder(4);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->qty_ahead, 0);
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, DuplicateOrderId_Rejected) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
        eh.OnStrategyOrder(order1);
        EXPECT_EQ(eh.PendingOrderCount(), 1);

        // Same order_id again
        auto order2 = MakeOrderAdd(1, OrderSide::kBid, 4990, 5, 2000);
        eh.OnStrategyOrder(order2);

        // Should still be just the first order
        EXPECT_EQ(eh.PendingOrderCount(), 1);
        EXPECT_EQ(eh.GetPendingOrder(1)->price, 5000'000'000'000);
    }

    // =============================================================================
    // MARK: Marketable Orders (Crossing the Spread)
    // =============================================================================

    TEST_F(ExecutionHandlerTest, MarketableOrder_BidAtAsk_FillsImmediately) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Bid at ask price = marketable
        auto order = MakeOrderAdd(1, OrderSide::kBid, 5025, 1, 3);
        eh.OnStrategyOrder(order);

        // Add another order to nudge current_time
        auto add_event = MakeMboAdd(OrderSide::kAsk, 5026, 1, 4 + kLatencyNs, 5);
        m_state_manager.OnMarketEvent(add_event);

        eh.OnMarketEvent(add_event);
        // Should NOT be pending — was filled immediately
        EXPECT_FALSE(eh.HasPendingOrders());

        // Should have emitted a fill
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);

        const StrategyFillEvent* fill = AsFill(fills[0]);
        EXPECT_EQ(fill->order_id, 1);
        EXPECT_EQ(fill->price, 5025'000'000'000);  // Filled at the resting ask
        EXPECT_EQ(fill->quantity, 1);
        EXPECT_EQ(fill->side, OrderSide::kBid);
        EXPECT_EQ(fill->header.timestamp, order.header.timestamp + kLatencyNs);  // Fill at live_ts
    }

    TEST_F(ExecutionHandlerTest, MarketableOrder_BidAboveAsk) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Bid above ask price = clearly marketable
        auto order = MakeOrderAdd(2, OrderSide::kBid, 5050, 1, 3);
        eh.OnStrategyOrder(order);

        // Add another order to nudge current_time
        auto add_event = MakeMboAdd(OrderSide::kBid, 4999, 1, 4 + kLatencyNs, 5);
        m_state_manager.OnMarketEvent(add_event);
        eh.OnMarketEvent(add_event);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->price, 5025'000'000'000);  // Still fills at resting ask
    }

    TEST_F(ExecutionHandlerTest, MarketableOrder_AskAtBid_FillsImmediately) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Ask at bid price = marketable sell
        auto order = MakeOrderAdd(3, OrderSide::kAsk, 5000, 1, 3);
        eh.OnStrategyOrder(order);

        // Add another order to nudge current_time
        auto add_event = MakeMboAdd(OrderSide::kBid, 4999, 1, 4 + kLatencyNs, 5);
        m_state_manager.OnMarketEvent(add_event);
        eh.OnMarketEvent(add_event);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);

        const StrategyFillEvent* fill = AsFill(fills[0]);
        EXPECT_EQ(fill->price, 5000'000'000'000);  // Filled at resting bid
        EXPECT_EQ(fill->quantity, 1);
        EXPECT_EQ(fill->side, OrderSide::kAsk);
    }

    TEST_F(ExecutionHandlerTest, MarketableOrder_AskBelowBid) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(4, OrderSide::kAsk, 4975, 1, 3);
        eh.OnStrategyOrder(order);

        // Add another order to nudge current_time
        auto add_event = MakeMboAdd(OrderSide::kAsk, 5010, 1, 4 + kLatencyNs, 5);
        m_state_manager.OnMarketEvent(add_event);
        eh.OnMarketEvent(add_event);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->price, 5000'000'000'000);
    }

    TEST_F(ExecutionHandlerTest, MarketableOrder_ZeroBbo_NotMarketable) {
        MarketStateManager blank_m_state_mang; // will have no bbo
        blank_m_state_mang.Initialize({ kInstrId });
        ExecutionHandler eh(event_queue_, config_, blank_m_state_mang);

        auto order = MakeOrderAdd(5, OrderSide::kBid, 5025, 1, 1000);
        eh.OnStrategyOrder(order);

        // With zero BBO IsMarketable returns false — order goes passive
        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    // =============================================================================
    // MARK: Latency Gating
    // =============================================================================

    TEST_F(ExecutionHandlerTest, LatencyGating_OrderNotLiveBeforeLatency) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Place order at ts=1000, latency=20ms=20,000,000ns, live at 20,001,000
        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        // Trade at our price BEFORE we're live — should NOT fill
        auto trade = MakeMboFill(5000, 20, 1000 + kLatencyNs - 1, OrderSide::kBid);
        m_state_manager.OnMarketEvent(trade);
        eh.OnMarketEvent(trade);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, LatencyGating_OrderLiveExactlyAtLatency) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5001, 1, 1000);
        eh.OnStrategyOrder(order);  // qty_ahead = 0, front of queue

        auto pending = eh.GetPendingOrder(1);
        EXPECT_EQ(pending->state, OrderState::PendingLive);

        // Trade exactly at live_ts — strategy order is now live
        uint64_t live_ts = 1000 + kLatencyNs;
        auto trade = MakeMboTrade(5001, 5, live_ts, OrderSide::kBid);
        m_state_manager.OnMarketEvent(trade);
        eh.OnMarketEvent(trade);

        auto pending_live = eh.GetPendingOrder(1);
        EXPECT_EQ(pending_live->state, OrderState::Live);

        // Fill exactly at live_ts — live strategy should fill
        auto fill = MakeMboFill(5001, 5, live_ts, OrderSide::kBid);
        m_state_manager.OnMarketEvent(fill);
        eh.OnMarketEvent(fill);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
    }

    // =============================================================================
    // MARK: Queue Position — Cancel Draining
    // =============================================================================

    TEST_F(ExecutionHandlerTest, QueueDrain_CancelReducesQtyAhead) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        auto mbo_event = MakeMboAdd(OrderSide::kBid, 5000, 49, 1001, 4);
        m_state_manager.OnMarketEvent(mbo_event);
        eh.OnMarketEvent(mbo_event);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Cancel 20 from our level on our side
        auto cancel = MakeMboCancel(OrderSide::kBid, 5000, 20, after_live, 4);
        m_state_manager.OnMarketEvent(cancel);
        eh.OnMarketEvent(cancel);

        // Still pending — cancels don't fill
        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());

        // qty_ahead should now be 30
        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->qty_ahead, 30);
    }

    TEST_F(ExecutionHandlerTest, QueueDrain_CancelBeyondQueueGoesNegative) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 5, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Mbo add after strategy order goes behind in the queue
        auto add = MakeMboAdd(OrderSide::kBid, 5000, 5, after_live, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // Cancel more than was ahead
        auto cancel = MakeMboCancel(OrderSide::kBid, 5000, 5, after_live + 1, 6);
        m_state_manager.OnMarketEvent(cancel);
        eh.OnMarketEvent(cancel);

        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_LT(pending->qty_ahead, 0);

        // Still no fill — only trades fill
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, QueueDrain_CancelOnWrongSide_NoEffect) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Cancel on ask side at our price — shouldn't affect our bid
        auto cancel = MakeMboCancel(OrderSide::kAsk, 5000, 20, after_live);
        eh.OnMarketEvent(cancel);

        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 1);
    }

    TEST_F(ExecutionHandlerTest, QueueDrain_CancelAtDifferentPrice_NoEffect) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Cancel at different price on our side
        auto cancel = MakeMboCancel(OrderSide::kBid, 4975, 20, after_live);
        eh.OnMarketEvent(cancel);

        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 1);
    }

    // =============================================================================
    // MARK: Queue Position — Trade Fills
    // =============================================================================

    TEST_F(ExecutionHandlerTest, TradeFill_QueueFullyDrainedThenFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 2, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Trade 35 at our level: 30 drains queue, 5 remain, our order is live
        auto trade = MakeMboTrade(5000, 35, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        // Follow up Fill message fills us
        auto fill = MakeMboFill(5000, 35, after_live, OrderSide::kBid);
        eh.OnMarketEvent(fill);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->quantity, 2);
        EXPECT_EQ(AsFill(fills[0])->price, 5000'000'000'000);
    }

    TEST_F(ExecutionHandlerTest, TradeFill_ExactQueueDepthTrade_NoFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 2, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Trade exactly 30 — drains queue but no volume left for us
        auto trade = MakeMboTrade(5000, 1, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        // Fill exactly 30 — drains queue but no volume left for us
        auto fill = MakeMboFill(5000, 1, after_live, OrderSide::kBid);
        eh.OnMarketEvent(fill);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 0);
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, TradeFill_PartialFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto add = MakeMboAdd(OrderSide::kBid, 5000, 4, 10, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // Order for 10 contracts, 5 ahead
        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 10, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Trade 8: 5 drain queue, 3 fill us (partial — we wanted 10)
        auto trade = MakeMboTrade(5000, 8, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        auto fill = MakeMboFill(5000, 8, after_live, OrderSide::kBid);
        eh.OnMarketEvent(fill);

        // Should still be pending with 7 remaining
        EXPECT_TRUE(eh.HasPendingOrders());

        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->remaining_qty, 7);
        EXPECT_EQ(pending->qty_ahead, 0);

        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->quantity, 3);
    }

    TEST_F(ExecutionHandlerTest, TradeFill_PartialThenComplete) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Seed book, level now has 5 resting
        auto add = MakeMboAdd(OrderSide::kAsk, 5025, 4, 10, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        //create our order
        auto order = MakeOrderAdd(1, OrderSide::kAsk, 5025, 10, 1000);
        eh.OnStrategyOrder(order);

        const PendingOrder* pending = eh.GetPendingOrder(1);
        EXPECT_EQ(pending->qty_ahead, 0); // not live

        uint64_t t1 = 1000 + kLatencyNs + 1;
        uint64_t t2 = t1 + 1000;

        // First trade: 5 drain + 3 fill = partial
        auto trade1 = MakeMboTrade(5025, 8, t1, OrderSide::kAsk);
        eh.OnMarketEvent(trade1);

        auto fill1 = MakeMboFill(5025, 8, t1, OrderSide::kAsk);
        eh.OnMarketEvent(fill1);

        EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 7);

        // Second trade: 7 fill the rest (queue already 0)
        auto trade2 = MakeMboTrade(5025, 8, t1, OrderSide::kAsk);
        eh.OnMarketEvent(trade2);

        auto fill2 = MakeMboFill(5025, 10, t2, OrderSide::kAsk);
        eh.OnMarketEvent(fill2);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 2);
        EXPECT_EQ(AsFill(fills[0])->quantity, 3);
        EXPECT_EQ(AsFill(fills[1])->quantity, 7);
    }

    TEST_F(ExecutionHandlerTest, TradeFill_QueueZero_ImmediateFillOnTrade) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Place at front of queue
        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        auto trade = MakeMboTrade(5000, 5, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);
        auto fill = MakeMboFill(5000, 5, after_live, OrderSide::kBid);
        eh.OnMarketEvent(fill);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->quantity, 3);
    }

    TEST_F(ExecutionHandlerTest, TradeFill_AskSide) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kAsk, 5025, 2, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Trade at the ask level drains queue then fills us
        auto trade = MakeMboTrade(5025, 15, after_live, OrderSide::kAsk);
        eh.OnMarketEvent(trade);
        auto fill = MakeMboFill(5025, 15, after_live, OrderSide::kAsk);
        eh.OnMarketEvent(fill);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->quantity, 2);
        EXPECT_EQ(AsFill(fills[0])->side, OrderSide::kAsk);
    }

    TEST_F(ExecutionHandlerTest, TradeFill_CancelsThenTrade) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        //Seed book, level now has 11 resting
        auto add = MakeMboAdd(OrderSide::kBid, 5000, 10, 10, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t t = 1000 + kLatencyNs + 1;

        // Cancels drain 5 from queue
        auto cancel1 = MakeMboCancel(OrderSide::kBid, 5000, 5, t, 6);
        m_state_manager.OnMarketEvent(cancel1);
        eh.OnMarketEvent(cancel1);

        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 6);

        // Trade 7: 6 drain remaining queue, 1 left, 1 fills us
        eh.OnMarketEvent(MakeMboFill(5000, 7, t + 2, OrderSide::kBid));

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->quantity, 1);
    }

    TEST_F(ExecutionHandlerTest, TradeFill_MarketStrategyFillEventType_AlsoFills) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add = MakeMboAdd(OrderSide::kBid, 4999, 5, after_live, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // kMarketFill should be treated same as kMarketTrade
        auto fill = MakeMboFill(5000, 5, after_live, OrderSide::kBid);
        eh.OnMarketEvent(fill);

        EXPECT_FALSE(eh.HasPendingOrders());
        EXPECT_FALSE(event_queue_.IsEmpty());
    }

    // =============================================================================
    // MARK: Trade-Through Detection
    // =============================================================================

    TEST_F(ExecutionHandlerTest, TradeThrough_BidFilledWhenTradeBelow) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Resting bid at 5000 with lots of queue ahead
        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 5, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add = MakeMboAdd(OrderSide::kBid, 4999, 5, after_live, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // Trade at 4975 — below our bid, market traded through us
        auto trade = MakeMboFill(4975, 1, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->price, 5000'000'000'000);  // Fill at our price, not trade price
        EXPECT_EQ(AsFill(fills[0])->quantity, 5);   // Full fill regardless of queue
    }

    TEST_F(ExecutionHandlerTest, TradeThrough_AskFilledWhenTradeAbove) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kAsk, 5025, 3, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add = MakeMboAdd(OrderSide::kBid, 4999, 5, after_live, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // Trade at 5050 — above our ask
        auto trade = MakeMboFill(5050, 1, after_live, OrderSide::kAsk);
        eh.OnMarketEvent(trade);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->price, 5025'000'000'000);
        EXPECT_EQ(AsFill(fills[0])->quantity, 3);
    }

    TEST_F(ExecutionHandlerTest, TradeThrough_TradeAtExactPrice_NotTradeThrough) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // seed 20 orders
        auto add = MakeMboAdd(OrderSide::kBid, 5000, 20, 900, 6);
        m_state_manager.OnMarketEvent(add);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add2 = MakeMboAdd(OrderSide::kBid, 4999, 5, after_live, 12);
        m_state_manager.OnMarketEvent(add2);
        eh.OnMarketEvent(add2);

        // Trade exactly at our price — NOT a trade-through, uses queue logic
        auto trade = MakeMboFill(5000, 2, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        // 21 ahead, only 2 traded — still pending with 19 ahead
        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 19);
    }

    TEST_F(ExecutionHandlerTest, TradeThrough_WrongDirection_NoFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Bid at 5000
        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add2 = MakeMboAdd(OrderSide::kBid, 4999, 5, after_live, 12);
        m_state_manager.OnMarketEvent(add2);
        eh.OnMarketEvent(add2);

        // Trade ABOVE our bid — doesn't affect us (that's the ask side trading)
        auto trade = MakeMboFill(5050, 100, after_live, OrderSide::kAsk);
        eh.OnMarketEvent(trade);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    // =============================================================================
    // MARK: Cancel Strategy Orders
    // =============================================================================

    TEST_F(ExecutionHandlerTest, CancelOrder_RemovesPending) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
        eh.OnStrategyOrder(order);
        EXPECT_EQ(eh.PendingOrderCount(), 1);

        auto cancel = MakeOrderCancel(1, 2000);
        eh.OnStrategyOrder(cancel);

        EXPECT_FALSE(eh.HasPendingOrders());
        EXPECT_EQ(eh.GetPendingOrder(1), nullptr);
    }

    TEST_F(ExecutionHandlerTest, CancelOrder_UnknownId_NoEffect) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
        eh.OnStrategyOrder(order);

        auto cancel = MakeOrderCancel(999, 2000);
        eh.OnStrategyOrder(cancel);

        // Original order still pending
        EXPECT_EQ(eh.PendingOrderCount(), 1);
        EXPECT_NE(eh.GetPendingOrder(1), nullptr);
    }

    // =============================================================================
    // MARK: Modify Strategy Orders
    // =============================================================================

    TEST_F(ExecutionHandlerTest, ModifyOrder_PriceChange_LosesPriority) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        //Seed book, level now has 10 resting
        auto add = MakeMboAdd(OrderSide::kBid, 4975, 5, 10, 6);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
        eh.OnStrategyOrder(order);

        // nudge timestamp so order is live
        auto add2 = MakeMboAdd(OrderSide::kBid, 4999, 5, 1000 + kLatencyNs, 12);
        m_state_manager.OnMarketEvent(add2);
        eh.OnMarketEvent(add2);

        // confirm live order
        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->price, 5000'000'000'000);
        EXPECT_EQ(pending->qty_ahead, 1);  // Reset to new queue depth
        EXPECT_EQ(pending->live_ts, 1000 + kLatencyNs);  // New latency window

        // Modify to new price — loses priority
        auto modify = MakeOrderModify(1, OrderSide::kBid, 4975, 3, 5000);
        eh.OnStrategyOrder(modify);


        // nudge timestamp so order is live
        auto add3 = MakeMboAdd(OrderSide::kBid, 4900, 5, 5000 + kLatencyNs, 17);
        m_state_manager.OnMarketEvent(add3);
        eh.OnMarketEvent(add3);

        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->price, 4975'000'000'000);
        EXPECT_EQ(pending->qty_ahead, 5);  // Reset to new queue depth
        EXPECT_EQ(pending->live_ts, 5000 + kLatencyNs);  // New latency window
    }

    TEST_F(ExecutionHandlerTest, ModifyOrder_SizeIncrease_LosesPriority) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 3, 1000);
        eh.OnStrategyOrder(order);

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4999, 5, 1000 + kLatencyNs, 12);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->price, 5000'000'000'000);
        EXPECT_EQ(pending->qty_ahead, 1);

        // add orders behind our strategy
        auto add2 = MakeMboAdd(OrderSide::kBid, 5000, 5, 1000 + kLatencyNs + 1, 13);
        m_state_manager.OnMarketEvent(add2);
        eh.OnMarketEvent(add2);

        // Increase size at same price — loses priority
        auto modify = MakeOrderModify(1, OrderSide::kBid, 5000, 5, 5000);
        eh.OnStrategyOrder(modify);

        // nudge timestamp so order is live
        auto add3 = MakeMboAdd(OrderSide::kBid, 4999, 5, 5000 + kLatencyNs, 15);
        m_state_manager.OnMarketEvent(add3);
        eh.OnMarketEvent(add3);

        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->remaining_qty, 5);
        EXPECT_EQ(pending->qty_ahead, 6);  // Orignal 1 +_added 5
        EXPECT_EQ(pending->live_ts, 5000 + kLatencyNs);
    }

    TEST_F(ExecutionHandlerTest, ModifyOrder_SizeDecrease_RetainsPriority) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 5, 1000);
        eh.OnStrategyOrder(order);

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4999, 5, 1000 + kLatencyNs, 12);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->price, 5000'000'000'000);
        EXPECT_EQ(pending->qty_ahead, 1);

        uint64_t original_live_ts = pending->live_ts;

        // add orders behind our strategy
        auto add2 = MakeMboAdd(OrderSide::kBid, 5000, 5, 1000 + kLatencyNs + 1, 13);
        m_state_manager.OnMarketEvent(add2);
        eh.OnMarketEvent(add2);

        // Decrease size at same price — retains priority
        auto modify = MakeOrderModify(1, OrderSide::kBid, 5000, 3, 5000);
        eh.OnStrategyOrder(modify);  // New queue depth passed but should be ignored

        // nudge timestamp so order is live
        auto add3 = MakeMboAdd(OrderSide::kBid, 4999, 5, 5000 + kLatencyNs + 1, 14);
        m_state_manager.OnMarketEvent(add3);
        eh.OnMarketEvent(add3);

        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->remaining_qty, 3);
        EXPECT_EQ(pending->qty_ahead, 1);  // Unchanged
        EXPECT_EQ(pending->live_ts, original_live_ts);  // Unchanged
    }

    TEST_F(ExecutionHandlerTest, ModifyOrder_UnknownId_NoEffect) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto modify = MakeOrderModify(999, OrderSide::kBid, 5000, 3, 2000);
        eh.OnStrategyOrder(modify);

        EXPECT_FALSE(eh.HasPendingOrders());
    }

    // =============================================================================
    // MARK: No-Fill Scenarios
    // =============================================================================

    TEST_F(ExecutionHandlerTest, NoFill_NoPendingOrders_MarketEventIgnored) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto trade = MakeMboFill(5000, 100, 5000, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, NoFill_TradeOnDifferentInstrument) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add = MakeMboAdd(OrderSide::kBid, 4999, 5, 1000 + kLatencyNs + 1, 14);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // Trade on a DIFFERENT instrument
        auto trade = MakeMboFillOtherInstr(5000, 100, after_live);
        eh.OnMarketEvent(trade);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, NoFill_TradeAtDifferentPrice) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add = MakeMboAdd(OrderSide::kBid, 4999, 5, 1000 + kLatencyNs + 1, 14);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // Trade at a different price on the same instrument
        auto trade = MakeMboFill(5025, 100, after_live, OrderSide::kAsk);
        eh.OnMarketEvent(trade);

        // Price 5025 is above our bid at 5000 — no trade-through (wrong direction),
        // not at our price level — no effect
        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, NoFill_InsufficientVolumeToDrainQueue) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Seed 5 contracts
        auto add = MakeMboAdd(OrderSide::kBid, 5000, 5, 12, 14);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4999, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        // Small trade doesn't drain enough
        auto trade = MakeMboFill(5000, 3, after_live + 1, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 3);
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, NoFill_OrderAddAtLevel_DoesNotAffectQueue) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4999, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 1);

        // New market order added at our level — should NOT affect qty_ahead
        auto add = MakeMboAdd(OrderSide::kBid, 5000, 10, after_live + 1);
        eh.OnMarketEvent(add);

        EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 1);
    }

    // =============================================================================
    // MARK: Multiple Pending Orders
    // =============================================================================

    TEST_F(ExecutionHandlerTest, MultiplePending_IndependentQueueTracking) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Seed 5 contracts
        auto add = MakeMboAdd(OrderSide::kBid, 4975, 5, 12, 14);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        // Two orders at different prices
        auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        auto order2 = MakeOrderAdd(2, OrderSide::kBid, 4975, 1, 1000);
        eh.OnStrategyOrder(order1);
        eh.OnStrategyOrder(order2);

        EXPECT_EQ(eh.PendingOrderCount(), 2);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        EXPECT_EQ(eh.GetPendingOrder(2)->qty_ahead, 5);

        // Trade at 5000 drains order1's queue but not order2's
        auto trade = MakeMboFill(5000, 2, after_live + 1, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        // Order 1 should fill (1 ahead, trade=2, 1 overflow fills our 1)
        // Order 2 is at 4975 — different price, untouched
        EXPECT_EQ(eh.PendingOrderCount(), 1);
        EXPECT_EQ(eh.GetPendingOrder(1), nullptr);
        EXPECT_NE(eh.GetPendingOrder(2), nullptr);
        EXPECT_EQ(eh.GetPendingOrder(2)->qty_ahead, 5);
    }

    TEST_F(ExecutionHandlerTest, MultiplePending_BothFilledByTradeThrough) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);


        auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 2, 1000);
        auto order2 = MakeOrderAdd(2, OrderSide::kBid, 4975, 3, 1000);
        eh.OnStrategyOrder(order1);
        eh.OnStrategyOrder(order2);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        // Trade at 4950 — below both orders, both get traded through
        auto trade = MakeMboFill(4950, 1, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        EXPECT_FALSE(eh.HasPendingOrders());
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 2);
    }

    // TEST_F(ExecutionHandlerTest, MultiplePending_SamePriceSameSide) {
    //     ExecutionHandler eh(event_queue_, config_, m_state_manager);

    //     // Two orders at same price, different queue positions
    //     auto order1 = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    //     auto order2 = MakeOrderAdd(2, OrderSide::kBid, 5000, 1, 1500);
    //     eh.OnStrategyOrder(order1);
    //     eh.OnStrategyOrder(order2);

    //     uint64_t after_live = 1500 + kLatencyNs + 1;

    //      // nudge timestamp so order is live
    //     auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
    //     m_state_manager.OnMarketEvent(add1);
    //     eh.OnMarketEvent(add1);

    //     EXPECT_EQ(eh.GetPendingOrder(1)->qty_ahead, 1);
    //     EXPECT_EQ(eh.GetPendingOrder(2)->qty_ahead, 2);

    //     // Trade 15: drains 10 ahead of order1 and fills it.
    //     // For order2: drains 15 from 20 ahead, leaving 5.
    //     auto trade = MakeMboFill(5000, 2, after_live + 1, OrderSide::kBid);
    //     eh.OnMarketEvent(trade);

    //     EXPECT_EQ(eh.GetPendingOrder(1), nullptr);  // Filled
    //     EXPECT_NE(eh.GetPendingOrder(2), nullptr);
    //     EXPECT_EQ(eh.GetPendingOrder(2)->qty_ahead, 0);
    // }

    TEST_F(ExecutionHandlerTest, MultiplePending_BidAndAsk) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto bid_order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        auto ask_order = MakeOrderAdd(2, OrderSide::kAsk, 5025, 1, 1000);
        eh.OnStrategyOrder(bid_order);
        eh.OnStrategyOrder(ask_order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);


        // Trade at bid price fills the bid but not the ask
        auto trade = MakeMboFill(5000, 5, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        EXPECT_EQ(eh.GetPendingOrder(1), nullptr);   // Bid filled
        EXPECT_NE(eh.GetPendingOrder(2), nullptr);    // Ask still pending
    }

    // =============================================================================
    // MARK: Fill Event Correctness
    // =============================================================================

    TEST_F(ExecutionHandlerTest, StrategyFillEvent_HasCorrectFields) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(42, OrderSide::kAsk, 5025, 7, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs;

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        uint64_t fill_ts = 1000 + kLatencyNs + 500;
        auto trade = MakeMboFill(5025, 10, fill_ts, OrderSide::kAsk);
        eh.OnMarketEvent(trade);

        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);

        const StrategyFillEvent* fill = AsFill(fills[0]);
        EXPECT_EQ(fill->header.type, EventType::kStrategyOrderFill);
        EXPECT_EQ(fill->header.timestamp, fill_ts);
        EXPECT_EQ(fill->order_id, 42);
        EXPECT_EQ(fill->instrument_id, kInstrId);
        EXPECT_EQ(fill->side, OrderSide::kAsk);
        EXPECT_EQ(fill->price, 5025'000'000'000);
        EXPECT_EQ(fill->quantity, 7);
        EXPECT_EQ(fill->strategy_id, 1);
    }

    TEST_F(ExecutionHandlerTest, StrategyFillEvent_MarketableFillTimestamp) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5030, 1, 3);
        eh.OnStrategyOrder(order);

        // Add another order to nudge current_time
        auto add_event = MakeMboAdd(OrderSide::kAsk, 5026, 1, 4 + kLatencyNs, 5);
        m_state_manager.OnMarketEvent(add_event);

        eh.OnMarketEvent(add_event);

        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);

        // Marketable fill timestamp should be submit_ts + latency
        EXPECT_EQ(AsFill(fills[0])->header.timestamp, 3 + kLatencyNs);
    }

    // =============================================================================
    // MARK: Top-of-Book Fill Model
    // =============================================================================

    // TEST_F(ExecutionHandlerTest, TOB_BidFillsWhenAskDropsToPrice) {
    //     ExecutionHandler eh(event_queue_, config_, m_state_manager);
    //     eh.SetFillModel(FillModel::TopOfBook);
    //     Bbo bbo = MakeBbo(5000, 5025);
    //
    //     auto order = MakeOrderAdd(1, OrderSide::kBid, 4975, 1, 1000);
    //     eh.OnStrategyOrder(order0);
    //
    //     uint64_t after_live = 1000 + kLatencyNs + 1;
    //
    //     // Ask drops to our bid price
    //     Bbo new_bbo = MakeBbo(4950, 4975);
    //     auto mbo = MakeMboFill(4975, 1, after_live);
    //     eh.OnMarketEvent(mbo, new_bbo);
    //
    //     EXPECT_FALSE(eh.HasPendingOrders());
    //     auto fills = DrainFills();
    //     ASSERT_EQ(fills.size(), 1);
    //     EXPECT_EQ(AsFill(fills[0])->fill_price, 4975);
    // }
    //
    // TEST_F(ExecutionHandlerTest, TOB_AskFillsWhenBidRisesToPrice) {
    //     ExecutionHandler eh(event_queue_, config_, m_state_manager);
    //     eh.SetFillModel(FillModel::TopOfBook);
    //     Bbo bbo = MakeBbo(5000, 5025);
    //
    //     auto order = MakeOrderAdd(1, OrderSide::kAsk, 5050, 1, 1000);
    //     eh.OnStrategyOrder(order0);
    //
    //     uint64_t after_live = 1000 + kLatencyNs + 1;
    //
    //     Bbo new_bbo = MakeBbo(5050, 5075);
    //     auto mbo = MakeMboFill(5050, 1, after_live);
    //     eh.OnMarketEvent(mbo, new_bbo);
    //
    //     EXPECT_FALSE(eh.HasPendingOrders());
    //     auto fills = DrainFills();
    //     ASSERT_EQ(fills.size(), 1);
    // }
    //
    // TEST_F(ExecutionHandlerTest, TOB_NoFillWhenBboDoesntReachPrice) {
    //     ExecutionHandler eh(event_queue_, config_, m_state_manager);
    //     eh.SetFillModel(FillModel::TopOfBook);
    //     Bbo bbo = MakeBbo(5000, 5025);
    //
    //     auto order = MakeOrderAdd(1, OrderSide::kBid, 4900, 1, 1000);
    //     eh.OnStrategyOrder(order0);
    //
    //     uint64_t after_live = 1000 + kLatencyNs + 1;
    //
    //     // Ask drops but not to our price
    //     Bbo new_bbo = MakeBbo(4950, 4975);
    //     auto mbo = MakeMboFill(4975, 1, after_live);
    //     eh.OnMarketEvent(mbo, new_bbo);
    //
    //     EXPECT_TRUE(eh.HasPendingOrders());
    // }
    //
    // TEST_F(ExecutionHandlerTest, TOB_ZeroBbo_NoFill) {
    //     ExecutionHandler eh(event_queue_, config_, m_state_manager);
    //     eh.SetFillModel(FillModel::TopOfBook);
    //     Bbo bbo = MakeBbo(5000, 5025);
    //
    //     auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
    //     eh.OnStrategyOrder(order);
    //
    //     uint64_t after_live = 1000 + kLatencyNs + 1;
    //
    //     Bbo zero_bbo = MakeBbo(0, 0);
    //     auto mbo = MakeMboFill(5000, 1, after_live);
    //     eh.OnMarketEvent(mbo, zero_bbo);
    //
    //     EXPECT_TRUE(eh.HasPendingOrders());
    // }

    // =============================================================================
    // MARK: Edge Cases & Stress
    // =============================================================================

    TEST_F(ExecutionHandlerTest, EdgeCase_UnhandledOrderType_NoSideEffect) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // kStrategyOrderClear — not explicitly handled, falls to default
        StrategyOrderEvent clear_order {
            .header = {.timestamp = 1000, .type = EventType::kStrategyOrderClear
            },
            .strategy_id = 1, .order_id = 1, .instrument_id = kInstrId,
            .side = OrderSide::kBid, .price = 5000, .quantity = 1
        };
        eh.OnStrategyOrder(clear_order);

        EXPECT_FALSE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, EdgeCase_MarketOrderAdd_NoFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 1, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;

        // Market order ADD should not trigger a fill
        auto add = MakeMboAdd(OrderSide::kBid, 5000, 10, after_live);
        eh.OnMarketEvent(add);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_TRUE(event_queue_.IsEmpty());
    }

    TEST_F(ExecutionHandlerTest, EdgeCase_LargeOrderSmallTrades_IncrementalFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5001, 100, 1000);
        eh.OnStrategyOrder(order);  // Front of queue

        uint64_t after_live = 1000 + kLatencyNs;

        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        uint64_t t = after_live + 1u;

        // 10 small trades of 5 each
        for (uint64_t i = 0; i < 10; ++i) {
            auto trade = MakeMboFill(5001, 5, t + i, OrderSide::kBid);
            eh.OnMarketEvent(trade);
        }

        // Should have 50 filled (10 * 5), 50 remaining
        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 50);

        auto fills = DrainFills();
        EXPECT_EQ(fills.size(), 10);

        int64_t total_filled = 0;
        for (auto& f : fills) {
            total_filled += AsFill(f)->quantity;
        }
        EXPECT_EQ(total_filled, 50);
    }

    TEST_F(ExecutionHandlerTest, EdgeCase_CancelAfterPartialFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 10, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs + 1;
        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        // Partial fill of 3
        auto trade = MakeMboFill(5000, 3, after_live, OrderSide::kBid);
        eh.OnMarketEvent(trade);
        EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 8);

        // Cancel the remaining
        auto cancel = MakeOrderCancel(1, after_live + 1000);
        eh.OnStrategyOrder(cancel);

        EXPECT_FALSE(eh.HasPendingOrders());

        // Should have one partial fill event only
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 1);
        EXPECT_EQ(AsFill(fills[0])->quantity, 2);
    }

    TEST_F(ExecutionHandlerTest, EdgeCase_ModifyAfterPartialFill) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        auto order = MakeOrderAdd(1, OrderSide::kBid, 5000, 10, 1000);
        eh.OnStrategyOrder(order);

        uint64_t after_live = 1000 + kLatencyNs;
        // nudge timestamp so order is live
        auto add1 = MakeMboAdd(OrderSide::kBid, 4974, 5, after_live, 15);
        m_state_manager.OnMarketEvent(add1);
        eh.OnMarketEvent(add1);

        // Partial fill of 4
        auto trade = MakeMboFill(5000, 4, after_live + 1, OrderSide::kBid);
        eh.OnMarketEvent(trade);
        EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 7);

        // Modify: decrease remaining from 6 to 3 (retains priority since decrease)
        auto modify = MakeOrderModify(1, OrderSide::kBid, 5000, 3, after_live + 1000);
        eh.OnStrategyOrder(modify);

        const PendingOrder* pending = eh.GetPendingOrder(1);
        ASSERT_NE(pending, nullptr);
        EXPECT_EQ(pending->remaining_qty, 3);
        EXPECT_EQ(pending->qty_ahead, 0);  // Retained original (was 0)
    }

    TEST_F(ExecutionHandlerTest, EdgeCase_ManyOrders_CancelSome_FillRest) {
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Place 5 orders at different prices
        for (int i = 1; i <= 5; ++i) {
            auto order = MakeOrderAdd(i, OrderSide::kBid, 5000 - i,
                1, 1000);
            eh.OnStrategyOrder(order);
        }
        EXPECT_EQ(eh.PendingOrderCount(), 5);

        uint64_t after_live = 1000 + kLatencyNs + 1;
        // Cancel orders 2 and 4
        eh.OnStrategyOrder(MakeOrderCancel(2, after_live));
        eh.OnStrategyOrder(MakeOrderCancel(4, after_live));
        EXPECT_EQ(eh.PendingOrderCount(), 3);

        // Trade through all remaining (trade at 4850 is below all resting bids)
        auto trade = MakeMboFill(4850, 1, after_live + 1, OrderSide::kBid);
        eh.OnMarketEvent(trade);

        EXPECT_EQ(eh.PendingOrderCount(), 0);
        auto fills = DrainFills();
        EXPECT_EQ(fills.size(), 3);
    }

    TEST_F(ExecutionHandlerTest, Walk_BuyCrossesMultipleAskLevels) {
        // asks: 5025x2 (1 from setup seed), 5050x3, 5075x1  
        SeedDepth(OrderSide::kAsk, 1, { {5025,1},{5050,3},{5075,1} });
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Buy 5 @ 5075: crosses; should take 2+3 = 5 across 5025 and 5050, stop before 5075.
        eh.OnStrategyOrder(MakeOrderAdd(1, OrderSide::kBid, 5075, 5, 1000));
        auto add = MakeMboAdd(OrderSide::kAsk, 5100, 1, 1000 + kLatencyNs, 7); // clock nudge, off-level
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        EXPECT_FALSE(eh.HasPendingOrders());              // fully filled
        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 2);                        // one per consumed level
        EXPECT_EQ(AsFill(fills[0])->price, 5025'000'000'000);
        EXPECT_EQ(AsFill(fills[0])->quantity, 2);
        EXPECT_EQ(AsFill(fills[1])->price, 5050'000'000'000);
        EXPECT_EQ(AsFill(fills[1])->quantity, 3);
        // all fills timestamped at live_ts
        EXPECT_EQ(AsFill(fills[0])->header.timestamp, 1000 + kLatencyNs);
    }

    TEST_F(ExecutionHandlerTest, Walk_SellCrossesMultipleBidLevels) {
        SeedDepth(OrderSide::kBid, 1, { {5000,3},{4975,2},{4950,10} });
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        // Sell 5 @ 4975: crosses; takes 4 @5000 + 1 @4975, remainder 0.
        eh.OnStrategyOrder(MakeOrderAdd(1, OrderSide::kAsk, 4975, 5, 1000));
        auto add = MakeMboAdd(OrderSide::kBid, 4900, 1, 1000 + kLatencyNs, 7);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        auto fills = DrainFills();
        ASSERT_EQ(fills.size(), 2);
        EXPECT_EQ(AsFill(fills[0])->price, 5000'000'000'000);
        EXPECT_EQ(AsFill(fills[0])->quantity, 4);
        EXPECT_EQ(AsFill(fills[1])->price, 4975'000'000'000);
        EXPECT_EQ(AsFill(fills[1])->quantity, 1);
        EXPECT_FALSE(eh.HasPendingOrders());
    }

    TEST_F(ExecutionHandlerTest, Walk_AggregatesAcrossPublishers) {
        SeedDepth(OrderSide::kAsk, 1, { {5025,1} });
        SeedDepth(OrderSide::kAsk, 2, { {5025,3} });   // same price, different venue => 5 total
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        eh.OnStrategyOrder(MakeOrderAdd(1, OrderSide::kBid, 5025, 5, 1000));
        auto add = MakeMboAdd(OrderSide::kAsk, 5100, 1, 1000 + kLatencyNs, 7);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        auto fills = DrainFills();
        // Depending on whether you emit per-publisher or per-price: assert TOTAL filled = 5 at 5025.
        int64_t total = 0; for (auto& f : fills) total += AsFill(f)->quantity;
        EXPECT_EQ(total, 5);
        for (auto& f : fills) EXPECT_EQ(AsFill(f)->price, 5025'000'000'000);
        EXPECT_FALSE(eh.HasPendingOrders());
    }

    TEST_F(ExecutionHandlerTest, Walk_PartialFillRestsRemainderNoRefill) {
        SeedDepth(OrderSide::kAsk, 1, { {5025,1} });    // only 2 available
        ExecutionHandler eh(event_queue_, config_, m_state_manager);

        eh.OnStrategyOrder(MakeOrderAdd(1, OrderSide::kBid, 5025, 5, 1000)); // wants 5
        auto add = MakeMboAdd(OrderSide::kAsk, 5100, 1, 1000 + kLatencyNs, 7);
        m_state_manager.OnMarketEvent(add);
        eh.OnMarketEvent(add);

        EXPECT_TRUE(eh.HasPendingOrders());
        EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 3);
        auto fills = DrainFills(); 
        ASSERT_EQ(fills.size(), 1); 
        EXPECT_EQ(AsFill(fills[0])->quantity, 2);

        // Another unrelated event with unchanged book must NOT refill the resting 3.
        auto ev2 = MakeMboAdd(OrderSide::kAsk, 5200, 1, 1000 + kLatencyNs + 5, 8);
        m_state_manager.OnMarketEvent(ev2);
        eh.OnMarketEvent(ev2);
        EXPECT_TRUE(event_queue_.IsEmpty());              // no refill
        EXPECT_EQ(eh.GetPendingOrder(1)->remaining_qty, 3);
    }

}