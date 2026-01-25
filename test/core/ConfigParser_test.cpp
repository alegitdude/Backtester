#include "../../include/core/ConfigParser.h"
#include <gtest/gtest.h>

namespace backtester {

class ConfigParserTest : public ::testing::Test {
 protected:

};

/////////////// ParseConfigToObj ////////////////////
TEST_F(ConfigParserTest, ThrowsWhenPathDoesNotExist) {
    std::filesystem::path bad_path = "/nonexistent/config.json";
    
    EXPECT_THROW({
        ParseConfigToObj(bad_path);
    }, std::runtime_error);
}

TEST_F(ConfigParserTest, ThrowsWhenFileDoesNotOpen) {
    std::filesystem::path cant_open = "../test/test_data/no-access.txt";
    
    EXPECT_THROW({
        ParseConfigToObj(cant_open);
    }, std::runtime_error);
}

TEST_F(ConfigParserTest, ThrowsWhenNotValidJson) {
    std::filesystem::path not_json_path = "../test/test_data/futures_mbo.csv.zst";
    
    EXPECT_THROW({
        ParseConfigToObj(not_json_path);
    }, std::runtime_error);
}

/////////////// ParseConfigFromJson /////////////////

TEST_F(ConfigParserTest, ParsesValidConfigWithAllFields) {
    nlohmann::json valid_config = {
        {"start_time", "2024-01-01T09:30:00Z"},
        {"end_time", "2024-01-01T16:00:00Z"},
        {"execution_latency", 100},
        {"initial_cash", 50000},
        {"log_file_path", "../../logs"},
        {"report_output_dir", "../../reports"},
        {"strategy_name", "TestStrategy"},
        {"traded_symbol", "ES"},
        {"data_streams", {{
				{"data_source_name" , "ES"},
				{"symbology_filepath" , "../test/test_data/ES-20251105_symbology.csv"}, 
				{"data_filepath" , "../test/test_data/futures_mbo.csv.zst"}, 
				{"encoding" , "CSV"},
				{"schema" , "MBO"},
				{"compression" , "ZSTD"},
				{"price_format" , "decimal"},
				{"timestamp_format" , "iso"}			
			}}}
    };
    
    AppConfig result = ParseConfigFromJson(valid_config);
    DataSourceConfig stream;
    stream.data_source_name = "ES";
    stream.compression = Compression::ZSTD;
    stream.encoding = Encoding::CSV;
    stream.price_format = PriceFormat::DECIMAL;
    stream.schema = DataSchema::MBO;
    stream.ts_format = TmStampFormat::ISO;
    stream.data_filepath = "../test/test_data/futures_mbo.csv.zst";
    stream.data_symbology = {{"ESH7",42140860, "2025-11-05"},
                            {"ESM9",42005050,"2025-11-05"},
                            {"ESM6-ESH7",42008149,"2025-11-05"},
                            {"ESH7-ESM7",42018261,"2025-11-05"},
                            {"ESZ5",294973,"2025-11-05"},
                            {"ESZ8",42033138,"2025-11-05"},
                            {"ESH7-ESU7",42007293,"2025-11-05"},
                            {"ESM6-ESZ6",42005583,"2025-11-05"},
                            {"ESZ6-ESM7",42006493,"2025-11-05"},
                            {"ESU6-ESZ6",42006263,"2025-11-05"},
                            {"ESM8",42140863,"2025-11-05"},
                            {"ESZ5-ESH6",42007065,"2025-11-05"},
                            {"ESZ6-ESH7",42008507,"2025-11-05"},
                            {"ESM7-ESU7",42016712,"2025-11-05"},
                            {"ESH8",42140879,"2025-11-05"},
                            {"ESM7",42140856,"2025-11-05"},
                            {"ESZ5-ESU6",42016422,"2025-11-05"},
                            {"ESZ5-ESM6",42006622,"2025-11-05"},
                            {"ESZ6",10252,"2025-11-05"},
                            {"ESU6-ESH7",42007174,"2025-11-05"},
                            {"ESZ9",42004134,"2025-11-05"},
                            {"ESU6-ESM7",42009453,"2025-11-05"},
                            {"ESH6-ESM6",42018017,"2025-11-05"},
                            {"ESM0",42004092,"2025-11-05"},
                            {"ESU9",42000977,"2025-11-05"},
                            {"ESM6-ESU6",42004810,"2025-11-05"},
                            {"ESH6",42140878,"2025-11-05"},
                            {"ESU0",42004613,"2025-11-05"},
                            {"ESM6",42140864,"2025-11-05"},
                            {"ESU7-ESZ7",42009476,"2025-11-05"},
                            {"ESZ7",17740,"2025-11-05"},
                            {"ESU7",42140874,"2025-11-05"},
                            {"ESZ0",42004653,"2025-11-05"},
                            {"ESH0",42000746,"2025-11-05"},
                            {"ESH6-ESZ6",42018129,"2025-11-05"},
                            {"ESU8",42140861,"2025-11-05"},
                            {"ESH6-ESU6",42004904,"2025-11-05"},
                            {"ESU6",42140870,"2025-11-05"},
                            {"ESZ5-ESZ6",42008091,"2025-11-05"},
                            {"ESH9",42002687,"2025-11-05"},
                            {"ESH6-ESH7",42011265,"2025-11-05"}};

    std::vector<uint32_t> expected_instrs = {42140860, 42005050, 42008149, 42018261, 
        294973, 42033138, 42007293, 42005583, 42006493, 42006263, 42140863,
        42007065, 42008507, 42016712, 42140879, 42140856, 42016422, 42006622, 10252,
        42007174, 42004134, 42009453, 42018017, 42004092, 42000977, 42004810, 42140878,
        42004613, 42140864, 42009476, 17740, 42140874, 42004653, 42000746, 42018129, 42140861,
        42004904, 42140870, 42008091, 42002687, 42011265};

    EXPECT_EQ(result.start_time, 1704101400000000000);
    EXPECT_EQ(result.end_time, 1704124800000000000);
    EXPECT_EQ(result.execution_latency, 100);
    EXPECT_EQ(result.initial_cash, 50000);
    EXPECT_EQ(result.log_file_path, "../../logs");
    EXPECT_EQ(result.report_output_dir, "../../reports");
    EXPECT_EQ(result.strategy_name, "TestStrategy");
    EXPECT_EQ(result.traded_symbol, "ES");
    EXPECT_EQ(result.active_instruments, expected_instrs);
    EXPECT_EQ(result.data_streams[0].compression, stream.compression);
    EXPECT_EQ(result.data_streams[0].data_filepath, stream.data_filepath);
    EXPECT_EQ(result.data_streams[0].data_source_name, stream.data_source_name);
    EXPECT_EQ(result.data_streams[0].encoding, stream.encoding);
    EXPECT_EQ(result.data_streams[0].price_format, stream.price_format);
    EXPECT_EQ(result.data_streams[0].schema, stream.schema);
    EXPECT_EQ(result.data_streams[0].ts_format, stream.ts_format);

    for(int i = 0; i < result.data_streams[0].data_symbology.size(); i++){
        EXPECT_EQ(result.data_streams[0].data_symbology[i].date, stream.data_symbology[i].date);
        EXPECT_EQ(result.data_streams[0].data_symbology[i].instrument_id, stream.data_symbology[i].instrument_id);
        EXPECT_EQ(result.data_streams[0].data_symbology[i].symbol, stream.data_symbology[i].symbol);
    }


}
}