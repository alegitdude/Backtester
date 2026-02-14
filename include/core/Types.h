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

enum class InstrumentType {
    FUT,
    STOCK,
    OPTION
};

enum class RiskMode {
    PosSizeInDollars,    // max_position_size in dollars
    PercentOfAcct       // max pct of account per trade
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

struct Strategy {
    std::string name;
    std::vector<int> params;
    int max_lob_lvl; // If strat only needs current price, lvl is 1, 
                     // otherwise how many levels strat is doing calculations on
};

struct TradedInstrument {
    uint32_t instrument_id;
    InstrumentType instrument_type;
    int64_t tick_size;
    int64_t tick_value;
    int64_t margin_req;
};

struct RiskLimits {
    RiskMode risk_mode;
    
    // Position-based limits (used if mode == PositionLimit)
    int64_t max_position_size = 0;            // Max contracts/shares per instrument, 0 = no limit
    
    // Risk-based limits (used if mode == RiskPercent)
    int64_t max_risk_per_trade_pct = 20'000'000;     // Max 2% of equity at risk per trade
    
    // Always enforced
    int64_t max_portfolio_delta = 0;         // Max total absolute delta, 0 = no limit
    int64_t max_drawdown_pct = 10'000'000;           // 10% circuit breaker
    int64_t max_delta_per_trade = 50000;     // Max dollar delta added per trade
};

struct AppConfig {
    uint64_t start_time; //Expected: YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ
    uint64_t end_time;  //Expected: YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ
    uint32_t execution_latency_ms = 200;
    int64_t initial_cash = 100000;
    std::string log_file_path;
    std::string report_output_dir;
    std::vector<Strategy> strategies;
    uint32_t max_lob_lvl;
    std::vector<TradedInstrument> traded_instruments;
    RiskLimits risk_limits;
    std::vector<DataSourceConfig> data_configs;
    std::vector<uint32_t> active_instruments; 
};

struct Position {
    uint32_t instrument_id = 0;
    int64_t quantity = 0;   // Signed: positive = long, negative = short
    int64_t avg_entry_price = 0;
    uint64_t last_update_ts = 0;

    double UnrealizedPnL(double currentPrice){
        return quantity * (currentPrice - avg_entry_price);
    }
    bool IsFlat() const { return quantity == 0; }
    bool IsLong() const { return quantity > 0; }
    bool IsShort() const { return quantity < 0; }
};

enum class DataInterval{
    MBO,
    Seconds,
    Minutes,
    Hours,
    Days
};


struct Trade {
    int64_t entryPrice, exitPrice;
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