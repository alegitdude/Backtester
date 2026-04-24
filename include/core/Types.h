#pragma once
#include "../data_ingestion/CsvZstReader.h"
#include "Event.h"
#include <iostream>
#include <memory>
#include <vector>
#include <limits>
#include <cstdint>

namespace backtester {
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)       __builtin_expect(!!(x), 1)
    #define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)       (x)
    #define UNLIKELY(x)     (x)
#endif

static constexpr auto kUndefPrice = std::numeric_limits<std::int64_t>::max();

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
    std::size_t max_lob_lvl; // If strat only needs current price, lvl is 1, 
                     // otherwise how many levels strat is doing calculations on
};

struct TradedInstrument {
    uint32_t instrument_id;
    InstrumentType instrument_type;
    uint64_t tick_size;
    uint64_t tick_value;
    uint64_t init_margin_req;
    uint64_t main_margin_req;
};

struct CommissionStruct {
    uint64_t fut_per_contract = 2'170'000'000;
    uint64_t stock_order_min = 350'000'000;
    uint64_t stock_per_share = 3'500'000;
    uint64_t stock_clearing_fee = 200'000;
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
    uint64_t execution_latency_ms = 200;
    uint64_t snapshot_interval_ns = 1'000'000'000;
    uint64_t initial_cash = 100000;
    CommissionStruct commission_struct;
    double risk_free_rate = 0.05;
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
    std::string strategy_id = "";
    int32_t last_order_id = 0;
    int64_t quantity = 0;
    int64_t avg_entry_price = 0;
    uint64_t last_update_ts = 0;

    double UnrealizedPnL(double currentPrice){
        return quantity * (currentPrice - avg_entry_price);
    }
    bool IsFlat() const { return quantity == 0; }
    bool IsLong() const { return quantity > 0; }
    bool IsShort() const { return quantity < 0; }

    bool operator==(const Position& pos2) const {
        return  instrument_id == pos2.instrument_id &&
                quantity == pos2.quantity &&
                avg_entry_price == pos2.avg_entry_price &&
                last_update_ts == pos2.last_update_ts;
    }
};

struct PriceLevel {
    int64_t price = kUndefPrice;
    uint32_t size = 0;
    uint32_t count = 0;
};

struct BidAskPair {
    PriceLevel bid;
    PriceLevel ask;

    bool operator==(const BidAskPair& bap2) const {
        return bid.price == bap2.bid.price &&
                ask.price == bap2.ask.price &&
                bid.size == bap2.bid.size &&
                ask.size == bap2.ask.size &&
                bid.count == bap2.bid.count &&
                ask.count == bap2.ask.count;
    }
};

struct TimeParseResult {
    uint64_t unix_nanos = 0;
    std::string error_msg = "";
    bool success = false;
};

struct LastTrade {
    uint64_t timestamp;
    int64_t price;
    uint32_t size;
    OrderSide aggressor_side;
};

struct MarketSnapshot {
    uint32_t instrument_id = 0;
    BidAskPair bbo = {};
    int64_t wmp = 0;
    int64_t vwap = 0;
    LastTrade last_trade = {};
    int64_t session_high = 0;
    int64_t session_low = kUndefPrice;
    int64_t cumulative_volume = 0;
    int64_t cumulative_notional = 0;
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
    int64_t quantity;
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