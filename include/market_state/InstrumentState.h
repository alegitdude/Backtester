#pragma once
#include "OrderBook.h"
#include <vector>

namespace backtester {

class InstrumentState {
 public:
    InstrumentState(uint32_t instr_id) : instrument_id(instr_id){
        snapshot_.instrument_id = instr_id;
    };

    uint32_t instrument_id;

    void OnMarketEvent(const MarketByOrderEvent& event);
    inline const BidAskPair GetInstrumentBbo() const {return instrument_Bbo_;}
    
    const std::vector<BidAskPair> GetOBSnapshotByPub(uint16_t publisher_id, 
                                               std::size_t level_count = 1) const;

    int64_t GetQueueDepthByPx(OrderSide side, int64_t price) const; 

    const MarketSnapshot&  GetMarketSnapshot() const { return snapshot_; }
    
    // double get_p99_latency_ns() const { return latency_tracker_.get_p99(); }
 private:
    // Key = publisher_id
    //std::unordered_map<uint16_t, OrderBook> books_;
    std::vector<OrderBook> books_;
    BidAskPair instrument_Bbo_;
    MarketSnapshot snapshot_;

    void UpdateInstrumentBbo();

    const inline OrderBook* GetOrderBook(uint16_t publisher_id) const {
        auto it = std::find_if(books_.begin(), books_.end(), [publisher_id] (const OrderBook& ob) {
            return publisher_id == ob.publisher_id;
        });
        return (it != books_.end()) ? &(*it) : nullptr;
    }

    inline OrderBook& GetOrInsertOrderBook(uint16_t publisher_id) {
        auto it = std::find_if(books_.begin(), books_.end(), [publisher_id] (const OrderBook& ob) {
            return publisher_id == ob.publisher_id;
        });
        
        if(LIKELY (it != books_.end())){
            return *it;
        }
        else {
            auto& ob = books_.emplace_back(publisher_id);
            return ob;
        }
    }
};

}