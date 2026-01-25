#pragma once
#include "../data_ingestion/CsvZstReader.h"
#include <iostream>
#include <memory>
#include <vector>

namespace backtester {
    
enum class DataSchema {
    MBO,
    OHLCV
};

enum class Encoding {
    DBN,
    CSV,
    JSON
};

enum class Compression {
    ZSTD,
    NONE
};

enum class PriceFormat {
    FIXPNTINT,
    DECIMAL
};

enum class TmStampFormat {
    UNIX,
    ISO
};

struct Symbol {
    std::string symbol;
    uint32_t instrument_id;
    std::string date;
};

struct DataSourceConfig {
    std::string data_source_name; // can be anything but needed to differentiate sources
    std::vector<Symbol> data_symbology;
    std::string data_filepath;
    DataSchema schema;
    Encoding encoding;
    Compression compression;
    PriceFormat price_format;
    TmStampFormat ts_format;
};

struct DataStream {
    std::unique_ptr<CsvZstReader> reader;
    DataSourceConfig config;
};

struct AppConfig {
    long long start_time; //Expected: YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ
    long long end_time;  //Expected: YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ
    int execution_latency = 500000;
    int initial_cash = 100000;
    std::string log_file_path;
    std::string report_output_dir;
    std::string strategy_name = "defaultStrat";
    // strategy_params: {
    //     "some_param": "5"
    // },
    std::string traded_symbol;
    // symbol, filepath, schema
    std::vector<DataSourceConfig> data_streams;
    std::vector<uint32_t> active_instruments; 

};

enum class DataInterval{
    MBO,
    Seconds,
    Minutes,
    Hours,
    Days
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

}