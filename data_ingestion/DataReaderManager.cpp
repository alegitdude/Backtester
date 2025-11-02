#include "DataReaderManager.h"
#include "../core/Types.h"
#include "spdlog/spdlog.h"
#include "core/EventQueue.h" 
#include <filesystem>

bool DataReaderManager::register_and_init_streams(
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
            std::cout << "Made it here error" + path<< std::endl;

            std::string failure = "Failed to open reader for: " + symbol;
            spdlog::error("Failed to open reader for: " + symbol + "at " + path);
            return false;
        }     

        // 3. Store the active reader
        readers_[symbol] = {std::move(reader), source.format};

        // 4. Load the very first event to start the queue
        if(!load_next_event_for_symbol(symbol)) 
             spdlog::warn("Symbol " + symbol + " has no events.");
    }

    std::cout << "Data readers initialized" << std::endl;;      
    return true;
}

bool DataReaderManager::load_next_event_for_symbol(const std::string& symbol){
    if(readers_.find(symbol) == readers_.end()){
        return false;  // Reader was already closed or not registered
    }       

    CsvZstReader& reader = *readers_[symbol].reader;
    std::string raw_line;

    // 1. Read the next line from the CSV/ZST stream
    if(!reader.readLine(raw_line)){
        spdlog::info("End of data for symbol: " + symbol);
        // clean up resources??
        // readers_.erase(symbol); 
    }
    // 2. Parse and Convert the line into a C++ Event object
    std::unique_ptr<MarketByOrderEvent> event_ptr;
    if(readers_[symbol].format == DataFormat::MBO){
        event_ptr = parse_mbo_line_to_event(symbol, raw_line);
    } else if (readers_[symbol].format == DataFormat::OHLCV){
        event_ptr = parse_ohlcv_line_to_event(symbol, raw_line);
    } else {
        throw std::runtime_error("Invalid data format " +  readers_[symbol].format);
    }

    if(event_ptr != nullptr){
        // 3. Push the new event into the central Priority Queue
        event_queue_.push_event(std::move(event_ptr));
        return true;
    }
  
    return false;
};

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
