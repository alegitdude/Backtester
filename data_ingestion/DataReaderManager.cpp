#include "DataReaderManager.h"
#include "../core/Types.h"
#include "spdlog/spdlog.h"
#include "../core/EventQueue.h" 
#include <filesystem>
#include <charconv>

namespace backtester{

bool DataReaderManager::RegisterAndInitStreams(
    const std::vector<DataSourceConfig>& data_sources) {
  
    for(DataSourceConfig source : data_sources){
        std::string symbol = source.symbol;       
        std::string path = source.filepath;      

        if (symbol.empty()) {
            std::cerr << "Error: Empty symbol" << std::endl;
            return false;
        }
        
        if (!std::filesystem::exists(path)) {
            std::cerr << "Error: File not found: " << path << std::endl;
            return false;
        }

        std::unique_ptr<CsvZstReader> reader = std::make_unique<CsvZstReader>();

        if(!reader->open(path)){
            std::string failure = "Failed to open reader for: " + symbol;
            std::cerr << failure << std::endl;
            spdlog::error(failure + "at " + path);
            return false;
        }   

        // Verify Header
        std::string header_line;
        reader->readLine(header_line);

        if(header_line != kExpectedMboHeader){
            std::string failure = "Incorrect header format for " + symbol;
            std::cerr << failure << std::endl;
            spdlog::error(failure + "at " + path);
            return false;
        }

        // 3. Store the active reader
        readers_[symbol] = {std::move(reader), source.schema, source.ts_format};

        // 4. Load the very first event to start the queue
        if(!LoadNextEventForSymbol(symbol)) 
             spdlog::warn("Symbol " + symbol + " has no events.");
    }

    std::cout << "Data readers initialized" << std::endl;;      
    return true;
}

bool DataReaderManager::LoadNextEventForSymbol(const std::string& symbol) {
    if(readers_.find(symbol) == readers_.end()){
        return false;  // Reader was already closed or not registered
    }       

    CsvZstReader& reader = *readers_[symbol].reader;
    TmStampFormat ts_type = readers_[symbol].ts_type;
    std::string raw_line;

    if(!reader.readLine(raw_line)){
        spdlog::info("End of data for symbol: " + symbol);
        // clean up resources?? TODO reader.close()
        // readers_.erase(symbol); 
    }

    std::unique_ptr<MarketByOrderEvent> event_ptr;

    if(readers_[symbol].schema == DataSchema::MBO){
       event_ptr = ParseMboLineToEvent(symbol, raw_line, ts_type);
    // } else if (readers_[symbol].schema == DataSchema::OHLCV){ // TODO
    //     event_ptr = ParseOhlcvLineToEvent(symbol, raw_line);
    } else {
        throw std::runtime_error("Invalid data schema ");
    }

    if(event_ptr != nullptr){
        event_queue_.push_event(std::move(event_ptr));
        return true;
    }
  
    return false;
};

std::unique_ptr<MarketByOrderEvent> DataReaderManager::ParseMboLineToEvent(
    const std::string& symbol, 
    const std::string& line,
    const TmStampFormat& ts_type ) {

    std::string_view current_view(line);
    size_t pos = 0;

    uint64_t ts_recv, ts_event, order_id;
    uint32_t instrument_id, size, sequence;
    uint16_t publisher_id; 
    EventType action;
    OrderSide side;
    uint8_t flags;
    int32_t ts_in_delta; 
    double price;

    for (int i = 1; i <= 14; ++i) { 
        std::string_view token = GetNextToken(pos, current_view);

        switch (i) {
            case 1: // ts_recv (uint64_t)
                if (token.empty()) throw std::runtime_error("Field 1 empty."); /////// TODO iso or unix conditional
                std::from_chars(token.data(), token.data() + token.size(), ts_recv);
                break;

            case 2: // ts_event (uint64_t)
                if (token.empty()) throw std::runtime_error("Field 2 empty.");
                std::from_chars(token.data(), token.data() + token.size(), ts_event);
                break;
                
            case 3: // rtype (uint16_t)
                break;

            case 4: // publisher_id (uint16_t)
                if (token.empty()) throw std::runtime_error("Field 4 empty.");
                std::from_chars(token.data(), token.data() + token.size(), publisher_id);
                break;

            case 5: // instrument_id (uint32_t)
                if (token.empty()) throw std::runtime_error("Field 5 empty.");
                std::from_chars(token.data(), token.data() + token.size(), instrument_id);
                break;
                
            case 6: // action (char)
                if (token.size() != 1) throw std::runtime_error("Field 6 malformed.");
                action = ActionToEventTyp(token[0]);
                break;
                
            case 7: // side (char)
                if (token.size() != 1) throw std::runtime_error("Field 7 malformed.");
                side = CharToOrderSide(token[0]);
                break;

            case 8: // price (int64_t)
                if (token.empty()) throw std::runtime_error("Field 8 empty.");
                uint32_t raw_price;
                std::from_chars(token.data(), token.data() + token.size(), raw_price);
                price = static_cast<double>(raw_price) * 1e-9;
                break;

            case 9: // size (uint32_t)
                if (token.empty()) throw std::runtime_error("Field 9 empty.");
                std::from_chars(token.data(), token.data() + token.size(), size);
                break;

            case 10: // channel_id (uint32_t)
                break;

            case 11: // order_id (uint64_t)
                if (token.empty()) throw std::runtime_error("Field 11 empty.");
                std::from_chars(token.data(), token.data() + token.size(), order_id);
                break;
            
            case 12: // flags (uint8_t)
                if (token.empty()) throw std::runtime_error("Field 12 empty.");
                std::from_chars(token.data(), token.data() + token.size(), flags);
                break;

            case 13: // ts_in_delta (int32_t)
                if (token.empty()) throw std::runtime_error("Field 13 empty.");
                std::from_chars(token.data(), token.data() + token.size(), ts_in_delta);
                break;

            case 14: // sequence (uint32_t)
                if (token.empty()) throw std::runtime_error("Field 14 empty.");
                std::from_chars(token.data(), token.data() + token.size(), sequence);
                break;

            case 15: // potentially symbol added
                break;

            default:
                // Should never happen if loop count is correct
                throw std::runtime_error("Unexpected field index.");
        }
    } 

    std::unique_ptr<MarketByOrderEvent> event_ptr = 
        std::make_unique<MarketByOrderEvent>(
            ts_recv, 
            action,
            ts_event, 
            publisher_id,
            instrument_id,
            side,
            price,
            size,
            order_id,
            flags,
            ts_in_delta,
            sequence,
            symbol
        );

    return event_ptr;
};

std::string_view DataReaderManager::GetNextToken(size_t& start_pos, std::string_view& current_view) {
    size_t delim_pos = current_view.find(',', start_pos);
    std::string_view token;

    if (delim_pos == std::string_view::npos) {
        // Last token in the line
        token = current_view.substr(start_pos);
        start_pos = current_view.size(); // Advance position to the end
    } else {
        // Found a token
        token = current_view.substr(start_pos, delim_pos - start_pos);
        start_pos = delim_pos + 1; // Advance past the comma
    }
    return token;
};

inline OrderSide DataReaderManager::CharToOrderSide(char side) {
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

EventType DataReaderManager::ActionToEventTyp(char act) {
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

inline int fast_atoi_2(const char* s) {
    return (s[0] - '0') * 10 + (s[1] - '0');
}

inline int fast_atoi_4(const char* s) {
    return (s[0] - '0') * 1000 +
           (s[1] - '0') * 100  +
           (s[2] - '0') * 10   +
           (s[3] - '0');
}

uint64_t DataReaderManager::ParseIsoToUnix(std::string str) {
    const char* s = str.c_str();

    struct tm t = {0};
    t.tm_year = fast_atoi_4(s) - 1900; // tm_year is years since 1900
    t.tm_mon  = fast_atoi_2(s + 5) - 1; // tm_mon is 0-11
    t.tm_mday = fast_atoi_2(s + 8);
    t.tm_hour = fast_atoi_2(s + 11);
    t.tm_min  = fast_atoi_2(s + 14);
    t.tm_sec  = fast_atoi_2(s + 17);
    t.tm_isdst = 0; // Not in daylight saving time 

    time_t epoch_seconds = timegm(&t);

    const char* p_nanos = s + 20; 
    uint32_t nanos = 0;

    for (int i = 0; i < 9; ++i) {
        nanos = nanos * 10 + (p_nanos[i] - '0');
    }
    return static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL + nanos;
}

// std::unique_ptr<Event> DataReaderManager::parse_line_to_event(
//     const std::string& symbol, 
//     const std::string& line
// ) {
//     std::stringstream ss(line);
//     uint64_t ts_recv, ts_event; 
//     char action, side_char;

//     // Example simplified parsing
//     // ss >> ts_recv >> ts_event >> action >> side_char >> ...
//     // Note: CSV parsing requires careful tokenizing to handle all fields correctly.

//     // 2. Create the Event
//     std::unique_ptr<MarketRecordEvent> event_ptr = 
//         std::make_unique<MarketRecordEvent>(
//             ts_recv, 
//             EventType::MARKET_RECORD_EVENT, 
//             ts_event, 
//             // ... all other required raw parameters
//         );

//     // 3. Set the resolved symbol string on the event
//     event_ptr->set_symbol_str(symbol);

//     return event_ptr;
// }

}

// Input line: "1672531200000000000,1672531200000000000,1,1234567,A,..."
// std::string_view current_view(line);
// size_t pos = 0;
// size_t delim_pos = std::string_view::npos;
// std::string_view token;



// Now 'pos' is positioned at the start of the 6th field (side_char)
// ... continue parsing the rest of the fields using the same pattern