#include "core/EventQueue.h"
#include <iostream>
#include <queue>

namespace backtester {

    void EventQueue::PushEvent(const EventUnion& event) {
        pq_.push_back(event);
        std::push_heap(pq_.begin(), pq_.end(), comparator_);
    }

    bool EventQueue::IsEmpty() const {
        return pq_.empty();
    }

    const EventUnion& EventQueue::ReadTopEvent() const {
        if (pq_.empty()) {
            throw std::out_of_range("Attempted to grab top event of empty queue");
        }
        return pq_.front();
    }

    EventUnion EventQueue::PopTopEvent() {
        if (pq_.empty()) {
            if (BT_UNLIKELY (pq_.empty())) {
                throw std::out_of_range("Attempted to pop from empty queue");
            } 
        }
        std::pop_heap(pq_.begin(), pq_.end(), comparator_);
        EventUnion top_event = pq_.back();
        pq_.pop_back();
        return top_event;
    }

    size_t EventQueue::size() const {
        return pq_.size();
    }

    void EventQueue::clear() {
        pq_ = std::vector<EventUnion>();
    }

}