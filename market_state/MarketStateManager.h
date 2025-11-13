#pragma once
#include "../core/Event.h"
#include <iostream>

namespace backtester {

class MarketStateManager{
 public:
	MarketStateManager() = default;

    void OnMarketEvent(const MarketByOrderEvent* event) {
        order_book_.on_event(event);

        // 2. Update BBO/WMP from the new book state.
        //    (OrderBook needs to return a 'BookUpdate' struct or similar)
        if (order_book_.has_changed()) {
            bbo_cache_.update(order_book_.get_bbo());
            wmp_calculator_.update(bbo_cache_.get_bid(), bbo_cache_.get_bid_size(),
                                   bbo_cache_.get_ask(), bbo_cache_.get_ask_size());
        }

        // 3. Update trade-based metrics if this event was a trade.
        if (event.action == 'F') { // 'F' for Fill/Trade
            vwap_.update(event.price, event.quantity);
            last_trade_.update(event.price, event.quantity);
            total_volume_.update(event.quantity);
            volatility_.update(event.price, event.timestamp);
        }

        // 4. Update flow-based metrics on all events.
        order_flow_.update(event);

        // 5. Update system stats (ALWAYS LAST)
        latency_tracker_.update_system_latency(event.timestamp);
    }

    const OrderBook& get_order_book() const { return order_book_; }
    double get_vwap() const { return vwap_.get_vwap(); }
    const Bbo& get_bbo() const { return bbo_cache_.get_bbo(); }
    double get_weighted_mid_price() const { return wmp_calculator_.get_wmp(); }
    double get_realized_volatility() const { return volatility_.get_vol(); }
    double get_p99_latency_ns() const { return latency_tracker_.get_p99(); }
    

private:
    OrderBook order_book_;

    // L1 / Microstructure
    BboCache bbo_cache_;
    WeightedMidPriceCalculator wmp_calculator_;
    
    // Trade Metrics
    VwapCalculator vwap_;
    LastTradeTracker last_trade_;
    TotalVolumeTracker total_volume_;

    // Flow Metrics
    OrderFlowTracker order_flow_;
    RealizedVolCalculator volatility_; // trade- or midprice-driven

    // System Metrics
    SystemLatencyTracker latency_tracker_;

};
};

