#pragma once
#include <iostream>
#include <memory>

enum class DataFormat {
    MBO,
    OHLCV
};

struct DataStream {
    std::unique_ptr<CsvZstReader> reader;
    DataFormat format;
};

struct DataSourceConfig {
    std::string symbol;
    std::string filepath;
    DataFormat format;
};

struct Config {
    //std::string data_format;  //csv, csv.zst
    std::string start_time; //Expected: YYYY-MM-DD HH:MM:SS[.nnnnnnnnn]
    std::string end_time;  //Expected: YYYY-MM-DD HH:MM:SS[.nnnnnnnnn]
    int execution_latency = 500000;
    int initial_cash = 100000;
    std::string log_file_path;
    std::string report_output_dir;
    std::string strategy_name = "defaultStrat";
    // strategy_params: {
    //     "some_param": "5"
    // },
    std::string traded_symbol;
    // symbol, filepath, format
    std::vector<DataSourceConfig> data_streams;

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