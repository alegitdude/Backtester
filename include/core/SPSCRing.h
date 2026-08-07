#pragma once
#include <array>
#include <atomic>
#include <cstddef>

namespace backtester {

template <class T, size_t Capacity>
class SPSCRing {
 public:
  bool TryPush(const T& v) {
    const size_t w = write_idx_.load(std::memory_order_relaxed);
    const size_t next = w + 1;
    if (next - cached_read_ > Capacity) {
      cached_read_ = read_idx_.load(std::memory_order_acquire);
      if (next - cached_read_ > Capacity) return false;
    }
    slots_[w & kMask] = v;
    write_idx_.store(next, std::memory_order_release);
    return true;
  };

  bool TryPop(T& out) {
    const size_t r = read_idx_.load(std::memory_order_relaxed);
    if (r == cached_write_) {
      cached_write_ = write_idx_.load(std::memory_order_acquire);
      if (r == cached_write_) return false;
    }
    out = slots_[r & kMask];
    read_idx_.store(r + 1, std::memory_order_release);
    return true;
  };

 private:
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
  static constexpr size_t kMask = Capacity - 1;
  static constexpr size_t kCacheLine = 64;
  alignas(kCacheLine) std::array<T, Capacity> slots_{};
  alignas(kCacheLine) std::atomic<size_t> write_idx_{0};
  alignas(kCacheLine) size_t cached_read_{0};  // producer-only copy of read_idx_
  alignas(kCacheLine) std::atomic<size_t> read_idx_{0};
  alignas(kCacheLine) size_t cached_write_{0};  // consumer-only copy of write_idx_
};

}  // namespace backtester