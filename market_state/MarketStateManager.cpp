#include "MarketStateManager.h"

namespace backtester {

void MarketStateManager::OnMarketEvent(const MarketByOrderEvent& event) {
        order_book_.OnEvent(event);

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
}