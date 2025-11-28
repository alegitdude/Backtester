#include "DataReaderManager.h"
#include "../core/Types.h"
#include "spdlog/spdlog.h"
#include "../core/EventQueue.h" 
#include <filesystem>
#include <charconv>

namespace backtester{

// MARK: Register&InitSteams

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

        if(!reader->Open(path)){
            std::string failure = "Failed to open reader for: " + symbol;
            std::cerr << failure << std::endl;
            spdlog::error(failure + "at " + path);
            return false;
        }   

        // Verify Header
        std::string header_line;
        reader->ReadLine(header_line);

        if(header_line != kExpectedMboHeader){
            std::string failure = "Incorrect header format for " + symbol;
            std::cerr << failure << std::endl;
            spdlog::error(failure + "at " + path);
            return false;
        }

        // 3. Store the active reader
        readers_[symbol] = {std::move(reader), source};

        // 4. Load the very first event to start the queue
        if(!LoadNextEventForSymbol(symbol)) 
             spdlog::warn("Symbol " + symbol + " has no events.");
    }

    std::cout << "Data readers initialized" << std::endl;;      
    return true;
}

// MARK: LoadNextEventForSymbol

bool DataReaderManager::LoadNextEventForSymbol(const std::string& symbol) {
    if(readers_.find(symbol) == readers_.end()){
        return false;  // Reader was already closed or not registered
    }       

    CsvZstReader& reader = *readers_[symbol].reader;
    std::string raw_line;

    if(!reader.ReadLine(raw_line)){
        spdlog::info("End of data for symbol: " + symbol);
        // clean up resources?? TODO reader.close()
        // readers_.erase(symbol);
        reader.Close(); 
        return false;
    }

    std::unique_ptr<MarketByOrderEvent> event_ptr;
    std::cout << "Yup" + std::to_string(static_cast<int>(readers_[symbol].config.schema)) << std::endl;
    if(readers_[symbol].config.schema == DataSchema::MBO){
        std::cout << raw_line << std::endl;
       event_ptr = ParseMboLineToEvent(symbol, raw_line);
    // } else if (readers_[symbol].schema == DataSchema::OHLCV){ // TODO
    //     event_ptr = ParseOhlcvLineToEvent(symbol, raw_line);
    } else {
        throw std::runtime_error("Invalid data schema ");
    }

    if(event_ptr != nullptr){
        event_queue_.PushEvent(std::move(event_ptr));
        return true;
    }
  
    return false;
};

// MARK:  ParseMboLineToEvent

std::unique_ptr<MarketByOrderEvent> DataReaderManager::ParseMboLineToEvent(
    const std::string& symbol, 
    const std::string& line) {

    DataSourceConfig config = readers_[symbol].config;

    std::string_view current_view(line);
    size_t pos = 0;

    uint64_t ts_recv, ts_event, order_id;
    uint32_t instrument_id, size, sequence;
    uint16_t publisher_id; 
    EventType action;
    OrderSide side;
    uint8_t flags;
    int32_t ts_in_delta; 
    int64_t price;

    for (int i = 1; i <= 15; ++i) { 
        std::string_view token = GetNextToken(pos, current_view);

        switch (i) {
            case 1: // ts_recv (uint64_t)
                if (token.empty()) throw std::runtime_error("Field 1 empty."); /////// TODO iso or unix conditional
                std::from_chars(token.data(), token.data() + token.size(), ts_recv);
                break;

            case 2: // ts_event (uint64_t)
                if (token.empty()) throw std::runtime_error("Field 2 empty."); /////// TODO iso or unix conditional
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
                if(action == EventType::kMarketOrderClear) break;
                if (token.empty()) throw std::runtime_error("Field 8 empty.");

                if(readers_[symbol].config.price_format == PriceFormat::DECIMAL){
                    double raw_price;
                    std::from_chars(token.data(), token.data() + token.size(), raw_price);
                    raw_price *= 100;
                    price = raw_price;
                    break;
                } else {
                    uint32_t raw_price;
                    std::from_chars(token.data(), token.data() + token.size(), raw_price);
                    price = raw_price;
                    break;
                }
                
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

// MARK: Get Next Token

std::string_view DataReaderManager::GetNextToken(size_t& start_pos, 
                                               std::string_view& current_view) {
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


}
