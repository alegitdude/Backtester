#pragma once
#include "../core/Event.h"
#include "CsvZstReader.h"
#include "../core/Types.h"
#include "../core/Constants.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace backtester {
class EventQueue;

class DataReaderManager {
 public:
    DataReaderManager(EventQueue& queue) : event_queue_(queue) {}

    bool RegisterAndInitStreams(const std::vector<DataSourceConfig>& file_paths);

    bool RefillEventQueueForSymbol(const std::string& symbol);

 private:
    std::unordered_map<std::string, DataStream> readers_;
    EventQueue& event_queue_;
    
    std::unique_ptr<MarketByOrderEvent> ParseMboLineToEvent(
        const std::string& symbol, 
        const std::string& line
    );
    std::string_view GetNextToken(size_t& start_pos, std::string_view& current_view);

    OrderSide CharToOrderSide(char side);

    EventType ActionToEventTyp(char act);

    // std::unique_ptr<Event> ParseOhlcvLineToEvent(  // TODO
    //     const std::string& symbol, 
    //     const std::string& line
    // );

    bool LoadNextEventForSymbol(const std::string& symbol);
};

}