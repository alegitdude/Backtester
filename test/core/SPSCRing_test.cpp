#include "../include/core/SPSCRing.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <random>
#include <thread>
#include <vector>

namespace backtester {
namespace {

struct Payload {
  uint64_t seq;
  uint64_t tag;
};
static_assert(std::is_trivially_copyable_v<Payload>);

//////////////////////////////////////////////////////////
// MARK: Single-threaded fixture
//////////////////////////////////////////////////////////

class SPSCRingTest : public ::testing::Test {
 protected:
  SPSCRing<uint64_t, 4> ring;
};

//////////////////////////////////////////////////////////
// MARK: Empty / initial state
//////////////////////////////////////////////////////////

TEST_F(SPSCRingTest, NewRing_PopFails) {
  uint64_t out = 12345;
  EXPECT_FALSE(ring.TryPop(out));
  EXPECT_EQ(out, 12345u);  // out must be untouched on failure
}

//////////////////////////////////////////////////////////
// MARK: Single element round-trip
//////////////////////////////////////////////////////////

TEST_F(SPSCRingTest, PushThenPop_ReturnsSameValue) {
  ASSERT_TRUE(ring.TryPush(42));
  uint64_t out = 0;
  ASSERT_TRUE(ring.TryPop(out));
  EXPECT_EQ(out, 42u);
}

TEST_F(SPSCRingTest, PopAfterDrain_Fails) {
  ASSERT_TRUE(ring.TryPush(7));
  uint64_t out;
  ASSERT_TRUE(ring.TryPop(out));
  EXPECT_FALSE(ring.TryPop(out));  // now empty again
}

//////////////////////////////////////////////////////////
// MARK: Capacity / full behavior
//////////////////////////////////////////////////////////

TEST_F(SPSCRingTest, FillsToExactlyCapacity_ThenRejects) {
  EXPECT_TRUE(ring.TryPush(1));
  EXPECT_TRUE(ring.TryPush(2));
  EXPECT_TRUE(ring.TryPush(3));
  EXPECT_TRUE(ring.TryPush(4));   // capacity == 4
  EXPECT_FALSE(ring.TryPush(5));  // 5th must be rejected
  EXPECT_FALSE(ring.TryPush(6));  // still full, no corruption
}

TEST_F(SPSCRingTest, FullRing_PreservesAllValues) {
  for (uint64_t i = 1; i <= 4; ++i) ASSERT_TRUE(ring.TryPush(i));
  for (uint64_t i = 1; i <= 4; ++i) {
    uint64_t out;
    ASSERT_TRUE(ring.TryPop(out));
    EXPECT_EQ(out, i);  // FIFO order
  }
}

//////////////////////////////////////////////////////////
// MARK: FIFO ordering
//////////////////////////////////////////////////////////

TEST_F(SPSCRingTest, MaintainsFifoOrder) {
  for (uint64_t i = 10; i < 14; ++i) ASSERT_TRUE(ring.TryPush(i));
  std::vector<uint64_t> out;
  uint64_t v;
  while (ring.TryPop(v)) out.push_back(v);
  EXPECT_EQ(out, (std::vector<uint64_t>{10, 11, 12, 13}));
}

//////////////////////////////////////////////////////////
// MARK: Wraparound — indices exceed Capacity many times over
//////////////////////////////////////////////////////////

TEST_F(SPSCRingTest, WrapsAroundManyTimes_StaysFifo) {
  uint64_t next_push = 0, expect = 0;
  for (int round = 0; round < 10000; ++round) {
    while (ring.TryPush(next_push)) ++next_push;  // fill
    uint64_t v;
    while (ring.TryPop(v)) {
      EXPECT_EQ(v, expect);
      ++expect;
    }  // drain
  }
  EXPECT_EQ(next_push, expect);  // everything pushed was popped
}

TEST_F(SPSCRingTest, InterleavedPushPop_PartialDrain) {
  // Push 3, pop 2, push 3 more — indices march past Capacity without ever full-draining.
  uint64_t push = 0, expect = 0;
  for (int round = 0; round < 5000; ++round) {
    for (int k = 0; k < 3 && ring.TryPush(push); ++k) ++push;
    uint64_t v;
    for (int k = 0; k < 2 && ring.TryPop(v); ++k) {
      EXPECT_EQ(v, expect);
      ++expect;
    }
  }
  uint64_t v;
  while (ring.TryPop(v)) {
    EXPECT_EQ(v, expect);
    ++expect;
  }
  EXPECT_EQ(push, expect);
}

//////////////////////////////////////////////////////////
// MARK: Layout — no false sharing, power-of-two enforced at compile time
//////////////////////////////////////////////////////////

TEST(SPSCRingLayout, CursorsOnSeparateCacheLines) {
  EXPECT_GE(sizeof(SPSCRing<uint64_t, 8>), 5u * 64u);
}

//////////////////////////////////////////////////////////
// MARK: Single-threaded stress (fixed seed, reproducible)
//////////////////////////////////////////////////////////

TEST(SPSCRingStress, RandomInterleave_1M_StaysFifo) {
  SPSCRing<uint64_t, 1024> ring;
  std::mt19937_64 rng(0xC0FFEE);
  std::uniform_int_distribution<int> coin(0, 1);
  uint64_t push = 0, expect = 0;
  for (int step = 0; step < 1'000'000; ++step) {
    if (coin(rng)) {
      if (ring.TryPush(push)) ++push;
    } else {
      uint64_t v;
      if (ring.TryPop(v)) {
        EXPECT_EQ(v, expect);
        ++expect;
      }
    }
  }
  uint64_t v;
  while (ring.TryPop(v)) {
    EXPECT_EQ(v, expect);
    ++expect;
  }
  EXPECT_EQ(push, expect);
}

//////////////////////////////////////////////////////////
// MARK: Concurrent — producer thread + consumer thread
//////////////////////////////////////////////////////////

TEST(SPSCRingConcurrent, TwoThreads_AllItemsInOrder_TryApi) {
  SPSCRing<Payload, 1024> ring;
  constexpr uint64_t N = 2'000'000;
  std::atomic<bool> order_ok{true};

  std::thread consumer([&] {
    uint64_t expect = 0;
    for (uint64_t got = 0; got < N;) {
      Payload p;
      if (ring.TryPop(p)) {
        if (p.seq != expect || p.tag != expect * 2 + 1) order_ok.store(false);
        ++expect;
        ++got;
      }
    }
  });
  std::thread producer([&] {
    for (uint64_t i = 0; i < N;) {
      Payload p{i, i * 2 + 1};
      if (ring.TryPush(p)) ++i;
    }
  });
  producer.join();
  consumer.join();
  EXPECT_TRUE(order_ok.load());
}

}  // namespace
}  // namespace backtester
