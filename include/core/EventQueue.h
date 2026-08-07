#pragma once
#include <optional>
#include <queue>

#include "Event.h"
#include "Types.h"

namespace backtester {

struct EventComparator {
  bool operator()(const EventUnion& a, const EventUnion& b) const noexcept {
    const EventHeader& ha = Hdr(a);
    const EventHeader& hb = Hdr(b);
    if (ha.timestamp != hb.timestamp) return ha.timestamp > hb.timestamp;
    return ha.type > hb.type;  // tie-break
  }
};

class EventQueue {
 public:
  EventQueue() {}

  ~EventQueue() {}

  void PushEvent(const EventUnion& event);

  bool IsEmpty() const;

  const EventUnion& ReadTopEvent() const;

  EventUnion PopTopEvent();

  size_t size() const;

  void clear();

 private:
  std::vector<EventUnion> pq_;
  EventComparator comparator_;
};

}  // namespace backtester