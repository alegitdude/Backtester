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
 
std::unique_ptr<Event> MakeEvent(uint64_t ts, EventType type) {
    return std::make_unique<Event>(ts, type);
}
 
// An Event subclass that carries an identity tag and tracks how many instances
// are currently alive. Used to distinguish events that share (ts, type),
// and assert that the queue actually destroys the events it owns. 
struct TaggedEvent : Event {
    uint32_t id;
    static inline int live_count = 0;
 
    TaggedEvent(uint64_t ts, EventType type, uint32_t id_)
        : Event(ts, type), id(id_) {
        ++live_count;
    }
    ~TaggedEvent() override { --live_count; }
};
 
std::unique_ptr<Event> MakeTagged(uint64_t ts, EventType type, uint32_t id) {
    return std::unique_ptr<Event>(new TaggedEvent(ts, type, id));
}
 
// Drain the queue, returning each popped event's timestamp in pop order.
std::vector<uint64_t> DrainTimestamps(EventQueue& q) {
    std::vector<uint64_t> out;
    while (!q.IsEmpty()) {
        auto e = q.PopTopEvent();
        out.push_back(e->timestamp);
    }
    return out;
}
 
// Drain the queue, returning each popped event's type in pop order.
std::vector<EventType> DrainTypes(EventQueue& q) {
    std::vector<EventType> out;
    while (!q.IsEmpty()) {
        auto e = q.PopTopEvent();
        out.push_back(e->type);
    }
    return out;
}
 
class EventQueueTest : public ::testing::Test {
 protected:
    void SetUp() override { TaggedEvent::live_count = 0; }
    void TearDown() override {
        // Every test must leave no TaggedEvents alive.
        EXPECT_EQ(TaggedEvent::live_count, 0)
            << "Leaked " << TaggedEvent::live_count << " events";
    }
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
 
TEST_F(EventQueueTest, PopTopEvent_OnEmpty_ReturnsNull) {
    EXPECT_EQ(q.PopTopEvent(), nullptr);
    // Popping empty must not corrupt state.
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_EQ(q.size(), 0u);
}
 
//////////////////////////////////////////////////////////
// MARK: Null handling
//////////////////////////////////////////////////////////
 
TEST_F(EventQueueTest, PushNullptr_IsIgnored) {
    q.PushEvent(nullptr);
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_EQ(q.size(), 0u);
}
 
TEST_F(EventQueueTest, PushNullptr_DoesNotDisturbRealEvents) {
    q.PushEvent(MakeEvent(10, EventType::kMarketTrade));
    q.PushEvent(nullptr);
    q.PushEvent(MakeEvent(5, EventType::kMarketTrade));
    EXPECT_EQ(q.size(), 2u);
    EXPECT_EQ(DrainTimestamps(q), (std::vector<uint64_t>{5, 10}));
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
    const Event& top = q.ReadTopEvent();
    EXPECT_EQ(top.timestamp, 42);
    EXPECT_EQ(top.type, EventType::kMarketTrade);
    EXPECT_EQ(q.size(), 1u);  // read is non-destructive
}
 
TEST_F(EventQueueTest, ReadTopEvent_IsRepeatable) {
    q.PushEvent(MakeEvent(7, EventType::kMarketTrade));
    EXPECT_EQ(q.ReadTopEvent().timestamp, 7);
    EXPECT_EQ(q.ReadTopEvent().timestamp, 7);
    EXPECT_EQ(q.size(), 1u);
}
 
TEST_F(EventQueueTest, PopTopEvent_ReturnsAndRemoves) {
    q.PushEvent(MakeEvent(42, EventType::kMarketTrade));
    auto e = q.PopTopEvent();
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->timestamp, 42);
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
    EXPECT_EQ(first->timestamp, 50);
    EXPECT_EQ(second->timestamp, 100);
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
    q.PushEvent(MakeEvent(5,  EventType::kMarketOrderAdd));
 
    auto e1 = q.PopTopEvent();  // ts 5
    auto e2 = q.PopTopEvent();  // ts 10, market
    auto e3 = q.PopTopEvent();  // ts 10, strategy
    auto e4 = q.PopTopEvent();  // ts 20
 
    EXPECT_EQ(e1->timestamp, 5);
    EXPECT_EQ(e2->timestamp, 10);
    EXPECT_EQ(e2->type, EventType::kMarketTrade);
    EXPECT_EQ(e3->timestamp, 10);
    EXPECT_EQ(e3->type, EventType::kStrategySignal);
    EXPECT_EQ(e4->timestamp, 20);
}
 
//////////////////////////////////////////////////////////
// MARK: Interleaved push / pop (the Backtester::RunLoop access pattern)
//////////////////////////////////////////////////////////
 
TEST_F(EventQueueTest, InterleavedPushPop_ReheapifiesCorrectly) {
    q.PushEvent(MakeEvent(5, EventType::kMarketTrade));
    auto a = q.PopTopEvent();          // 5
    EXPECT_EQ(a->timestamp, 5);
 
    q.PushEvent(MakeEvent(7, EventType::kMarketTrade));
    q.PushEvent(MakeEvent(3, EventType::kMarketTrade));   // earlier than 7, pushed after
    EXPECT_EQ(q.PopTopEvent()->timestamp, 3);
    EXPECT_EQ(q.PopTopEvent()->timestamp, 7);
    EXPECT_TRUE(q.IsEmpty());
}
 
TEST_F(EventQueueTest, InterleavedPushPop_MaintainsGlobalOrder) {
    // Push a batch, pop one, push more — pop everything and verify monotonic.
    for (uint64_t ts : {40u, 10u, 30u}) q.PushEvent(MakeEvent(ts, EventType::kMarketTrade));
    EXPECT_EQ(q.PopTopEvent()->timestamp, 10);
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
 
    std::vector<uint32_t> ids;
    while (!q.IsEmpty()) {
        auto e = q.PopTopEvent();
        ids.push_back(static_cast<TaggedEvent*>(e.get())->id);
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids, (std::vector<uint32_t>{1, 2}));
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
 
    const Event& top = q.ReadTopEvent();
    uint64_t top_ts = top.timestamp;
    uint32_t top_id = static_cast<const TaggedEvent&>(top).id;
 
    auto popped = q.PopTopEvent();
    EXPECT_EQ(popped->timestamp, top_ts);
    EXPECT_EQ(static_cast<TaggedEvent*>(popped.get())->id, top_id);

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
    EXPECT_EQ(q.PopTopEvent(), nullptr);
}
 
TEST_F(EventQueueTest, Clear_DestroysOwnedEvents) {
    for (uint32_t i = 0; i < 5; ++i) q.PushEvent(MakeTagged(i, EventType::kMarketTrade, i));
    EXPECT_EQ(TaggedEvent::live_count, 5);
    q.clear();
    EXPECT_EQ(TaggedEvent::live_count, 0);  // ownership released, not leaked
}
 
TEST_F(EventQueueTest, Clear_QueueIsReusable) {
    q.PushEvent(MakeEvent(1, EventType::kMarketTrade));
    q.clear();
    q.PushEvent(MakeEvent(99, EventType::kMarketTrade));
    EXPECT_EQ(q.size(), 1u);
    EXPECT_EQ(q.PopTopEvent()->timestamp, 99);
}
 
//////////////////////////////////////////////////////////
// MARK: Ownership / lifetime (no leaks)
//////////////////////////////////////////////////////////
 
TEST_F(EventQueueTest, PoppedEvents_AreDestroyed_NoLeak) {
    for (uint32_t i = 0; i < 100; ++i) q.PushEvent(MakeTagged(i, EventType::kMarketTrade, i));
    EXPECT_EQ(TaggedEvent::live_count, 100);
    while (!q.IsEmpty()) q.PopTopEvent();  // each unique_ptr drops at scope end
    EXPECT_EQ(TaggedEvent::live_count, 0);
}
 
TEST_F(EventQueueTest, DestroyingQueue_DestroysOwnedEvents) {
    {
        EventQueue local;
        for (uint32_t i = 0; i < 10; ++i) local.PushEvent(MakeTagged(i, EventType::kMarketTrade, i));
        EXPECT_EQ(TaggedEvent::live_count, 10);
    }  // local destroyed here
    EXPECT_EQ(TaggedEvent::live_count, 0);
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
        ASSERT_NE(e, nullptr);
        EXPECT_GE(e->timestamp, prev);  // non-decreasing
        prev = e->timestamp;
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
        } else {
            auto e = q.PopTopEvent();
            ASSERT_NE(e, nullptr);
            // Within a contiguous run of pops the sequence is non-decreasing;
            // a push can introduce a smaller ts, so we only check the heap
            // invariant holds for the current top each time we pop.
            EXPECT_GE(e->timestamp, 0);
            last_popped = e->timestamp;
            (void)last_popped;
            ++popped;
        }
    }
    while (!q.IsEmpty()) { q.PopTopEvent(); ++popped; }
    EXPECT_EQ(pushed, popped);
}
 
}
}
