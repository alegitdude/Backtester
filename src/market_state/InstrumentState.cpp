#include "market_state/InstrumentState.h"
#include "market_state/OrderBook.h"
#include <span>

namespace backtester {

    void InstrumentState::OnMarketEvent(const MarketByOrderEvent& event) {

        OrderBook& book = GetOrInsertOrderBook(event.publisher_id);
        book.Apply(event);

        if (event.price != std::numeric_limits<int64_t>::max()) {
            // Update VWAP - equation : cumulative_notional / cumulative_volume
            if (event.type == EventType::kMarketTrade) {
                snapshot_.cumulative_volume += event.size;
                cumulative_notional_ += event.price * event.size;
                snapshot_.vwap = static_cast<price_t>(cumulative_notional_ / snapshot_.cumulative_volume);

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
    }

    void InstrumentState::UpdateInstrumentBbo() {
        instrument_Bbo_.bid = {};
        instrument_Bbo_.ask = {};

        for (auto& book : books_) {
            BidAskPair bbo = book.GetBbo();
            if (bbo.bid.price != 0 && bbo.bid.price != kUndefPrice) {

                if (bbo.bid.price > instrument_Bbo_.bid.price || instrument_Bbo_.bid.price == kUndefPrice) {
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
        snapshot_.bbo = instrument_Bbo_;
    }

    const std::vector<BidAskPair> InstrumentState::GetOBSnapshotByPub(
        uint16_t publisher_id,
        std::size_t level_count) const {
        static const std::vector<BidAskPair> EMPTY_SNAPSHOT;

        const OrderBook* book = GetOrderBook(publisher_id);
        return book ? book->GetSnapshot(level_count) : EMPTY_SNAPSHOT;
    }

    void InstrumentState::GetAggOBBidsSnapshot(std::span<PriceLevel> snapshot) const {
        size_t num_books = books_.size();
        std::vector<size_t> book_indices(num_books, 0);
        size_t snap_i = 0;
        size_t book_i = 0;

        while(snap_i < snapshot.size()){
            for(book_i; book_i < num_books; book_i++){
                PriceLevel level = books_[book_i].GetBidLevel(book_indices[book_i]);
                if(level.price != snapshot[snap_i].price) continue;
                snapshot[snap_i].count += level.count;
                snapshot[snap_i].size += level.size;
                book_indices[book_i]++;
            }
            book_i = 0;
            snap_i++;
        }
    }

    void InstrumentState::GetAggOBAsksSnapshot(std::span<PriceLevel> snapshot) const {
        size_t num_books = books_.size();
        std::vector<size_t> book_indices(num_books, 0);
        size_t snap_i = 0;
        size_t book_i = 0;
     
        while(snap_i < snapshot.size()){
            for(book_i; book_i < num_books; book_i++){
                PriceLevel level = books_[book_i].GetAskLevel(book_indices[book_i]);
                if(level.price != snapshot[snap_i].price) continue;
                snapshot[snap_i].count += level.count;
                snapshot[snap_i].size += level.size;
                book_indices[book_i]++;
            }
            book_i = 0;
            snap_i++;
        }
    }

    int64_t InstrumentState::GetQueueDepthByPx(OrderSide side, int64_t price) const {
        int64_t total_depth = 0;
        for (auto& book : books_) {
            total_depth += book.GetLevelByPx(side, price).size;
        }
        return total_depth;
    }

}