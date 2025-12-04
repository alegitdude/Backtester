#pragma once
#include "../core/Event.h"
#include "OrderBook.h"
#include <iostream>

namespace backtester {

class MarketStateManager{
 public:
	MarketStateManager() = default;

    void OnMarketEvent(const MarketByOrderEvent& event);

    const std::vector<BidAskPair> GetOBSnapshot(int levels) const { return order_book_.GetSnapshot(levels); }
    // double get_vwap() const { return vwap_.get_vwap(); }
    // const Bbo& get_bbo() const { return bbo_cache_.get_bbo(); }
    // double get_weighted_mid_price() const { return wmp_calculator_.get_wmp(); }
    // double get_realized_volatility() const { return volatility_.get_vol(); }
    // double get_p99_latency_ns() const { return latency_tracker_.get_p99(); }
    

private:
    OrderBook order_book_;

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
};

