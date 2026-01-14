#include "InstrumentState.h"
#include "OrderBook.h"

namespace backtester {

void InstrumentState::OnMarketEvent(const MarketByOrderEvent& event) {

    auto it = books_.find(event.publisher_id);
        
    // If this is the first time seeing this publisher, create the book
    if (it == books_.end()) {
        it = books_.emplace(event.publisher_id, OrderBook()).first;
    }
    it->second.Apply(event);

        // 2. Update BBO/WMP from the new book state.
        //    (OrderBook needs to return a 'BookUpdate' struct or similar)
    //     if (order_book_.has_changed()) {
    //         bbo_cache_.update(order_book_.get_bbo());
    //         wmp_calculator_.update(bbo_cache_.get_bid(), bbo_cache_.get_bid_size(),
    //                                bbo_cache_.get_ask(), bbo_cache_.get_ask_size());
    //     }

    //     // 3. Update trade-based metrics if this event was a trade.
    //     if (event.action == 'F') { // 'F' for Fill/Trade
    //         vwap_.update(event.price, event.quantity);
    //         last_trade_.update(event.price, event.quantity);
    //         total_volume_.update(event.quantity);
    //         volatility_.update(event.price, event.timestamp);
    //     }

    //     // 4. Update flow-based metrics on all events.
    //     order_flow_.update(event);

    //     // 5. Update system stats (ALWAYS LAST)
    //     latency_tracker_.update_system_latency(event.timestamp);
}

const std::vector<BidAskPair> InstrumentState::GetOBSnapshotByPub(
                                                uint16_t publisher_id, 
                                                std::size_t level_count) const {
    static const std::vector<BidAskPair> EMPTY_SNAPSHOT;
                                                
    const OrderBook* book = GetOrderBook(publisher_id);
    return book? book->GetSnapshot(level_count) : EMPTY_SNAPSHOT;
}

}