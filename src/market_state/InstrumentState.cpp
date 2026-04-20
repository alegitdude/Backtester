#include "../../include/market_state/InstrumentState.h"
#include "../../include/market_state/OrderBook.h"

namespace backtester {

    void InstrumentState::OnMarketEvent(const MarketByOrderEvent& event) {

        auto it = books_.find(event.publisher_id);

        // If this is the first time seeing this publisher, create the book
        if (it == books_.end()) {
            it = books_.emplace(event.publisher_id, OrderBook()).first;
        }
        it->second.Apply(event);

        if (event.price != std::numeric_limits<int64_t>::min() ) {
            // Update VWAP - equation : cumulative_notional / cumulative_volume
            if (event.type == EventType::kMarketTrade) {
                snapshot_.cumulative_volume += event.size;
                snapshot_.cumulative_notional += event.price * event.size;
                snapshot_.vwap = snapshot_.cumulative_notional / snapshot_.cumulative_volume;

                snapshot_.last_trade.aggressor_side = event.side;
                snapshot_.last_trade.price = event.price;
                snapshot_.last_trade.size = event.size;
                snapshot_.last_trade.timestamp = event.timestamp;

                snapshot_.session_high = std::max(event.price, snapshot_.session_high);
                snapshot_.session_low = std::min(event.price, snapshot_.session_low);
            }
            else if (event.type != EventType::kMarketFill && event.flags & 0x80) {
                UpdateInstrumentBbo();
                // Update WMP - equation : (bid_price * ask_size + ask_price * bid_size) / (bid_size + ask_size)
                int64_t total_size = instrument_Bbo_.bid.size + instrument_Bbo_.ask.size;
                if (total_size > 0 && instrument_Bbo_.bid.price != kUndefPrice &&
                    instrument_Bbo_.ask.price != kUndefPrice) {
                    snapshot_.wmp = (instrument_Bbo_.bid.price * instrument_Bbo_.ask.size + instrument_Bbo_.ask.price
                        * instrument_Bbo_.bid.size) / total_size;
                }
            }
        }

        //     // 5. Update system stats (ALWAYS LAST)
        //     latency_tracker_.update_system_latency(event.timestamp);
    }

    void InstrumentState::UpdateInstrumentBbo() {
        BidAskPair prev_bbo = instrument_Bbo_;
        instrument_Bbo_.bid = {};
        instrument_Bbo_.ask = {};

        for (auto& [publisher, book] : books_) {
            BidAskPair bbo = book.GetBbo();
            if (bbo.bid.price != 0 && bbo.bid.price != kUndefPrice) {
                if (bbo.bid.price > instrument_Bbo_.bid.price) {
                    instrument_Bbo_.bid = bbo.bid;
                }
                else if (bbo.bid.price == instrument_Bbo_.bid.price) {
                    instrument_Bbo_.bid.size += bbo.bid.size;
                    instrument_Bbo_.bid.count += bbo.bid.count;
                }
            }

            if (bbo.ask.price != 0 && bbo.ask.price != kUndefPrice) {
                if (bbo.ask.price < instrument_Bbo_.ask.price) {
                    instrument_Bbo_.ask = bbo.ask;
                }
                else if (bbo.ask.price == instrument_Bbo_.ask.price) {
                    instrument_Bbo_.ask.size += bbo.ask.size;
                    instrument_Bbo_.ask.count += bbo.ask.count;
                }
            }
        }

        if (instrument_Bbo_.bid.price != kUndefPrice && instrument_Bbo_.bid.price > 
            instrument_Bbo_.ask.price) {
            //throw std::logic_error("bid price is higher than ask price?");
            instrument_Bbo_ = prev_bbo;
        }
        snapshot_.bbo = instrument_Bbo_;
    }

    const std::vector<BidAskPair> InstrumentState::GetOBSnapshotByPub(
        uint16_t publisher_id,
        std::size_t level_count) const {
        static const std::vector<BidAskPair> EMPTY_SNAPSHOT;

        const OrderBook* book = GetOrderBook(publisher_id);
        return book ? book->GetSnapshot(level_count) : EMPTY_SNAPSHOT;
    }

    int64_t InstrumentState::GetQueueDepthByPx(OrderSide side, int64_t price) const {
        int64_t total_depth = 0;
        for (auto& [publisher, book] : books_) {
            PriceLevel price_level = book.GetLevelByPx(side, price);
            total_depth += price_level.size;
        }
        return total_depth;
    }

}