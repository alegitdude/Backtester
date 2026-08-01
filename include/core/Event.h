#pragma once
#include <memory>
#include <string>

enum class EventType : uint8_t {
    kMarketOrderAdd, // 0
    kMarketOrderCancel,
    kMarketOrderModify,
    kMarketOrderClear,
    kMarketTrade,
    kMarketFill,
    kMarketNone,
    kMarketHeartbeat,

    kStrategySignal, // 8

    kStrategyOrderAdd,
    kStrategyOrderCancel,
    kStrategyOrderModify,
    kStrategyOrderClear,
    kStrategyOrderFill,
    kStrategyOrderRejection,

    kBacktestControlStart, // 14
    kBacktestControlEndOfDay,
    kBacktestControlSnapshot,
    kBacktestControlEndOfBacktest
};

enum class OrderSide : uint8_t { kBid, kAsk, kNone };

enum class OrderType { kAdd, kModify, kCancel, kClear };

enum class SignalType { kBuySignal, kSellSignal, kCancelSignal, kModifySignal };

enum class RejectionReason {
    kInvalidTick,
    kDrawdownLimit,
    kInsufficientBuyingPower,
    kNonTradableInstr,
    kPositionLimit,
    kNoOrderExists,
    kUnknownSignalType
};

//////////////////////////////////////////////////////////////
///////////// MARK: Base Event Header
//////////////////////////////////////////////////////////////

struct EventHeader { // 16
    uint64_t  timestamp;    // 8  ts_event
    EventType type;         // 1  tie-break key + dispatch tag
};

//////////////////////////////////////////////////////////////
///////////// MARK: MBO Event Class
//////////////////////////////////////////////////////////////

struct MarketByOrderEvent {  // 62
    EventHeader header;         // 16
    uint16_t    data_source_id; //  2  
    uint64_t    ts_recv;        //  8
    uint64_t    order_id;       //  8
    int64_t     price;          //  8
    uint32_t    size;           //  4
    uint32_t    sequence;       //  4
    uint32_t    instrument_id;  //  4
    int32_t     ts_in_delta;    //  4
    uint16_t    publisher_id;   //  2
    OrderSide   side;           //  1
    uint8_t     flags;          //  1
};


//////////////////////////////////////////////////////////////
///////////// MARK: Strategy Classes
//////////////////////////////////////////////////////////////

struct StrategySignalEvent { // 50
    EventHeader header;         // 16 ts, type
    uint16_t    strategy_id;    //  2
    int64_t     signal_id;      //  8
    uint32_t    instrument_id;  //  4 
    SignalType  signal_type;    //  4
    int64_t     price;          //  8
    int64_t     quantity;       //  8
};

struct StrategyOrderEvent { // 47
    EventHeader header;         // 16 ts, type
    uint16_t    strategy_id;    //  2
    int64_t     order_id;       //  8
    uint32_t    instrument_id;  //  4 
    OrderSide   side;           //  1
    int64_t     price;          //  8
    int64_t     quantity;       //  8
};

struct StrategyOrderRejectionEvent { // 54
    EventHeader     header;         // 16 ts, type
    uint16_t        strategy_id;    //  2
    int64_t         signal_id;      //  8
    uint32_t        instrument_id;  //  4 
    SignalType      signal_type;    //  4
    int64_t         price;          //  8
    int64_t         quantity;       //  8
    RejectionReason reason;         //  4
};

struct StrategyFillEvent { // 55
    EventHeader header;         // 16 ts, type
    uint16_t    strategy_id;    //  2
    int64_t     order_id;       //  8
    uint32_t    instrument_id;  //  4 
    OrderSide   side;           //  1
    int64_t     price;          //  8
    int64_t     quantity;       //  8
    int64_t     commission;  //  8
};

//////////////////////////////////////////////////////////////
///////////// MARK: Backtester Classes
//////////////////////////////////////////////////////////////

struct ControlEvent { EventHeader header; };

union EventUnion {
    MarketByOrderEvent mbo;
    StrategySignalEvent strat_signal_ev;
    StrategyOrderEvent strat_order_ev;
    StrategyOrderRejectionEvent strat_rej_ev;
    StrategyFillEvent strat_fill_ev;
    ControlEvent control_ev;
};

inline const EventHeader& Hdr(const EventUnion& e) noexcept { return e.mbo.header; }