#include <iostream>
#include <queue>
#include <memory>

enum EventType {
    kMarketOrderAdd,
    KMarketOrderExecuted,
    kMarketOrderCancel,
    kMarketOrderModify,
    kMarketTrade,
    kMarketHeartbeat, 

    kStrategySignal, // Sent to Order/Execution Management System type class 
    kStrategyOrderSubmit,  
    kStrategyOrderCancel,
    kStrategyOrderModify,
    kStrategyOrderFill,  
    KStrategyOrderRejection, 

    kBacktestControlStart,
    kBacktestControlEndOfDay,
    kBacktestControlEndOfBacktest,
    kBacktestControlSnapshot
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

class Event {
public:
    Event(long long ts, EventType t) : timestamp_ns(ts), type(t) {}

    virtual ~Event() {}
    
    long long get_timestamp() const { return timestamp_ns; }
    EventType get_type() const { return type; }

protected: 
    long long timestamp_ns; 
    EventType type;         

};

//////////////////////////////////////////////////////////////
///////////// MARK: MBO Class
//////////////////////////////////////////////////////////////

class MarketByOrderEvent : Event {
 public:
    MarketByOrderEvent(
        uint64_t ts_recv, 
        EventType type, // From Event base
        uint64_t ts_event, 
        uint16_t publisher_id, 
        uint32_t instrument_id,
        char action, 
        char side_char, 
        int64_t raw_price, 
        uint32_t size,
        uint64_t order_id, 
        uint8_t flags, 
        int32_t ts_in_delta, 
        uint32_t sequence
    )
    : Event(ts_recv, type), // Use ts_recv as the primary timestamp for Event base
      ts_event_(ts_event), 
      publisher_id_(publisher_id), 
      instrument_id_(instrument_id),
      action_(action), 
      side_char_(side_char), 
      raw_price_(raw_price), 
      size_(size),
      order_id_(order_id), 
      flags_(flags), 
      ts_in_delta_(ts_in_delta), 
      sequence_(sequence)
    {
        price_double_ = static_cast<double>(raw_price) * 1e-9; // Convert to double
        side_ = convert_char_to_OrderSide(side_char); // Helper function/enum conversion
        // symbol_str will need to be populated externally, e.g., via a lookup table
        // or passed in during parsing if you resolved instrument_id to a symbol
    }

    virtual ~MarketByOrderEvent() {}

    uint64_t get_ts_event() const { return ts_event_; }
    uint16_t get_publisher_id() const { return publisher_id_; }
    uint32_t get_instrument_id() const { return instrument_id_; }
    char get_action_char() const { return action_; } // Get raw char if needed
    char get_side_char() const { return side_char_; } // Get raw char if needed
    int64_t get_raw_price() const { return raw_price_; }
    uint32_t get_size() const { return size_; }
    uint64_t get_order_id() const { return order_id_; }
    uint8_t get_flags() const { return flags_; }
    int32_t get_ts_in_delta() const { return ts_in_delta_; }
    uint32_t get_sequence() const { return sequence_; }

    // Getters for converted/derived properties
    const std::string& get_symbol() const { return symbol_str_; } // Needs to be set
    double get_price() const { return price_double_; }
    OrderSide get_side() const { return side_; } // Your internal enum

    // Method to set the resolved symbol string
    void set_symbol_str(const std::string& sym) { symbol_str_ = sym; }

 private:
    uint64_t ts_event_;      // Capture-server-received timestamp
    uint16_t publisher_id_;  // Datab_ento publisher ID
    uint32_t instrument_id_; // Numeric instrument ID (Databento's ID for the asset)
    char action_;            // The raw action character from DBN (e.g., 'A', 'C', 'M', 'T', 'F')
    char side_char_;        // The raw side character from DBN (e.g., 'B', 'A', 'N')
    int64_t raw_price_;      // The order price as 1e-9 units
    uint32_t size_;          // The order quantity
    uint64_t order_id_;      // The order ID assigned by the venue
    uint8_t flags_;          // Bit field for event characteristics
    int32_t ts_in_delta_;    // Matching-engine-sending timestamp relative to ts_recv
    uint32_t sequence_;      // Sequence number

    // Derived/Converted properties for easier use in backtester
    std::string symbol_str_;   // Derived from instrument_id via symbology service
    double price_double_;      // Converted raw_price to double
    OrderSide side_;           // Converted side_char to your internal OrderSide enum

    OrderSide convert_char_to_OrderSide(char s_char) const {
        if (s_char == 'A') return OrderSide::kAsk; 
        if (s_char == 'B') return OrderSide::kBid;  
        return OrderSide::kNone; 
    }
};

//////////////////////////////////////////////////////////////
///////////// MARK: Strategy Classes
//////////////////////////////////////////////////////////////

class StrategySignalEvent : Event {
public:
    std::string symbol;
    SignalType signal_type; // e.g., BUY_SIGNAL, SELL_SIGNAL, Maybe? HEDGE_SIGNAL
    // double suggested_price; // Optional, for limit orders
    // long suggested_quantity; // Optional

    StrategySignalEvent(long long ts, const std::string& sym,  double price = 0.0, long qty = 0)
        : Event(ts, EventType::kStrategySignal), symbol(sym), signal_type(st) {}
};

class StrategyOrderEvent : Event {
public:
    std::string symbol;
    OrderType order_type; // Add, Modify, Cancel, Fill, Rejection
    double price;
    long quantity;
    uint64_t order_id;
    std::string strategy_id;
    EventType event_type;

    StrategyOrderEvent(long long ts, const std::string& sym, SignalType st, double price = 0.0, long qty = 0)
        : Event(ts, event_type), symbol(sym), order_type(st) {}
};

//////////////////////////////////////////////////////////////
///////////// MARK: Backtester Classes
//////////////////////////////////////////////////////////////
 kBacktestControlStart,
    kBacktestControlEndOfDay,
    kBacktestControlEndOfBacktest,
    kBacktestControlSnapshot

//////////////////////////////////////////////////////////////
///////////// MARK: Event Queue 
//////////////////////////////////////////////////////////////


class EventQueue {
private:
    // The underlying priority queue storing unique pointers to Events
    // std::vector as the underlying container for the heap
    // EventComparator ensures min-heap behavior based on timestamp (and then type)
    std::priority_queue<std::unique_ptr<Event>, std::vector<std::unique_ptr<Event>>, EventComparator> pq;

public:
    // Constructor
    EventQueue() {}

    // Destructor (std::unique_ptr handles memory of contained events)
    ~EventQueue() {}

    // Takes std::unique_ptr to take ownership of the event object
    void push_event(std::unique_ptr<Event> event_ptr) {
        if(event_ptr != nullptr){
            pq.push(std::move(event_ptr)); // Use std::move to transfer ownership
        }
    }

    bool is_empty() const {
        return pq.empty();
    }

    // Get a reference to the top event (without removing)
    // Returns a const reference to avoid modification of the event while in queue
    CONST Event& top_event() CONST {
        // You might want to add a check for pq.empty() here and throw an exception
        RETURN *pq.top();
    }

    // Remove and return the top event
    // Returns std::unique_ptr to transfer ownership to the caller
    std::unique_ptr<Event> pop_top_event() {
        IF pq.empty() THEN
            // Handle error: queue is empty, can't pop
            // e.g., throw std::runtime_error("Attempted to pop from empty EventQueue");
            RETURN nullptr;
        END IF
        std::unique_ptr<Event> top_event_ptr = std::move(pq.top()); // Get and move ownership
        pq.pop(); // Remove from queue
        RETURN top_event_ptr;
    }

    // Get the current size of the queue
    SIZE_T size() CONST {
        return pq.size();
    }

    // Clear all events from the queue (for resetting)
    void clear() {
        // While pq.pop() would work, this might be faster to clear the underlying vector
        pq = std::priority_queue<std::unique_ptr<Event>, std::vector<std::unique_ptr<Event>>, EventComparator>();
    }
};