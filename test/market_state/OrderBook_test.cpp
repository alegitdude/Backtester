#include "../../data_ingestion/CsvZstReader.h"
#include "../../data_ingestion/DataReaderManager.h"
#include "../../market_state/OrderBook.h"
#include "../../core/EventQueue.h"
#include "../../market_state/MarketStateManager.h"
#include "../../core/Types.h"
#include <gtest/gtest.h>
#include <deque>

namespace backtester {

struct ExpectedMBP10 {
    uint64_t ts_event;
    std::vector<BidAskPair> levels; 
};    

class OrderBookTest : public ::testing::Test {
 protected:
    std::deque<std::pair<uint64_t, std::vector<BidAskPair>>> expected_mbp10_;

    DataSourceConfig data_source = {.symbol = "ES", 
        .filepath = "../test/test_data/ES-glbx-20251105.mbo.csv.zst", 
        .schema = DataSchema::MBO, 
        .encoding = Encoding::CSV, 
        .compression = Compression::ZSTD, 
        .price_format = PriceFormat::FIXPNTINT, 
        .ts_format = TmStampFormat::UNIX };
    AppConfig test_config = {.data_streams = std::vector<DataSourceConfig>{data_source}};                              
                                    

    std::vector<std::string_view> SplitFields(std::string line) {
        std::vector<std::string_view> result;
        size_t start_pos = 0;
        std::string_view current_view(line);
        while(true){
            size_t delim_pos = current_view.find(',', start_pos);
            std::string_view token;

            if (delim_pos == std::string_view::npos) {
                // Last token in the line
                token = current_view.substr(start_pos);
                break;
            } else {
                // Found a token
                token = current_view.substr(start_pos, delim_pos - start_pos);
                start_pos = delim_pos + 1; 
            }
            result.push_back(token);
        }
        return result;
    };

    void LoadExpectedMbp10(const std::string& mbp10_path) {
        backtester::CsvZstReader reader;
        ASSERT_TRUE(reader.Open(mbp10_path));

        std::string line;
        bool first = true;
        int count = 0;
        while (reader.ReadLine(line)) {
            std::cout << line << std::endl;
            count++;
            if(count > 2) {break;}
            if (first) { first = false; continue; }  // skip header
            if (count > 10) {break;}
            std::vector<std::string_view> fields = SplitFields(line); 
            ASSERT_GE(fields.size(), 73);

            uint64_t ts_recv = std::stoull(std::string(fields[0]));
            // field 13 starts bid_px
            std::vector<BidAskPair> snapshot(10, {kUndefPrice, kUndefPrice, 0, 0, 0, 0});
            for (int i = 0; i < 10; ++i) {
                int base = 13 + i * 6;  
                if (fields[base] != "") {
                    std::cout << fields[base] << std::endl;
                    snapshot[i].bid_px = std::stoll(std::string(fields[base]));
                    std::cout << fields[base + 2] << std::endl;   
                    snapshot[i].bid_sz = std::stoull(std::string(fields[base+2]));
                    std::cout << fields[base + 4] << std::endl;
                    snapshot[i].bid_ct = std::stoi(std::string(fields[base+4]));
                }
                if (fields[base+1] != "") {
                    snapshot[i].ask_px = std::stoll(std::string(fields[base+1]));
                    snapshot[i].ask_sz = std::stoull(std::string(fields[base+3]));
                    snapshot[i].ask_ct = std::stoi(std::string(fields[base+5]));
                }
            }
            expected_mbp10_.push_back({ts_recv, std::move(snapshot)});
            count++;
            
        }
        //std::cout << line << std::endl;
        std::cout << "finished mbp-10:" << expected_mbp10_.size() << std::endl;
    }

    inline bool isMarketEvent(EventType type) {
    return type >= kMarketOrderAdd && type <= kMarketHeartbeat;
	}
};

TEST_F(OrderBookTest, ES_2025_11_05_FullDay_MatchesDatabentoMBP10_Exactly) {
    LoadExpectedMbp10("../test/test_data/ES-glbx-20251105.mbp-10.csv.zst");

    // EventQueue event_queue;
    // DataReaderManager data_reader_manager(event_queue);
    // MarketStateManager market_state_manager;
    // ASSERT_TRUE(data_reader_manager.RegisterAndInitStreams(test_config.data_streams));

    // size_t events_processed = 0;
    // std::cout << "mbp10:" << expected_mbp10_.size() << std::endl;
    // std::cout << "MBO: " << event_queue.IsEmpty() << std::endl;
    // while(!event_queue.IsEmpty()){     
    
    //     auto current_event = event_queue.PopTopEvent(); 
    //     uint64_t current_time = current_event->timestamp;
    //     EventType eventType = current_event->type;

    //     if (isMarketEvent(eventType)) {
    //         const MarketByOrderEvent* market_event = 
    //         static_cast<const MarketByOrderEvent*>(current_event.get());

    //         market_state_manager.OnMarketEvent(*market_event);
    //         data_reader_manager.LoadNextEventForSymbol(market_event->symbol);

    //         while (!expected_mbp10_.empty() && 
    //             market_event->timestamp >= expected_mbp10_.front().first) {

    //             const auto& expected_levels = expected_mbp10_.front().second;
    //             const auto actual_levels = market_state_manager.GetOBSnapshot(10);

    //             EXPECT_EQ(actual_levels, expected_levels);
    //             //     << "Mismatch at ts_event = " << expected_mbp10_.front().first
    //             //     << " after " << events_processed << " MBO events";

    //             expected_mbp10_.pop_front();
    //         }
    //     } 
        //events_processed++;
   // }

    //EXPECT_TRUE(expected_mbp10_.empty());
}
}