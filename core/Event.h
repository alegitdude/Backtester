#pragma once
#include <iostream>
#include <memory>

enum EventType {
    kMarketOrderAdd, // 0
    KMarketOrderExecuted,
    kMarketOrderCancel,
    kMarketOrderModify,
    kMarketOrderClear,
    kMarketTrade,
    kMarketFill,
    kMarketNone,
    kMarketHeartbeat, 

    kStrategySignal, // 9
    kStrategyOrderSubmit,  
    kStrategyOrderCancel,
    kStrategyOrderModify,
    kStrategyOrderFill,  
    KStrategyOrderRejection, 

    kBacktestControlStart, // 15
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
    kFill,
    kRejected
};

enum SignalType {
    kBuySignal,
    kSellSignal
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
        uint64_t ts_recv, 
        EventType type, // action
        uint64_t ts_event, 
        uint16_t publisher_id, 
        uint32_t instrument_id,
        OrderSide side, 
        double price, // converted
        uint32_t size,
        uint64_t order_id, 
        uint8_t flags, 
        int32_t ts_in_delta, 
        uint32_t sequence,
        std::string symbol
    ) : Event(ts_recv, type), // ts_recv as primary timestamp for Event 
        ts_event(ts_event), 
        publisher_id(publisher_id), 
        instrument_id(instrument_id),
        side(side), 
        price(price), 
        size(size),
        order_id(order_id), 
        flags(flags), 
        ts_in_delta(ts_in_delta), 
        sequence(sequence),
        symbol(symbol) {}

    virtual ~MarketByOrderEvent() = default;

    uint64_t ts_event;      // Capture-server-received timestamp
    uint16_t publisher_id;  // Databento publisher ID
    uint32_t instrument_id; // Numeric instrument ID (Databento's ID for the asset)
    OrderSide side;         // raw char converted
    double price;           // The order price as converted from 1e-9 units
    uint32_t size;          // The order quantity
    uint64_t order_id;      // The order ID assigned by the venue
    uint8_t flags;          // Bit field for event characteristics
    int32_t ts_in_delta;    // Matching-engine-sending timestamp relative to ts_recv
    uint32_t sequence;      // Sequence number
    std::string symbol;

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

class StrategySignalEvent : Event {
 public:
    StrategySignalEvent(
        long long ts, 
        const std::string& sym, 
        SignalType s_t, 
        double price
    ) : Event(ts, EventType::kStrategySignal), 
        symbol_(sym), 
        signal_type_(s_t), 
        suggested_price_(price) {}
    
    ~StrategySignalEvent() {}    

    std::string get_symbol() const { return symbol_; }
    SignalType get_signal_type() const { return signal_type_ ;}

 private:
    std::string symbol_;
    SignalType signal_type_; // BUY, SELL, Maybe? HEDGE?
    double suggested_price_; // Optional, for limit orders
     // long suggested_quantity; // Optional
};

class StrategyOrderEvent : Event {
 public:
    StrategyOrderEvent(
        long long ts,  
        EventType e_t, 
        const std::string& symbol, 
        OrderType ot, 
        OrderSide side, 
        double price, 
        long qty, 
        uint64_t id, 
        std::string st_id
    ) : Event(ts, e_t), 
        symbol_(symbol), 
        order_type_(ot), 
        side_(side),
        price_(price), 
        quantity_(qty),
        order_id_(id),
        strategy_id_(st_id){}
    
    ~StrategyOrderEvent() {}

    std::string get_symbol() { return symbol_; }
    OrderType get_order_type() { return order_type_; }
    OrderSide get_side() { return side_; }
    double get_price() { return price_; }
    long get_quantity() { return quantity_; }
    uint64_t get_order_id() { return order_id_; }
    std::string get_strategy_id() { return strategy_id_; }

 private:
    std::string symbol_;
    OrderType order_type_; // Add, Modify, Cancel, Fill, Rejection
    OrderSide side_;
    double price_;
    long quantity_;
    uint64_t order_id_;
    std::string strategy_id_;
};

//////////////////////////////////////////////////////////////
///////////// MARK: Backtester Classes
//////////////////////////////////////////////////////////////
    // kBacktestControlStart,
    // kBacktestControlEndOfDay,
    // kBacktestControlEndOfBacktest,
    // kBacktestControlSnapshot