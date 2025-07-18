#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <charconv>
#include <ctime>
#include <chrono>
#include <memory>
#include <cmath>

enum DataInterval{
    MBO,
    Seconds,
    Minutes,
    Hours,
    Days
};

struct Position {
    double entryPrice;
    long entryTime;
    int quantity;

    double UnrealizedPnL(double currentPrice){
        return quantity * (currentPrice - entryPrice);
    }
};

struct Trade {
    double entryPrice, exitPrice;
    long entryTime, exitTime;
    int quantity;
};

struct DataBentoOHCLV1sRow{
    std::string tsEvent;
    int rtype;
    int publisherId;
    int instrumentId;


};


struct OHLCBar {
    int64_t timestamp;
    uint32_t instrumentId;
    double open, high, low, close, volume;
    
}; 

/// For OHLCV data
class DataLoader{
    private:
        static int64_t nanosToSeconds(int64_t nanos){
            return nanos / 1'000'000'000;
        };

    public:
        static int64_t parseDbIsoFormat(const char* str){
            // Assumed format of 2025-06-25T08:00:10.000000000Z --DB has same format for every OHLCV
            //                   YYYY-MM-DDTHH:MM:SS.sssz

            // Convert all datetime str values to ints
            int year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + 
               (str[2] - '0') * 10 + (str[3] - '0');
            int month = (str[5] - '0') * 10 + (str[6] - '0');
            int day = (str[8] - '0') * 10 + (str[9] - '0');
            int hour = (str[11] - '0') * 10 + (str[12] - '0');
            int minute = (str[14] - '0') * 10 + (str[15] - '0');
            int second = (str[17] - '0') * 10 + (str[18] - '0');

            // Convert to tm struct
            std::tm tm = {};
            tm.tm_year = year - 1900;  // tm_year is years since 1900
            tm.tm_mon = month - 1;      // tm_mon is 0-11
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;

            return timegm(&tm); 
        }

        static std::vector<OHLCBar> loadCsv(const std::string& filePath, DataInterval& interval){
            //Try to open the file
            FILE* file = fopen(filePath.c_str(), "r");
            //Check if the filepath is real and openable + opened
            if (!file){
                throw std::runtime_error("Failed to open file at:" + filePath);
            }

            //Declare return object
            std::vector<OHLCBar> bars;
            //Pre allocate the amount of time in the bars
            //Seconds for one day 234000, TODO generalize later
            bars.reserve(234000);

            //Skip headers
            char line[512];
            fgets(line, sizeof(line), file);  // Skip header
        
            while (fgets(line, sizeof(line), file)) {       
                OHLCBar bar;
                char ts_event[64];
                // Skip rtype, publisher_id, instrument_id
                if (sscanf(line, "%63[^,],%*d,%*d,%*d,%lf,%lf,%lf,%lf,%lf",
                        ts_event,
                        &bar.open,
                        &bar.high,
                        &bar.low,
                        &bar.close,
                        &bar.volume) == 6) {
                    
                        bar.timestamp = parseDbIsoFormat(ts_event);
                        bars.push_back(bar);
                    }
               
            }   

            fclose(file);
            return bars;     
        };
    };

std::string GetCSVTestEnvVar () {
     std::ifstream file(".env");

    if (!file.is_open()) {
        std::cerr << "Error: Could not open environment variable file:";
        return "";
    }
    std::string csvFilePath;
    std::string line;
    while (std::getline(file, line)) {
        size_t equalsPos = line.find('=');
        if (equalsPos != std::string::npos) {
            std::string key = line.substr(0, equalsPos - 1);
            std::string value = line.substr(equalsPos + 2);
            if(key == "PATH_TO_QQQ_OHLCV_CSV"){
                return value;
            }
        }
    }
    file.close();
    return "";
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


int main(int argc, char* argv[]) {

    ///  Argument Parsing & Initial Setup 
    
    ///  Initialize Logger 

    ///  Configuration Loading 

    ///  Component Initialization 

    ///  Create central EventQueue

    /// Initialize Data Reader(s) and possibly DataReaderManager?

    /// Initialize Market State Manager

    /// Initialize Portfolio Manager

    /// Initialize Report Generator

    /// Initialize Execution Handler

    /// Initialize Strategy Manager and specific Strategy


    /// Populate Initial Events (Pre-warm the queue) 
    // Read the first event from each data source and add to the EventQueue
    // (This is handled internally by DataReaderManager's interaction with EventQueue)
    // Logger::info("Populating initial events from data sources...")
    // data_reader_manager.populate_initial_events(event_queue) // This method reads one event from each reader and pushes to event_queue

    //  Main Backtest Loop 
    // Logger::info("Starting backtest loop...")
    // current_time_ns = config.start_time

    // WHILE NOT event_queue.is_empty() AND event_queue.top_event().timestamp <= config.end_time DO
    
      /// Event Dispatching 
   

    ///    END IF
         // End Event Dispatching

        // After processing, try to load next event from the source that provided the just-processed event
        // This ensures the data_reader_manager always tries to keep its internal buffers topped up.
        //data_reader_manager.try_load_next_event_for_source(event_queue, current_event.source_symbol_id)

       
    ///  End Backtest Loop

    ///  Logger::info("Backtest loop finished.")

    ///  Final Reporting & Cleanup 

    // // Clean up dynamically allocated objects (if not using smart pointers everywhere)
    // DELETE event_queue
    // DELETE data_reader_manager
    // DELETE market_state_manager
    // DELETE portfolio_manager
    // DELETE report_generator
    // DELETE execution_handler
    // DELETE strategy_manager
    // DELETE strategy // Specific strategy instance

    // Logger::info("Backtester shutting down.")
    return 0; 
    // Success
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

