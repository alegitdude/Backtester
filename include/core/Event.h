#pragma once
#include <iostream>
#include <memory>

enum EventType {
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
    KStrategyOrderRejection, 

    kBacktestControlStart, // 14
    kBacktestControlEndOfDay,
    kBacktestControlSnapshot,
    kBacktestControlEndOfBacktest
};

enum OrderSide {
    kBid,
    kAsk,
    kNone
};

enum OrderType {
    kAdd,
    kModify,
    kCancel,
    kClear
};

enum SignalType {
    kBuySignal,
    kSellSignal,
    kCancelSignal,
    kModifySignal
};

struct TradeRecord {
    uint64_t timestamp;
    std::string strategy_id;
    uint32_t instrument_id;
    OrderSide side;
    int64_t price;
    uint32_t quantity;
    int64_t realized_pnl;  
    int64_t commission;
};
//////////////////////////////////////////////////////////////
///////////// MARK: Base Event Class
//////////////////////////////////////////////////////////////

struct Event {
public:
    Event(long long ts, EventType t) : timestamp(ts), type(t) {}

    virtual ~Event() = default;
   
    long long timestamp; 
    EventType type;         
};

//////////////////////////////////////////////////////////////
///////////// MARK: MBO Event Class
//////////////////////////////////////////////////////////////

struct MarketByOrderEvent : Event {
 public:
    MarketByOrderEvent(
        uint64_t ts_event, 
        EventType type, // action
        uint64_t ts_recv,         
        uint16_t publisher_id, 
        uint32_t instrument_id,
        OrderSide side, 
        int64_t price, // converted
        uint32_t size,
        uint64_t order_id, 
        uint8_t flags, 
        int32_t ts_in_delta, 
        uint32_t sequence,
        std::string symbol,
        std::string data_source
    ) : Event(ts_event, type), // ts_event
        ts_recv(ts_recv), 
        publisher_id(publisher_id), 
        instrument_id(instrument_id),
        side(side), 
        price(price), 
        size(size),
        order_id(order_id), 
        flags(flags), 
        ts_in_delta(ts_in_delta), 
        sequence(sequence),
        symbol(symbol),
        data_source(data_source) {}

    virtual ~MarketByOrderEvent() = default;

    uint64_t ts_recv;      // Capture-server-received timestamp
    uint16_t publisher_id;  // Databento publisher ID
    uint32_t instrument_id; // Numeric instrument ID (Databento's ID for the asset)
    OrderSide side;         // raw char converted
    int64_t price;          // The order price as converted from 1e-9 units
    uint32_t size;          // The order quantity
    uint64_t order_id;      // The order ID assigned by the venue
    uint8_t flags;          // Bit field for event characteristics
    int32_t ts_in_delta;    // Matching-engine-sending timestamp relative to ts_recv
    uint32_t sequence;      // Sequence number
    std::string symbol;
    std::string data_source;

};

//////////////////////////////////////////////////////////////
///////////// MARK: OHLCV Event Class
//////////////////////////////////////////////////////////////

// class OhlcvEvent : Event {
//  public:
//   OhlcvEvent(
//     uint64_t ts_event, 	 // number of nanoseconds since the UNIX epoch.
//     uint8_t rtype, 	 	// 32 (OHLCV-1s), 33 (OHLCV-1m), 34 (OHLCV-1h), or 35 (OHLCV-1d). 
//     uint16_t publisher_id ,	 	
//     uint32_t instrument_id, 	 	
//     int64_t open, 	 	//The open price for the bar where every 1 unit corresponds to 1e-9, i.e. 1/1,000,000,000 or 0.000000001. See Prices.
//     uint64_t high, 	 	
//     uint64_t low, 	 	
//     uint64_t close, 	 	
//     uint64_t volume, 	
//     ) : Event(ts_event, )
//  private: 
// }



//////////////////////////////////////////////////////////////
///////////// MARK: Strategy Classes
//////////////////////////////////////////////////////////////

struct StrategySignalEvent : Event {
 public:
    StrategySignalEvent(
        uint64_t ts, 
        EventType event_type,
        int32_t signal_id,
        std::string strategy_id,
        uint32_t instrument_id, 
        SignalType signal_type, 
        int64_t price,
        uint32_t quantity
    ) : Event(ts, EventType::kStrategySignal), 
        signal_id(signal_id),
        strategy_id(strategy_id),
        instrument_id(instrument_id), 
        signal_type(signal_type), 
        price(price),
        quantity(quantity) {}
    
    ~StrategySignalEvent() = default;   

    int32_t signal_id;
    std::string strategy_id;
    uint32_t instrument_id;
    SignalType signal_type; // BUY, SELL, Cancel, Mod
    int64_t price; // Optional? for limit orders
    uint32_t quantity;
};

struct StrategyOrderEvent : Event {
    StrategyOrderEvent(
        uint64_t ts,  
        EventType event_type, 
        int32_t order_id, 
        uint32_t instrument_id,  
        OrderSide side, 
        int64_t price, 
        uint32_t qty,       
        std::string strategy_id
    ) : Event(ts, event_type), 
        order_id_(order_id),
        instrument_id_(instrument_id), 
        side_(side),
        price_(price), 
        quantity_(qty),     
        strategy_id_(strategy_id){}
    
    ~StrategyOrderEvent() {}

    int32_t order_id_;
    uint32_t instrument_id_;
    OrderType order_type_; // Add, Modify, Cancel, Clear
    OrderSide side_;
    int64_t price_;
    long quantity_;
    std::string strategy_id_;
};

struct FillEvent : Event {
    FillEvent(
        uint64_t timestamp,
        const std::string& strategy_id,
        int32_t order_id,
        uint32_t instrument_id,
        OrderSide side,
        int64_t fill_price,     
        uint32_t fill_quantity,       
        int64_t commission = 0
    ) : Event(timestamp, EventType::kStrategyOrderFill),
        strategy_id(std::move(strategy_id)),
        instrument_id(instrument_id),
        side(side),
        fill_price(fill_price),
        fill_quantity(fill_quantity),
        order_id(order_id),
        commission(commission) {}

    int32_t order_id;
    std::string strategy_id;
    uint32_t instrument_id;
    OrderSide side;
    int64_t fill_price;
    uint32_t fill_quantity;
    double commission;
};

//////////////////////////////////////////////////////////////
///////////// MARK: Backtester Classes
//////////////////////////////////////////////////////////////
    // kBacktestControlStart,
    // kBacktestControlEndOfDay,
    // kBacktestControlEndOfBacktest,
    // kBacktestControlSnapshot