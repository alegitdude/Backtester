#pragma once
#include "Event.h"
#include <queue>

namespace backtester {

struct EventComparator {
    bool operator()(
        const std::unique_ptr<Event>& a, 
        const std::unique_ptr<Event>& b) const {
        // Primary sort: timestamp (ascending)
        if(a->timestamp != b->timestamp) {
            return a->timestamp < b->timestamp; // For MIN-heap, "greater" means lower priority
        }        
        // Market -> Strategy -> Backtest
        return a->type < b->type; 
    }
};

class EventQueue {
 public:
    EventQueue() {}

    ~EventQueue() {}

    void push_event(std::unique_ptr<Event> event_ptr);

    bool is_empty() const ;

    const Event& top_event() const ;

    std::unique_ptr<Event> pop_top_event() ;

    size_t size() const ;

    // Clear all events from the queue (for resetting)
    void clear() ;

 private:
    std::vector<std::unique_ptr<Event>> pq_;
    EventComparator comparator_;
};

}