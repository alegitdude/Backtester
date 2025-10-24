#pragma once
#include "../core/Event.h"
#include "CsvZstReader.h"
#include <unordered_map>
#include <string>
#include <memory>

class EventQueue;

class DataReaderManager {
 public:
    DataReaderManager(EventQueue& queue) : event_queue_(queue) {}

    bool register_and_init_streams(const std::vector<std::pair<std::string, std::string>>& file_paths);

    // Method called by the Backtester loop after consuming an event.
    bool refill_event_queue_for_symbol(const std::string& symbol);

 private:
    std::unordered_map<std::string, std::unique_ptr<CsvZstReader>> readers_;
    EventQueue& event_queue_;

    // Private helper to convert a raw CSV line into a MarketRecordEvent object
    std::unique_ptr<Event> parse_line_to_event(
        const std::string& symbol, 
        const std::string& line
    );

    bool load_next_event_for_symbol(const std::string& symbol);
};