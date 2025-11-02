#pragma once
#include "../core/Event.h"
#include "CsvZstReader.h"
#include "../core/Types.h"
#include <unordered_map>
#include <string>
#include <memory>

class EventQueue;

class DataReaderManager {
 public:
    DataReaderManager(EventQueue& queue) : event_queue_(queue) {}

    bool register_and_init_streams(const std::vector<DataSourceConfig>& file_paths);

    // Method called by the Backtester loop after consuming an event.
    bool refill_event_queue_for_symbol(const std::string& symbol);

 private:
    std::unordered_map<std::string, DataStream> readers_;
    EventQueue& event_queue_;
    
    std::unique_ptr<MarketByOrderEvent> parse_mbo_line_to_event(
        const std::string& symbol, 
        const std::string& line
    );

    std::unique_ptr<Event> parse_ohlcv_line_to_event(
        const std::string& symbol, 
        const std::string& line
    );

    bool load_next_event_for_symbol(const std::string& symbol);
};