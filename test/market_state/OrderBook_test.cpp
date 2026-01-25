#include "../../include/data_ingestion/CsvZstReader.h"
#include "../../include/data_ingestion/DataReaderManager.h"
#include "../../include/market_state/OrderBook.h"
#include "../../include/core/EventQueue.h"
#include "../../include/market_state/MarketStateManager.h"
#include "../../include/core/ConfigParser.h"
#include "../../include/core/Types.h"
#include <gtest/gtest.h>
#include <deque>
#include <charconv>

namespace backtester {

struct ExpectedMBP10 {
    uint64_t ts_event;
    int64_t price;
    uint32_t sequence_id;
    std::vector<BidAskPair> levels; 
};    

class OrderBookTest : public ::testing::Test {
 protected:
    // instrument_id for key
    std::unordered_map<uint32_t, std::deque<ExpectedMBP10>> expected_mbp10_;

    DataSourceConfig data_source = {
        .data_source_name = "ES",
        .data_filepath = "/home/r/Desktop/11_06_25_ES_FUT/MBO/glbx-mdp3-20251106.mbo.ESZ5.csv.zst", 
        .schema = DataSchema::MBO, 
        .encoding = Encoding::CSV, 
        .compression = Compression::ZSTD, 
        .price_format = PriceFormat::FIXPNTINT, 
        .ts_format = TmStampFormat::UNIX };

    AppConfig test_config = {.data_streams = std::vector<DataSourceConfig>{data_source}};                              
                                    
    inline bool isMarketEvent(EventType type) {
    return type >= kMarketOrderAdd && type <= kMarketHeartbeat;
	}

    template <typename T>
    void ParseField(std::string_view sv, T& value) {
        if (sv.empty()) return;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    }

    void LoadExpectedMbp10(const std::string& mbp10_path) {
        backtester::CsvZstReader reader;
        ASSERT_TRUE(reader.Open(mbp10_path));

        std::string line;
        std::vector<std::string_view> fields;
        fields.reserve(100); 
        bool header = true;

        while (reader.ReadLine(line)) {
            if (header) { header = false; continue; }  
            fields.clear();
            std::string_view sv(line);
            size_t start = 0, end = 0;
            while ((end = sv.find(',', start)) != std::string_view::npos) {
                fields.push_back(sv.substr(start, end - start));
                start = end + 1;
            }
            fields.push_back(sv.substr(start));

            if (fields.size() < 73 || fields[5] == "T" || fields[5] == "F") continue;

            uint32_t instrument_id;
            ParseField(fields[4], instrument_id);

            auto& instr_queue = expected_mbp10_[instrument_id];
            instr_queue.emplace_back(); 
            auto& snapshot = instr_queue.back();

            ParseField(fields[1], snapshot.ts_event);
            ParseField(fields[8], snapshot.price);
            ParseField(fields[12], snapshot.sequence_id);

            snapshot.levels.assign(10, {kUndefPrice, kUndefPrice, 0, 0, 0, 0});
            for (int i = 0; i < 10; ++i) {
                int base = 13 + i * 6;
                if(fields[base] != ""){
                    ParseField(fields[base], snapshot.levels[i].bid_px);
                    ParseField(fields[base+2], snapshot.levels[i].bid_sz);
                    ParseField(fields[base+4], snapshot.levels[i].bid_ct);
                }
                if(fields[base+1] != ""){
                    ParseField(fields[base+1], snapshot.levels[i].ask_px);
                    ParseField(fields[base+3], snapshot.levels[i].ask_sz);
                    ParseField(fields[base+5], snapshot.levels[i].ask_ct);
                }
            }
        }
    }
};


TEST_F(OrderBookTest, ES20251105_FullDay_MatchesDB_MBP10_OnePub) {
    DataSourceConfig data_source = {
        .data_source_name = "ES",
        .data_filepath = "../test/test_data/ES-glbx-20251105.mbo.csv.zst", 
        .schema = DataSchema::MBO, 
        .encoding = Encoding::CSV, 
        .compression = Compression::ZSTD, 
        .price_format = PriceFormat::FIXPNTINT, 
        .ts_format = TmStampFormat::UNIX };
    
    std::string sym_path = "../test/test_data/ES-20251105_symbology.csv";
    std::vector<Symbol> symbols = backtester::ParseDataSymbols(sym_path);

    AppConfig config = {.data_streams = std::vector<DataSourceConfig>{data_source},
                        .active_instruments = {}};  

    for(const auto& symbol : symbols){
		config.active_instruments.push_back(symbol.instrument_id);	
	}
    
    LoadExpectedMbp10("../test/test_data/ES-glbx-20251105.mbp-10.csv.zst");

    EventQueue event_queue;
    DataReaderManager data_reader_manager(event_queue);
    MarketStateManager market_state_manager;
    market_state_manager.Initialize(config.active_instruments);
    
    ASSERT_TRUE(data_reader_manager.RegisterAndInitStreams(config.data_streams));

    size_t events_processed = 0;
    size_t snapshots_compared = 0;

    std::cout << "mbp10:" << expected_mbp10_.size() << std::endl;

    while(!event_queue.IsEmpty()){      
        auto current_event = event_queue.PopTopEvent(); 
        uint64_t current_time = current_event->timestamp;
        EventType eventType = current_event->type;

        if (isMarketEvent(eventType)) {
            const MarketByOrderEvent* market_event = 
            static_cast<const MarketByOrderEvent*>(current_event.get());    
            
            market_state_manager.OnMarketEvent(*market_event);
            data_reader_manager.LoadNextEventFromSource(market_event->data_source);
            
            bool has_last_flag = (market_event->flags & 0x80) != 0;  // F_LAST = 128
            uint32_t instr_id = market_event->instrument_id;

            bool can_compare;
            if(!expected_mbp10_.count(instr_id)){
                can_compare = false;
            } else {
                can_compare = has_last_flag && 
                current_time == expected_mbp10_[instr_id].front().ts_event &&
                market_event->sequence == expected_mbp10_[instr_id].front().sequence_id &&
                market_event->price == expected_mbp10_[instr_id].front().price;
            }

            if (can_compare) {
                const auto& expected_levels = expected_mbp10_[instr_id].front().levels;
                const auto actual_levels = market_state_manager.GetOBSnapshot(
                    market_event->instrument_id, market_event->publisher_id, 10);
                
                EXPECT_EQ(actual_levels, expected_levels)
                    << "Mismatch at ts_event = " << expected_mbp10_[instr_id].front().ts_event
                    << " after " << events_processed << " MBO events";
                
                expected_mbp10_[instr_id].pop_front();
                if(expected_mbp10_[instr_id].empty()){
                    expected_mbp10_.erase(instr_id);
                }
                snapshots_compared++;               
            }                        
        } 
        events_processed++;
    }
    std::cout << events_processed << std::endl;
    EXPECT_TRUE(expected_mbp10_.empty());
}

}