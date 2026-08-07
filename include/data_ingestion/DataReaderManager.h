#pragma once
#include <unordered_map>

#include "../core/Constants.h"
#include "../core/Event.h"
#include "../core/Types.h"
#include "CsvZstReader.h"

namespace backtester {
class EventQueue;

class DataReaderManager {
 public:
  DataReaderManager() = default;

  bool RegisterAndInitStreams(const std::vector<DataSourceConfig>& file_paths);
  bool LoadNextEventFromSource(uint16_t data_source_id, MarketByOrderEvent& out);

 private:
  std::vector<DataStream> readers_;

  bool ParseMboLineToEvent(const std::vector<backtester::DataStream>::iterator it,
                           const std::string& line, MarketByOrderEvent& out);

  std::string_view GetNextToken(size_t& start_pos, std::string_view& current_view);

  // std::unique_ptr<Event> ParseOhlcvLineToEvent(  // TODO
  //     const std::string& symbol,
  //     const std::string& line
  // );

  inline OrderSide CharToOrderSide(char side) {
    if (side == 'A') {
      return OrderSide::kAsk;
    }
    if (side == 'B') {
      return OrderSide::kBid;
    } else {
      return OrderSide::kNone;
    }
  }

  inline EventType ActionToEventTyp(char act) {
    if (act == 'A') {
      return EventType::kMarketOrderAdd;
    }
    if (act == 'M') {
      return EventType::kMarketOrderModify;
    }
    if (act == 'C') {
      return EventType::kMarketOrderCancel;
    }
    if (act == 'R') {
      return EventType::kMarketOrderClear;
    }
    if (act == 'T') {
      return EventType::kMarketTrade;
    }
    if (act == 'F') {
      return EventType::kMarketFill;
    } else {
      return EventType::kMarketNone;
    }
  }
};

}  // namespace backtester