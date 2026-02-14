#pragma once
#include "OrderBook.h"
#include <vector>

namespace backtester {

class InstrumentState {
 public:
    InstrumentState(uint32_t instr_id) : instrument_id(instr_id){};

    uint32_t instrument_id;

    void OnMarketEvent(const MarketByOrderEvent& event);
    inline Bbo GetInstrumentBbo(){return instrument_Bbo_;}
    
    const std::vector<BidAskPair> GetOBSnapshotByPub(uint16_t publisher_id, 
                                               std::size_t level_count = 1) const;
    // double get_vwap() const { return vwap_.get_vwap(); }
    // const Bbo& get_bbo() const { return bbo_cache_.get_bbo(); }
    // double get_weighted_mid_price() const { return wmp_calculator_.get_wmp(); }
    // double get_realized_volatility() const { return volatility_.get_vol(); }
    // double get_p99_latency_ns() const { return latency_tracker_.get_p99(); }
 private:
    // Key = publisher_id
    std::unordered_map<uint16_t, OrderBook> books_;
    Bbo instrument_Bbo_;

    void UpdateInstrumentBbo();

    const inline OrderBook* GetOrderBook(uint16_t publisher_id) const {
        auto it = books_.find(publisher_id);
        if (it != books_.end()) {
            return &it->second;
        }

        return nullptr; 
    }
    // // L1 / Microstructure
    // BboCache bbo_cache_;
    // WeightedMidPriceCalculator wmp_calculator_;
    
    // // Trade Metrics
    // VwapCalculator vwap_;
    // LastTradeTracker last_trade_;
    // TotalVolumeTracker total_volume_;

    // // Flow Metrics
    // OrderFlowTracker order_flow_;
    // RealizedVolCalculator volatility_; // trade- or midprice-driven

    // // System Metrics
    // SystemLatencyTracker latency_tracker_;
};

}