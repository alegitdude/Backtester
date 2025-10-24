#include "EventQueue.h"
#include <iostream>
#include <queue>
#include <memory>

void EventQueue::push_event(std::unique_ptr<Event> event_ptr) {
    if(event_ptr != nullptr){
        pq_.push_back(std::move(event_ptr)); 
        std::push_heap(pq_.begin(), pq_.end(), comparator_);
    }
}

bool EventQueue::is_empty() const {
    return pq_.empty();
}

const Event& EventQueue::top_event() const {
    if(pq_.empty()) {
        throw std::out_of_range("Attempted to grab top event of empty quque");     
    }        
    return *pq_.front();
}

std::unique_ptr<Event> EventQueue::pop_top_event() {
    if(pq_.empty()) {
        return std::unique_ptr<Event>{}; 
    }
    std::pop_heap(pq_.begin(), pq_.end(), comparator_);
    std::unique_ptr<Event> top_event_ptr = std::move(pq_.back()); 
    pq_.pop_back(); 
    return top_event_ptr;
}

size_t EventQueue::size() const {
    return pq_.size();
}

// Clear all events from the queue (for resetting)
void EventQueue::clear() {
    pq_ = std::vector<std::unique_ptr<Event>>();
}
