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
    std::unordered_map<uint16_t, OrderBook> books_;
    BidAskPair instrument_Bbo_;
    MarketSnapshot snapshot_;

    void UpdateInstrumentBbo();

    const inline OrderBook* GetOrderBook(uint16_t publisher_id) const {
        auto it = books_.find(publisher_id);
        if (it != books_.end()) {
            return &it->second;
        }

        return nullptr; 
    }
    // SystemLatencyTracker latency_tracker_;
};

}