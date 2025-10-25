#include "DataReaderManager.h"
#include "spdlog/spdlog.h"
// #include "core/EventQueue.h" // Full definition needed here

bool DataReaderManager::register_and_init_streams(
    const std::vector<std::pair<std::string, std::string>>& file_paths) {

    for(std::pair<std::string, std::string> file_path : file_paths){
        std::string symbol = file_path.first;       
        std::string path = file_path.second; 
       
        std::unique_ptr<CsvZstReader> reader = std::make_unique<CsvZstReader>();
        if(!reader->open(path)){
            std::string failure = "Failed to open reader for: " + symbol;
            spdlog::error("Failed to open reader for: " + symbol + "at " + path);
            return false;
        }     
        // 3. Store the active reader
        readers_[symbol] = std::move(reader);

        // 4. Load the very first event to start the queue
        if(!load_next_event_for_symbol(symbol)) 
             spdlog::warn("Symbol " + symbol + " has no events.");
    }
          
    return true;
}

// // ----------------------------------------------------------------------
// // Core Refill Logic (Maintains the Invariant)
// // ----------------------------------------------------------------------

// BOOL DataReaderManager::refill_event_queue_for_symbol(CONST std::string& symbol) {
//     // Simply delegates to the helper function
//     RETURN load_next_event_for_symbol(symbol);
// }

// // ----------------------------------------------------------------------
// // Internal Helper to Read and Push ONE Event
// // ----------------------------------------------------------------------

// BOOL DataReaderManager::load_next_event_for_symbol(CONST std::string& symbol) {
//     IF readers_.find(symbol) IS readers_.end() THEN
//         // Reader was already closed or not registered
//         RETURN FALSE;
//     END IF

//     CsvZstReader& reader = *readers_[symbol];
//     std::string raw_line;

//     // 1. Read the next line from the CSV/ZST stream
//     IF (!reader.readLine(raw_line)) THEN
//         // End of file reached for this symbol
//         Logger::info("End of data for symbol: " + symbol);
//         // Optional: Remove reader from map to free resources
//         readers_.erase(symbol); 
//         RETURN FALSE;
//     END IF

//     // 2. Parse and Convert the line into a C++ Event object
//     std::unique_ptr<MarketRecordEvent> event_ptr = parse_line_to_event(symbol, raw_line);

//     IF (event_ptr IS NOT NULL) THEN
//         // 3. Push the new event into the central Priority Queue
//         event_queue_.push_event(std::move(event_ptr));
//         RETURN TRUE;
//     END IF

//     // Should rarely happen unless parsing fails mid-file
//     RETURN FALSE; 
// }


// // ----------------------------------------------------------------------
// // Parsing Implementation (Highly simplified)
// // ----------------------------------------------------------------------

// std::unique_ptr<MarketRecordEvent> DataReaderManager::parse_line_to_event(
//     CONST std::string& symbol, 
//     CONST std::string& line
// ) {
//     // This is where the complex CSV parsing logic goes, using std::stringstream
//     std::stringstream ss(line);
//     // Variables to hold parsed data
//     uint64_t ts_recv, ts_event; 
//     char action, side_char;
//     // ... many other variables corresponding to your MarketRecordEvent fields

//     // Example simplified parsing
//     // ss >> ts_recv >> ts_event >> action >> side_char >> ...
//     // Note: CSV parsing requires careful tokenizing to handle all fields correctly.

//     // 1. Resolve Instrument ID to Symbol (if using raw DBN, otherwise skip)
//     uint32_t instrument_id = symbology_service_.get_instrument_id(symbol);

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

//     RETURN event_ptr;
// }