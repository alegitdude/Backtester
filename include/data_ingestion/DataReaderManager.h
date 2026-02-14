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
    explicit DataReaderManager(EventQueue& queue) : event_queue_(queue) {}

    bool RegisterAndInitStreams(const std::vector<DataSourceConfig>& file_paths);
    bool LoadNextEventFromSource(const std::string& source_name);

 private:
    std::unordered_map<std::string, DataStream>readers_;
    EventQueue& event_queue_;
    
    std::unique_ptr<MarketByOrderEvent> ParseMboLineToEvent(
        const std::string& symbol, 
        const std::string& line
    );

    uint64_t ParseIsoToUnix(std::string str);

    std::string_view GetNextToken(size_t& start_pos, std::string_view& current_view);

    // std::unique_ptr<Event> ParseOhlcvLineToEvent(  // TODO
    //     const std::string& symbol, 
    //     const std::string& line
    // ); 

    inline OrderSide CharToOrderSide(char side) {
        if(side == 'A'){
            return OrderSide::kAsk;
        }
        if(side == 'B'){
            return OrderSide::kBid;
        }
        else {
            return OrderSide::kNone;
        }  
    }  

    inline EventType ActionToEventTyp(char act) {
        if(act == 'A'){
            return EventType::kMarketOrderAdd;
        }
        if(act == 'M'){
            return EventType::kMarketOrderModify;
        }
        if(act == 'C'){
            return EventType::kMarketOrderCancel;
        }
        if(act == 'R'){
            return EventType::kMarketOrderClear;
        }
        if(act == 'T'){
            return EventType::kMarketTrade;
        }
        if(act == 'F'){
            return EventType::kMarketFill;
        }
        else {
            return EventType::kMarketNone;
        }
    }
};

}