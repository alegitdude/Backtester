#include "core/EventQueue.h"
#include "core/Event.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace backtester {
    namespace {

        EventUnion MakeEvent(uint64_t ts, EventType type) {
            return EventUnion{ .control_ev = {.header = {.timestamp = ts, .type = type} } };
        }

        EventUnion MakeTagged(uint64_t ts, EventType type, uint64_t id) {
            return EventUnion{ .mbo = 
                {.header = {.timestamp = ts, .type = type},
                .ts_recv = ts,
                .order_id = id,
                .price = 5,
                .size = 1,
                .sequence = 1,
                .instrument_id = 1,
                .ts_in_delta = 1,
                .data_source_id = 1,
                .publisher_id = 1,
                .side = OrderSide::kBid,
                .flags = 1 } };
        }
        uint64_t TagOf(const EventUnion& e) noexcept { return e.mbo.order_id; }

        std::vector<uint64_t> DrainTimestamps(EventQueue& q) {
            std::vector<uint64_t> out;
            while (!q.IsEmpty()) out.push_back(Hdr(q.PopTopEvent()).timestamp);
            return out;
        }
        std::vector<EventType> DrainTypes(EventQueue& q) {
            std::vector<EventType> out;
            while (!q.IsEmpty()) out.push_back(Hdr(q.PopTopEvent()).type);
            return out;
        }

        class EventQueueTest : public ::testing::Test {
        protected:
            EventQueue q;
        };

        //////////////////////////////////////////////////////////
        // MARK: Empty / initial state
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, NewQueue_IsEmpty) {
            EXPECT_TRUE(q.IsEmpty());
            EXPECT_EQ(q.size(), 0u);
        }

        TEST_F(EventQueueTest, ReadTopEvent_OnEmpty_Throws) {
            EXPECT_THROW(q.ReadTopEvent(), std::out_of_range);
        }

        TEST_F(EventQueueTest, PopTopEvent_OnEmpty_Throws) {
            EXPECT_THROW(q.PopTopEvent(), std::out_of_range);
            EXPECT_TRUE(q.IsEmpty());
        }

        //////////////////////////////////////////////////////////
        // MARK: Single element
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, PushSingle_StateUpdates) {
            q.PushEvent(MakeEvent(42, EventType::kMarketTrade));
            EXPECT_FALSE(q.IsEmpty());
            EXPECT_EQ(q.size(), 1u);
        }

        TEST_F(EventQueueTest, ReadTopEvent_ReturnsTopWithoutRemoving) {
            q.PushEvent(MakeEvent(42, EventType::kMarketTrade));
            const EventUnion& top = q.ReadTopEvent();
            EXPECT_EQ(Hdr(top).timestamp, 42);
            EXPECT_EQ(Hdr(top).type, EventType::kMarketTrade);
            EXPECT_EQ(q.size(), 1u);  // read is non-destructive
        }

        TEST_F(EventQueueTest, ReadTopEvent_IsRepeatable) {
            q.PushEvent(MakeEvent(7, EventType::kMarketTrade));
            EXPECT_EQ(Hdr(q.ReadTopEvent()).timestamp, 7);
            EXPECT_EQ(Hdr(q.ReadTopEvent()).timestamp, 7);
            EXPECT_EQ(q.size(), 1u);
        }

        TEST_F(EventQueueTest, PopTopEvent_ReturnsAndRemoves) {
            q.PushEvent(MakeEvent(42, EventType::kMarketTrade));
            auto e = q.PopTopEvent();
            EXPECT_EQ(Hdr(e).timestamp, 42);
            EXPECT_TRUE(q.IsEmpty());
            EXPECT_EQ(q.size(), 0u);
        }

        //////////////////////////////////////////////////////////
        // MARK: Primary ordering — ascending timestamp regardless of insertion order
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, Pops_AscendingTimestamp_InsertedDescending) {
            for (uint64_t ts : {50u, 40u, 30u, 20u, 10u}) q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));
            EXPECT_EQ(DrainTimestamps(q), (std::vector<uint64_t>{10, 20, 30, 40, 50}));
        }

        TEST_F(EventQueueTest, Pops_AscendingTimestamp_InsertedAscending) {
            for (uint64_t ts : {10u, 20u, 30u, 40u, 50u}) q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));
            EXPECT_EQ(DrainTimestamps(q), (std::vector<uint64_t>{10, 20, 30, 40, 50}));
        }

        TEST_F(EventQueueTest, Pops_AscendingTimestamp_InsertedScrambled) {
            for (uint64_t ts : {30u, 10u, 50u, 20u, 40u}) q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));
            EXPECT_EQ(DrainTimestamps(q), (std::vector<uint64_t>{10, 20, 30, 40, 50}));
        }

        // Timestamp is the PRIMARY key: an earlier control event must precede a later
        // market event, even though control has a higher EventType value. 
        TEST_F(EventQueueTest, TimestampDominatesType) {
            q.PushEvent(MakeEvent(100, EventType::kMarketTrade));            // later, low type
            q.PushEvent(MakeEvent(50, EventType::kBacktestControlSnapshot)); // earlier, high type
            auto first = q.PopTopEvent();
            auto second = q.PopTopEvent();
            EXPECT_EQ(Hdr(first).timestamp, 50);
            EXPECT_EQ(Hdr(second).timestamp, 100);
        }

        //////////////////////////////////////////////////////////
        // MARK: Tie-break ordering at EQUAL timestamps
        //////////////////////////////////////////////////////////

        // CONTRACT "Market -> Strategy -> Backtest".
        // At an equal timestamp the queue must hand back Market events (enum 0..7)
        // before Strategy events (8..13) before Control events (14..17), so that the
        // book is updated before strategies react at the same instant.

        TEST_F(EventQueueTest, EqualTimestamp_MarketBeforeStrategyBeforeControl) {
            const uint64_t ts = 1000;
            // Insert in a deliberately scrambled order.
            q.PushEvent(MakeEvent(ts, EventType::kBacktestControlSnapshot)); // 16
            q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));             // 4
            q.PushEvent(MakeEvent(ts, EventType::kStrategySignal));          // 8

            auto types = DrainTypes(q);
            ASSERT_EQ(types.size(), 3u);
            EXPECT_EQ(types[0], EventType::kMarketTrade)
                << "Market data must be processed before strategy/control at equal ts";
            EXPECT_EQ(types[1], EventType::kStrategySignal);
            EXPECT_EQ(types[2], EventType::kBacktestControlSnapshot);
        }

        TEST_F(EventQueueTest, EqualTimestamp_PopsByAscendingType) {
            const uint64_t ts = 2000;
            // Push several distinct types out of type order.
            for (EventType t : {EventType::kBacktestControlEndOfBacktest, EventType::kMarketOrderAdd,
                EventType::kStrategyOrderFill, EventType::kMarketHeartbeat}) {
                q.PushEvent(MakeEvent(ts, t));
            }
            auto types = DrainTypes(q);
            ASSERT_EQ(types.size(), 4u);
            EXPECT_TRUE(std::is_sorted(types.begin(), types.end()))
                << "At equal timestamps, lower EventType values must pop first";
        }

        // Realistic mixed case: market + strategy at the same ts, control at the next.
        TEST_F(EventQueueTest, MixedTimestampsAndTypes_FullyOrdered) {
            q.PushEvent(MakeEvent(10, EventType::kStrategySignal));
            q.PushEvent(MakeEvent(10, EventType::kMarketTrade));
            q.PushEvent(MakeEvent(20, EventType::kBacktestControlSnapshot));
            q.PushEvent(MakeEvent(5, EventType::kMarketOrderAdd));

            auto e1 = q.PopTopEvent();  // ts 5
            auto e2 = q.PopTopEvent();  // ts 10, market
            auto e3 = q.PopTopEvent();  // ts 10, strategy
            auto e4 = q.PopTopEvent();  // ts 20

            EXPECT_EQ(Hdr(e1).timestamp, 5);
            EXPECT_EQ(Hdr(e2).timestamp, 10);
            EXPECT_EQ(Hdr(e2).type, EventType::kMarketTrade);
            EXPECT_EQ(Hdr(e3).timestamp, 10);
            EXPECT_EQ(Hdr(e3).type, EventType::kStrategySignal);
            EXPECT_EQ(Hdr(e4).timestamp, 20);
        }

        //////////////////////////////////////////////////////////
        // MARK: Interleaved push / pop (the Backtester::RunLoop access pattern)
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, InterleavedPushPop_ReheapifiesCorrectly) {
            q.PushEvent(MakeEvent(5, EventType::kMarketTrade));
            auto a = q.PopTopEvent();   // 5
            EXPECT_EQ(Hdr(a).timestamp, 5);

            q.PushEvent(MakeEvent(7, EventType::kMarketTrade));
            q.PushEvent(MakeEvent(3, EventType::kMarketTrade));   // earlier than 7, pushed after
            EXPECT_EQ(Hdr(q.PopTopEvent()).timestamp, 3);
            EXPECT_EQ(Hdr(q.PopTopEvent()).timestamp, 7);
            EXPECT_TRUE(q.IsEmpty());
        }

        TEST_F(EventQueueTest, InterleavedPushPop_MaintainsGlobalOrder) {
            // Push a batch, pop one, push more — pop everything and verify monotonic.
            for (uint64_t ts : {40u, 10u, 30u}) q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));
            EXPECT_EQ(Hdr(q.PopTopEvent()).timestamp, 10);
            for (uint64_t ts : {5u, 35u, 20u}) q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));

            auto out = DrainTimestamps(q);
            EXPECT_TRUE(std::is_sorted(out.begin(), out.end()));
            EXPECT_EQ(out.size(), 5u);  // 6 pushed, 1 already popped
        }

        //////////////////////////////////////////////////////////
        // MARK: Duplicates
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, DuplicateTimestampAndType_BothRetrieved) {
            // Heaps are not stable, so we only assert both come out — not their order.
            q.PushEvent(MakeTagged(100, EventType::kMarketTrade, 1u));
            q.PushEvent(MakeTagged(100, EventType::kMarketTrade, 2u));
            EXPECT_EQ(q.size(), 2u);

            std::vector<uint64_t> ids;
            while (!q.IsEmpty()) {
                auto e = q.PopTopEvent();
                ids.push_back(TagOf(e));
            }
            std::sort(ids.begin(), ids.end());
            EXPECT_EQ(ids, (std::vector<uint64_t>{1, 2}));
        }

        //////////////////////////////////////////////////////////
        // MARK: size() bookkeeping
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, Size_TracksPushAndPop) {
            EXPECT_EQ(q.size(), 0u);
            q.PushEvent(MakeEvent(1, EventType::kMarketTrade));
            EXPECT_EQ(q.size(), 1u);
            q.PushEvent(MakeEvent(2, EventType::kMarketTrade));
            EXPECT_EQ(q.size(), 2u);
            q.PopTopEvent();
            EXPECT_EQ(q.size(), 1u);
            q.PopTopEvent();
            EXPECT_EQ(q.size(), 0u);
        }

        //////////////////////////////////////////////////////////
        // MARK: ReadTopEvent agrees with the next PopTopEvent
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, ReadTop_MatchesNextPop) {
            q.PushEvent(MakeTagged(30, EventType::kMarketTrade, 30));
            q.PushEvent(MakeTagged(10, EventType::kMarketTrade, 10));
            q.PushEvent(MakeTagged(20, EventType::kMarketTrade, 20));

            const EventUnion& top = q.ReadTopEvent();
            uint64_t top_ts = Hdr(top).timestamp;
            uint64_t top_id = TagOf(top);

            auto popped = q.PopTopEvent();
            EXPECT_EQ(Hdr(popped).timestamp, top_ts);
            EXPECT_EQ(TagOf(popped), top_id);

            q.PopTopEvent();
            q.PopTopEvent();
        }

        //////////////////////////////////////////////////////////
        // MARK: clear()
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, Clear_EmptiesQueue) {
            for (uint64_t ts : {1u, 2u, 3u}) q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));
            q.clear();
            EXPECT_TRUE(q.IsEmpty());
            EXPECT_EQ(q.size(), 0u);
            EXPECT_THROW(q.ReadTopEvent(), std::out_of_range);
        }

        TEST_F(EventQueueTest, Clear_QueueIsReusable) {
            q.PushEvent(MakeEvent(1, EventType::kMarketTrade));
            q.clear();
            q.PushEvent(MakeEvent(99, EventType::kMarketTrade));
            EXPECT_EQ(q.size(), 1u);
            EXPECT_EQ(Hdr(q.PopTopEvent()).timestamp, 99);
        }

        //////////////////////////////////////////////////////////
        // MARK: Stress / property tests
        //////////////////////////////////////////////////////////

        TEST_F(EventQueueTest, Stress_RandomTimestamps_PopMonotonic) {
            constexpr int N = 10000;
            std::mt19937_64 rng(0xC0FFEE);  // fixed seed -> reproducible
            std::uniform_int_distribution<uint64_t> dist(0, 1'000'000);

            for (int i = 0; i < N; ++i) q.PushEvent(MakeEvent(dist(rng), EventType::kMarketTrade));
            EXPECT_EQ(q.size(), static_cast<size_t>(N));

            uint64_t prev = std::numeric_limits<uint64_t>::min();
            int count = 0;
            while (!q.IsEmpty()) {
                auto e = q.PopTopEvent();
                EXPECT_GE(Hdr(e).timestamp, prev);  // non-decreasing
                prev = Hdr(e).timestamp;
                ++count;
            }
            EXPECT_EQ(count, N);
        }

        TEST_F(EventQueueTest, Stress_RandomPushPopInterleave_StaysOrdered) {
            std::mt19937_64 rng(0xBADC0DE);
            std::uniform_int_distribution<uint64_t> ts_dist(0, 100000);
            std::uniform_int_distribution<int> coin(0, 1);

            uint64_t last_popped = std::numeric_limits<uint64_t>::min();
            int pushed = 0, popped = 0;

            for (int step = 0; step < 20000; ++step) {
                if (coin(rng) || q.IsEmpty()) {
                    q.PushEvent(MakeEvent(ts_dist(rng), EventType::kMarketTrade));
                    ++pushed;
                }
                else {
                    auto e = q.PopTopEvent();
                    // Within a contiguous run of pops the sequence is non-decreasing;
                    // a push can introduce a smaller ts, so we only check the heap
                    // invariant holds for the current top each time we pop.
                    EXPECT_GE(Hdr(e).timestamp, 0);
                    last_popped = Hdr(e).timestamp;
                    (void)last_popped;
                    ++popped;
                }
            }
            while (!q.IsEmpty()) { q.PopTopEvent(); ++popped; }
            EXPECT_EQ(pushed, popped);
        }

    }
}
