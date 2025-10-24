#pragma once
#include "Event.h"
#include <queue>

struct EventComparator {
    bool operator()(
        const std::unique_ptr<Event>& a, 
        const std::unique_ptr<Event>& b) const {
        // Primary sort: timestamp (ascending)
        if(a->get_timestamp() != b->get_timestamp()) {
            return a->get_timestamp() < b->get_timestamp(); // For MIN-heap, "greater" means lower priority
        }        
        // Market -> Strategy -> Backtest
        return a->get_type() < b->get_type(); 
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