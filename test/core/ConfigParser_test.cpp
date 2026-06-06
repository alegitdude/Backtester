#include "../../include/core/ConfigParser.h"
#include <gtest/gtest.h>
 
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
  
namespace backtester {
 
const std::filesystem::path kTestDataFolder = TEST_DATA_DIR;
const std::filesystem::path kRootFolder = PROJECT_ROOT_DIR;
 
namespace {
constexpr uint64_t kFxd = 1'000'000'000ULL;     
constexpr uint64_t kOneDayMs = 86'400'000ULL;
}  
 
class ConfigParserTest : public ::testing::Test {
 protected:
  std::filesystem::path tmp_dir;
  std::filesystem::path config_path;     // pretend config location
  std::filesystem::path symbology_path;  // real CSV written in SetUp
  std::filesystem::path data_path;       // never opened by parser
 
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    tmp_dir = std::filesystem::temp_directory_path() /
              ("cfgparser_" + std::string(info ? info->name() : "x"));
    std::error_code ec;
    std::filesystem::remove_all(tmp_dir, ec);
    std::filesystem::create_directories(tmp_dir);
 
    config_path = tmp_dir / "config.json";
    symbology_path = tmp_dir / "symbology.csv";
    data_path = tmp_dir / "data.csv.zst";
 
    WriteFile(symbology_path,
              "symbol,instrument_id,date\n"
              "ESH7,42140860,2025-11-05\n"
              "ESM9,42005050,2025-11-05\n"
              "ESZ5,294973,2025-11-05\n");
    WriteFile(data_path, "not-really-zstd");  // parser only stores the path
  }
 
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(tmp_dir, ec);
  }
 
  static void WriteFile(const std::filesystem::path& p,
                        const std::string& contents) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << contents;
    f.close();
  }
 
  nlohmann::json MakeValidConfig() const {
    return {
        {"start_time", "2024-01-01T09:30:00Z"},
        {"end_time", "2024-01-01T16:00:00Z"},
        {"strategies",
         {{{"name", "MovAvgCrossMin"},
           {"params", {5, 10}},
           {"traded_instr_id", 294973},
           {"max_lob_lvl", 1}}}},
        {"traded_instruments",
         {{{"instrument_id", 294973},
           {"instrument_type", "FUT"},
           {"tick_size", 0.25},
           {"tick_value", 12.50},
           {"init_margin_req", 20845},
           {"main_margin_req", 17017}}}},
        {"data_streams",
         {{{"data_source_name", "ES"},
           {"symbology_filepath", symbology_path.string()},
           {"data_filepath", data_path.string()},
           {"encoding", "CSV"},
           {"schema", "MBO"},
           {"compression", "ZSTD"},
           {"price_format", "decimal"},
           {"timestamp_format", "unix"}}}}};
  }
 
  AppConfig Parse(const nlohmann::json& j) const {
    return ParseConfigFromJson(j, config_path);
  }
};
 
// Helper: fixed-point conversion of a non-exactly-representable decimal can land
// on either side by one ULP depending on round vs. truncate. Tolerate +/-1.
static void ExpectFixedPointNear(uint64_t actual, double decimal) {
  EXPECT_NEAR(static_cast<double>(actual), decimal * 1e9, 1.0);
}
static void ExpectFixedPointNear(int64_t actual, double decimal) {
  EXPECT_NEAR(static_cast<double>(actual), decimal * 1e9, 1.0);
}
 
//////////////////////////////////////////////////////////
// MARK: ParseConfigToObj — file-level paths
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, ToObj_ThrowsWhenPathDoesNotExist) {
  std::filesystem::path bad = kTestDataFolder / "nonexistent" / "config.json";
  EXPECT_THROW(ParseConfigToObj(bad), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ToObj_ThrowsWhenNotValidJson) {
  std::filesystem::path not_json = tmp_dir / "garbage.json";
  WriteFile(not_json, "this is { definitely not : valid json ]");
  EXPECT_THROW(ParseConfigToObj(not_json), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ToObj_ThrowsOnEmptyFile) {
  std::filesystem::path empty = tmp_dir / "empty.json";
  WriteFile(empty, "");
  EXPECT_THROW(ParseConfigToObj(empty), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ToObj_ParsesValidFileFromDisk) {
  // Write the baseline config to disk and confirm the file entry point yields
  // the same result as the in-memory entry point.
  WriteFile(config_path, MakeValidConfig().dump());
  AppConfig from_file = ParseConfigToObj(config_path);
  EXPECT_EQ(from_file.start_time, 1704101400000000000ULL);
  EXPECT_EQ(from_file.end_time, 1704124800000000000ULL);
  ASSERT_EQ(from_file.strategies.size(), 1u);
  EXPECT_EQ(from_file.strategies[0].name, "MovAvgCrossMin");
  ASSERT_EQ(from_file.data_configs.size(), 1);
  EXPECT_EQ(from_file.active_instruments.size(), 3);
}
 
//////////////////////////////////////////////////////////
// MARK: ParseConfigFromJson — happy path / full snapshot
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, ParsesBaselineSuccessfully) {
  EXPECT_NO_THROW(Parse(MakeValidConfig()));
}
 
TEST_F(ConfigParserTest, ParsesAllScalarFieldsWithExpectedConversions) {
  AppConfig r = Parse(MakeValidConfig());
 
  EXPECT_EQ(r.start_time, 1704101400000000000ULL);
  EXPECT_EQ(r.end_time, 1704124800000000000ULL);
 
  // Optionals omitted -> defaults.
  EXPECT_EQ(r.execution_latency_ms, 200ULL);           // kDefaultExecLatencyMs
  EXPECT_EQ(r.snapshot_interval_ns, 1'000'000'000ULL); // 1000ms * 1e6
  EXPECT_EQ(r.initial_cash, 100000ULL * kFxd);         // kDefaultInitialCash scaled
  EXPECT_DOUBLE_EQ(r.risk_free_rate, 0.05);            // kDefaultRiskFreeRate
}
 
TEST_F(ConfigParserTest, ParsesStrategyFields) {
  AppConfig r = Parse(MakeValidConfig());
  ASSERT_EQ(r.strategies.size(), 1u);
  EXPECT_EQ(r.strategies[0].name, "MovAvgCrossMin");
  EXPECT_EQ(r.strategies[0].params, (std::vector<int>{5, 10}));
  EXPECT_EQ(r.strategies[0].traded_instr_id, 294973u);
  EXPECT_EQ(r.strategies[0].max_lob_lvl, 1u);
}
 
TEST_F(ConfigParserTest, ParsesTradedInstrumentWithFixedPoint) {
  AppConfig r = Parse(MakeValidConfig());
  ASSERT_EQ(r.traded_instruments.size(), 1u);
  const auto& ti = r.traded_instruments[0];
  EXPECT_EQ(ti.instrument_id, 294973u);
  EXPECT_EQ(ti.instrument_type, InstrumentType::FUT);
  EXPECT_EQ(ti.tick_size, 250'000'000ULL);         
  EXPECT_EQ(ti.tick_value, 12'500'000'000ULL);       
  EXPECT_EQ(ti.init_margin_req, 20845ULL * kFxd);
  EXPECT_EQ(ti.main_margin_req, 17017ULL * kFxd);
}
 
TEST_F(ConfigParserTest, ParsesDataStreamEnumsAndPaths) {
  AppConfig r = Parse(MakeValidConfig());
  ASSERT_EQ(r.data_configs.size(), 1);
  const auto& s = r.data_configs[0];
  EXPECT_EQ(s.data_source_name, "ES");
  EXPECT_EQ(s.schema, DataSchema::MBO);
  EXPECT_EQ(s.encoding, Encoding::CSV);
  EXPECT_EQ(s.compression, Compression::ZSTD);
  EXPECT_EQ(s.price_format, PriceFormat::DECIMAL);
  EXPECT_EQ(s.ts_format, TmStampFormat::UNIX);
  EXPECT_EQ(s.data_filepath, data_path.string());
}
 
TEST_F(ConfigParserTest, BuildsActiveInstrumentsFromSymbology) {
  AppConfig r = Parse(MakeValidConfig());
  std::vector<uint32_t> expected = {42140860, 42005050, 294973};
  EXPECT_EQ(r.active_instruments, expected);
}
 
//////////////////////////////////////////////////////////
// MARK: start_time / end_time
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, ThrowsOnMissingTimes) {
  for (const char* field : {"start_time", "end_time"}) {
    auto cfg = MakeValidConfig();
    cfg.erase(field);
    EXPECT_THROW(Parse(cfg), std::runtime_error) << "missing: " << field;
  }
}
 
TEST_F(ConfigParserTest, ThrowsWhenTimeIsWrongType) {
  auto cfg = MakeValidConfig();
  cfg["start_time"] = 12345;  // number, not ISO string
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ThrowsWhenStartEqualsEnd) {
  auto cfg = MakeValidConfig();
  cfg["end_time"] = cfg["start_time"];
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ThrowsWhenStartAfterEnd) {
  auto cfg = MakeValidConfig();
  cfg["start_time"] = "2024-01-01T16:00:00Z";
  cfg["end_time"] = "2024-01-01T09:30:00Z";
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ParsesFractionalSeconds) {
  // README: fractional seconds 0-9 digits allowed.
  auto cfg = MakeValidConfig();
  cfg["start_time"] = "2024-01-01T09:30:00.500000000Z";
  AppConfig r = Parse(cfg);
  EXPECT_EQ(r.start_time, 1704101400500000000ULL);
}
 
TEST_F(ConfigParserTest, ThrowsOnUnparseableTimestamp) {
  auto cfg = MakeValidConfig();
  cfg["start_time"] = "definitely-not-a-timestamp";
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}

TEST_F(ConfigParserTest, ThrowsWhenStartTimeHasTimezoneOffset) {
  auto cfg = MakeValidConfig();
  cfg["start_time"] = "2024-01-01T09:30:00+05:00";
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
//////////////////////////////////////////////////////////
// MARK: Execution latency ms
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, ExecutionLatencyDefault) {
  auto cfg = MakeValidConfig();  // omitted
  EXPECT_EQ(Parse(cfg).execution_latency_ms, 200);
}
 
TEST_F(ConfigParserTest, ExecutionLatencyCustom) {
  auto cfg = MakeValidConfig();
  cfg["execution_latency_ms"] = 100;
  EXPECT_EQ(Parse(cfg).execution_latency_ms, 100);
}
 
TEST_F(ConfigParserTest, ExecutionLatencyZeroIsAllowed) {
  auto cfg = MakeValidConfig();
  cfg["execution_latency_ms"] = 0;
  EXPECT_EQ(Parse(cfg).execution_latency_ms, 0);
}
 
TEST_F(ConfigParserTest, ExecutionLatencyNegativeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["execution_latency_ms"] = -5;
  EXPECT_EQ(Parse(cfg).execution_latency_ms, 200);
}
 
TEST_F(ConfigParserTest, ExecutionLatencyWrongTypeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["execution_latency_ms"] = "fast";
  EXPECT_EQ(Parse(cfg).execution_latency_ms, 200);
}
 
TEST_F(ConfigParserTest, ExecutionLatencyNullFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["execution_latency_ms"] = nullptr;
  EXPECT_EQ(Parse(cfg).execution_latency_ms, 200);
}
 
//////////////////////////////////////////////////////////
// MARK: Snapshot_interval_ms 
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, SnapshotIntervalDefault) {
  auto cfg = MakeValidConfig();  // omitted -> 1000ms
  EXPECT_EQ(Parse(cfg).snapshot_interval_ns, 1'000'000'000ULL);
}
 
TEST_F(ConfigParserTest, SnapshotIntervalCustomConvertedToNs) {
  auto cfg = MakeValidConfig();
  cfg["snapshot_interval_ms"] = 5000;
  EXPECT_EQ(Parse(cfg).snapshot_interval_ns, 5'000ULL * 1'000'000ULL);
}
 
TEST_F(ConfigParserTest, SnapshotIntervalCappedAtOneDay) {
  auto cfg = MakeValidConfig();
  cfg["snapshot_interval_ms"] = 2 * kOneDayMs; 
  // Capped to one day before the *1e6 conversion.
  EXPECT_EQ(Parse(cfg).snapshot_interval_ns, kOneDayMs * 1'000'000ULL);
}
 
TEST_F(ConfigParserTest, SnapshotIntervalExactlyOneDayNotCapped) {
  auto cfg = MakeValidConfig();
  cfg["snapshot_interval_ms"] = kOneDayMs;  // boundary: `>` so not capped
  EXPECT_EQ(Parse(cfg).snapshot_interval_ns, kOneDayMs * 1'000'000ULL);
}
 
TEST_F(ConfigParserTest, SnapshotIntervalNegativeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["snapshot_interval_ms"] = -1;
  EXPECT_EQ(Parse(cfg).snapshot_interval_ns, 1'000'000'000ULL);
}

TEST_F(ConfigParserTest, SnapshotIntervalWrongTypeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["snapshot_interval_ms"] = "100";
  EXPECT_EQ(Parse(cfg).initial_cash, 100000ULL * kFxd);
}
 
//////////////////////////////////////////////////////////
// MARK: Initial Cash
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, InitialCashDefaultScaled) {
  auto cfg = MakeValidConfig();  // omitted
  EXPECT_EQ(Parse(cfg).initial_cash, 100000ULL * kFxd);
}
 
TEST_F(ConfigParserTest, InitialCashCustomScaled) {
  auto cfg = MakeValidConfig();
  cfg["initial_cash"] = 250000;
  EXPECT_EQ(Parse(cfg).initial_cash, 250000ULL * kFxd);
}
 
TEST_F(ConfigParserTest, InitialCashZeroFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["initial_cash"] = 0;  // < 1 -> default, then scaled
  EXPECT_EQ(Parse(cfg).initial_cash, 100000ULL * kFxd);
}
 
TEST_F(ConfigParserTest, InitialCashNegativeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["initial_cash"] = -100;
  EXPECT_EQ(Parse(cfg).initial_cash, 100000ULL * kFxd);
}
 
TEST_F(ConfigParserTest, InitialCashWrongTypeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["initial_cash"] = "lots";
  EXPECT_EQ(Parse(cfg).initial_cash, 100000ULL * kFxd);
}
 
//////////////////////////////////////////////////////////
// MARK: Risk_free_rate
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, RiskFreeRateDefault) {
  EXPECT_DOUBLE_EQ(Parse(MakeValidConfig()).risk_free_rate, 0.05);
}
 
TEST_F(ConfigParserTest, RiskFreeRateCustom) {
  auto cfg = MakeValidConfig();
  cfg["risk_free_rate"] = 0.03;
  EXPECT_DOUBLE_EQ(Parse(cfg).risk_free_rate, 0.03);
}
 
TEST_F(ConfigParserTest, RiskFreeRateNegativeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["risk_free_rate"] = -0.01;
  EXPECT_DOUBLE_EQ(Parse(cfg).risk_free_rate, 0.05);
}

TEST_F(ConfigParserTest, RiskFreeRateRidiculous) {
  auto cfg = MakeValidConfig();
  cfg["risk_free_rate"] = 1000;
  EXPECT_THROW(Parse(cfg).risk_free_rate, std::runtime_error);
}
 
//////////////////////////////////////////////////////////
// MARK: log_file_path / report_output_dir (optional)
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, LogPathDefaultsToRootLogs) {
  auto cfg = MakeValidConfig();  // omitted -> kRootFolder/"logs" (absolute, kept)
  EXPECT_EQ(Parse(cfg).log_file_path, (kRootFolder / "logs").string());
}
 
TEST_F(ConfigParserTest, ReportDirDefaultsToRootReports) {
  auto cfg = MakeValidConfig();
  EXPECT_EQ(Parse(cfg).report_output_dir, (kRootFolder / "reports").string());
}
 
TEST_F(ConfigParserTest, EmptyStringPathUsesDefault) {
  auto cfg = MakeValidConfig();
  cfg["log_file_path"] = "";
  EXPECT_EQ(Parse(cfg).log_file_path, (kRootFolder / "logs").string());
}
 
TEST_F(ConfigParserTest, RelativePathResolvedAgainstConfigDir) {
  auto cfg = MakeValidConfig();
  cfg["log_file_path"] = "sub/logs";
  std::string expected =
      (tmp_dir / "sub" / "logs").lexically_normal().string();
  EXPECT_EQ(Parse(cfg).log_file_path, expected);
}
 
TEST_F(ConfigParserTest, RelativePathWithDotDotNormalized) {
  auto cfg = MakeValidConfig();
  cfg["report_output_dir"] = "../reports";
  std::string expected =
      (tmp_dir / ".." / "reports").lexically_normal().string();
  EXPECT_EQ(Parse(cfg).report_output_dir, expected);
}
 
TEST_F(ConfigParserTest, AbsolutePathLeftAsIs) {
  auto cfg = MakeValidConfig();
  cfg["log_file_path"] = "/var/tmp/btlogs";
  EXPECT_EQ(Parse(cfg).log_file_path, "/var/tmp/btlogs");
}
 
//////////////////////////////////////////////////////////
// MARK: Strategies 
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, ThrowsWhenStrategiesMissingOrEmptyOrNotArray) {
  {
    auto cfg = MakeValidConfig();
    cfg.erase("strategies");
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["strategies"] = nlohmann::json::array();
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["strategies"] = "MovAvgCrossMin";  // not an array
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
}
 
TEST_F(ConfigParserTest, ThrowsOnMissingRequiredStrategyField) {
  for (const char* field :
       {"name", "params", "traded_instr_id", "max_lob_lvl"}) {
    auto cfg = MakeValidConfig();
    cfg["strategies"][0].erase(field);
    EXPECT_THROW(Parse(cfg), std::runtime_error) << "missing: " << field;
  }
}
 
TEST_F(ConfigParserTest, ThrowsWhenStrategyParamsWrongType) {
  auto cfg = MakeValidConfig();
  cfg["strategies"][0]["params"] = "5,10";  // string, not int array
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ParsesMultipleStrategies) {
  auto cfg = MakeValidConfig();
  nlohmann::json second = cfg["strategies"][0];
  second["name"] = "MeanRevert";
  second["params"] = {20};
  cfg["strategies"].push_back(second);
  AppConfig r = Parse(cfg);
  ASSERT_EQ(r.strategies.size(), 2);
  EXPECT_EQ(r.strategies[1].name, "MeanRevert");
  EXPECT_EQ(r.strategies[1].params, (std::vector<int>{20}));
}
 
TEST_F(ConfigParserTest, RejectsStrategyTradingUnlistedInstrument) {
  auto cfg = MakeValidConfig();
  cfg["strategies"][0]["traded_instr_id"] = 999999;  // not in traded_instruments
  EXPECT_THROW(Parse(cfg), std::runtime_error);       
}
 
//////////////////////////////////////////////////////////
// MARK: Traded_instruments 
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, ThrowsWhenTradedInstrumentsMissingOrEmptyOrNotArray) {
  {
    auto cfg = MakeValidConfig();
    cfg.erase("traded_instruments");
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["traded_instruments"] = nlohmann::json::array();
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["traded_instruments"] = 294973;  // not an array
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
}
 
TEST_F(ConfigParserTest, ThrowsOnMissingRequiredTradedInstrumentField) {
  for (const char* field : {"instrument_id", "instrument_type", "tick_size",
                            "tick_value", "init_margin_req", "main_margin_req"}) {
    auto cfg = MakeValidConfig();
    cfg["traded_instruments"][0].erase(field);
    EXPECT_THROW(Parse(cfg), std::runtime_error) << "missing: " << field;
  }
}
 
TEST_F(ConfigParserTest, ThrowsWhenTickSizeIsZero) {
  auto cfg = MakeValidConfig();
  cfg["traded_instruments"][0]["tick_size"] = 0.0;
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ThrowsWhenTickValueIsZero) {
  auto cfg = MakeValidConfig();
  cfg["traded_instruments"][0]["tick_value"] = 0.0;
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ThrowsWhenInstrumentIdNegative) {
  auto cfg = MakeValidConfig();
  cfg["traded_instruments"][0]["instrument_id"] = -1;
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ThrowsWhenTickSizeNegative) {
  auto cfg = MakeValidConfig();
  cfg["traded_instruments"][0]["tick_size"] = -0.25;
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, ParsesStockAndOptionInstrumentTypes) {
  {
    auto cfg = MakeValidConfig();
    cfg["traded_instruments"][0]["instrument_type"] = "STOCK";
    EXPECT_EQ(Parse(cfg).traded_instruments[0].instrument_type,
              InstrumentType::STOCK);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["traded_instruments"][0]["instrument_type"] = "OPTION";
    EXPECT_EQ(Parse(cfg).traded_instruments[0].instrument_type,
              InstrumentType::OPTION);
  }
}
 
TEST_F(ConfigParserTest, ParsesMultipleTradedInstruments) {
  auto cfg = MakeValidConfig();
  nlohmann::json second = cfg["traded_instruments"][0];
  second["instrument_id"] = 10252;
  cfg["traded_instruments"].push_back(second);
  AppConfig r = Parse(cfg);
  ASSERT_EQ(r.traded_instruments.size(), 2u);
  EXPECT_EQ(r.traded_instruments[1].instrument_id, 10252u);
}
 
//////////////////////////////////////////////////////////
// MARK: Risk_Limits 
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, RiskLimitsDefaultsWhenAbsent) {
  AppConfig r = Parse(MakeValidConfig());  // kDefaultRiskLimits
  const auto& rl = r.risk_limits;
  EXPECT_EQ(rl.risk_mode, RiskMode::PercentOfAcct);
  EXPECT_EQ(rl.max_position_size, 3);
  EXPECT_EQ(rl.max_risk_per_trade_pct, 20'000'000);  // 0.02 * 1e9
  EXPECT_EQ(rl.max_portfolio_delta, 0);
  EXPECT_EQ(rl.max_drawdown_pct, 400'000'000);      
  EXPECT_EQ(rl.max_delta_per_trade, 0);
}
 
TEST_F(ConfigParserTest, RiskLimitsFullyParsed) {
  auto cfg = MakeValidConfig();
  cfg["risk_limits"] = {{"risk_mode", "PosSizeInDollars"},
                        {"max_position_size", 5},
                        {"max_risk_per_trade_pct", 0.5},   
                        {"max_portfolio_delta", 0.25},
                        {"max_drawdown_pct", 0.125},
                        {"max_delta_per_trade", 0.5}};
  const auto& rl = Parse(cfg).risk_limits;
  EXPECT_EQ(rl.risk_mode, RiskMode::PosSizeInDollars);
  EXPECT_EQ(rl.max_position_size, 5);
  EXPECT_EQ(rl.max_risk_per_trade_pct, 500'000'000);
  EXPECT_EQ(rl.max_portfolio_delta, 250'000'000);
  EXPECT_EQ(rl.max_drawdown_pct, 125'000'000);
  EXPECT_EQ(rl.max_delta_per_trade, 500'000'000);
}
 
TEST_F(ConfigParserTest, RiskLimitsPartialUsesPerFieldDefaults) {
  auto cfg = MakeValidConfig();
  cfg["risk_limits"] = {{"max_position_size", 7}};  // only one field set
  const auto& rl = Parse(cfg).risk_limits;
  EXPECT_EQ(rl.max_position_size, 7);
  EXPECT_EQ(rl.risk_mode, RiskMode::PercentOfAcct);   // "" -> default
  ExpectFixedPointNear(rl.max_risk_per_trade_pct, 0.02);
  ExpectFixedPointNear(rl.max_drawdown_pct, 0.4);   
}
 
TEST_F(ConfigParserTest, RiskLimitsNegativePositionSizeFallsBackToDefault) {
  auto cfg = MakeValidConfig();
  cfg["risk_limits"] = {{"max_position_size", -2}};
  EXPECT_EQ(Parse(cfg).risk_limits.max_position_size, 3); 
}
 
//////////////////////////////////////////////////////////
// MARK: Commissions 
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, CommissionDefaultsWhenAbsent) {
  const auto& c = Parse(MakeValidConfig()).commission_struct;
  ExpectFixedPointNear(c.fut_per_contract, 2.17);
  ExpectFixedPointNear(c.stock_clearing_fee, 0.0002);    
  ExpectFixedPointNear(c.stock_order_min, 0.35);     
  ExpectFixedPointNear(c.stock_per_share, 0.035);     
}
 
TEST_F(ConfigParserTest, CommissionFullyParsed) {
  auto cfg = MakeValidConfig();
  cfg["commissions"] = {{"fut_per_contract", 3.0},
                        {"stock_clearing_fee", 0.5},
                        {"stock_order_min", 0.25},
                        {"stock_per_share", 0.125}};
  const auto& c = Parse(cfg).commission_struct;
  EXPECT_EQ(c.fut_per_contract, 3'000'000'000ULL);
  EXPECT_EQ(c.stock_clearing_fee, 500'000'000ULL);
  EXPECT_EQ(c.stock_order_min, 250'000'000ULL);
  EXPECT_EQ(c.stock_per_share, 125'000'000ULL);
}
 
TEST_F(ConfigParserTest, CommissionPartialUsesPerFieldDefaults) {
  auto cfg = MakeValidConfig();
  cfg["commissions"] = {{"fut_per_contract", 1.0}};
  const auto& c = Parse(cfg).commission_struct;
  EXPECT_EQ(c.fut_per_contract, 1'000'000'000ULL);
  ExpectFixedPointNear(c.stock_clearing_fee, 0.0002);  
}
 
//////////////////////////////////////////////////////////
// MARK: Data Streams
//////////////////////////////////////////////////////////
TEST_F(ConfigParserTest, ThrowsWhenDataStreamsMissingOrEmptyOrNotArray) {
  {
    auto cfg = MakeValidConfig();
    cfg.erase("data_streams");
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"] = nlohmann::json::array();
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"] = "ES";
    EXPECT_THROW(Parse(cfg), std::runtime_error);
  }
}
 
TEST_F(ConfigParserTest, ThrowsOnMissingRequiredDataStreamField) {
  for (const char* field :
       {"data_source_name", "symbology_filepath", "data_filepath", "schema",
        "encoding", "compression", "price_format", "timestamp_format"}) {
    auto cfg = MakeValidConfig();
    cfg["data_streams"][0].erase(field);
    EXPECT_THROW(Parse(cfg), std::runtime_error) << "missing: " << field;
  }
}
 
TEST_F(ConfigParserTest, ParsesAlternateEnumValues) {
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"][0]["schema"] = "OHLCV";
    EXPECT_EQ(Parse(cfg).data_configs[0].schema, DataSchema::OHLCV);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"][0]["encoding"] = "DBN";
    EXPECT_EQ(Parse(cfg).data_configs[0].encoding, Encoding::DBN);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"][0]["encoding"] = "JSON";
    EXPECT_EQ(Parse(cfg).data_configs[0].encoding, Encoding::JSON);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"][0]["compression"] = "NONE";
    EXPECT_EQ(Parse(cfg).data_configs[0].compression, Compression::NONE);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"][0]["price_format"] = "fixpntint";
    EXPECT_EQ(Parse(cfg).data_configs[0].price_format, PriceFormat::FIXPNTINT);
  }
  {
    auto cfg = MakeValidConfig();
    cfg["data_streams"][0]["timestamp_format"] = "iso";
    EXPECT_EQ(Parse(cfg).data_configs[0].ts_format, TmStampFormat::ISO);
  }
}
 
TEST_F(ConfigParserTest, SymbologyRelativePathResolvedAgainstConfigDir) {
  // Reference the symbology by a path relative to the (temp) config dir.
  auto cfg = MakeValidConfig();
  cfg["data_streams"][0]["symbology_filepath"] = "symbology.csv";
  AppConfig r = Parse(cfg);
  ASSERT_EQ(r.data_configs.size(), 1u);
  EXPECT_EQ(r.data_configs[0].data_symbology.size(), 3u);
}
 
TEST_F(ConfigParserTest, MultipleDataStreamsConcatenateActiveInstruments) {
  auto sym2 = tmp_dir / "sym2.csv";
  WriteFile(sym2,
            "symbol,instrument_id,date\n"
            "NQH7,11111,2025-11-05\n"
            "NQM7,22222,2025-11-05\n");
  auto cfg = MakeValidConfig();
  nlohmann::json stream2 = cfg["data_streams"][0];
  stream2["data_source_name"] = "NQ";
  stream2["symbology_filepath"] = sym2.string();
  cfg["data_streams"].push_back(stream2);
 
  AppConfig r = Parse(cfg);
  ASSERT_EQ(r.data_configs.size(), 2u);
  std::vector<uint32_t> expected = {42140860, 42005050, 294973, 11111, 22222};
  EXPECT_EQ(r.active_instruments, expected);
}
 
//////////////////////////////////////////////////////////
// MARK: Symbology CSV parsing (ParseDataSymbols via the data-stream path)
//////////////////////////////////////////////////////////
 
TEST_F(ConfigParserTest, SymbologyParsesFieldsAndSkipsHeader) {
  AppConfig r = Parse(MakeValidConfig());
  const auto& syms = r.data_configs[0].data_symbology;
  ASSERT_EQ(syms.size(), 3u);  // header skipped
  EXPECT_EQ(syms[0].symbol, "ESH7");
  EXPECT_EQ(syms[0].instrument_id, 42140860u);
  EXPECT_EQ(syms[0].date, "2025-11-05");
  EXPECT_EQ(syms[2].symbol, "ESZ5");
  EXPECT_EQ(syms[2].instrument_id, 294973u);
}
 
TEST_F(ConfigParserTest, ThrowsWhenSymbologyFileMissing) {
  auto cfg = MakeValidConfig();
  cfg["data_streams"][0]["symbology_filepath"] =
      (tmp_dir / "does_not_exist.csv").string();
  EXPECT_THROW(Parse(cfg), std::runtime_error);
}
 
TEST_F(ConfigParserTest, SymbologyRowsWithBadIdAreSkipped) {
  auto bad = tmp_dir / "bad_sym.csv";
  WriteFile(bad,
            "symbol,instrument_id,date\n"
            "GOOD,123,2025-01-01\n"
            "BAD,notanumber,2025-01-01\n"
            "GOOD2,456,2025-01-01\n");
  auto cfg = MakeValidConfig();
  cfg["data_streams"][0]["symbology_filepath"] = bad.string();
  AppConfig r = Parse(cfg);
  ASSERT_EQ(r.data_configs[0].data_symbology.size(), 2u);  // bad row dropped
  EXPECT_EQ(r.active_instruments, (std::vector<uint32_t>{123, 456}));
}
 
TEST_F(ConfigParserTest, SymbologyMalformedRowsMissingColumnsAreSkipped) {
  auto bad = tmp_dir / "short_rows.csv";
  WriteFile(bad,
            "symbol,instrument_id,date\n"
            "ONLYSYMBOL\n"
            "SYM,789\n"            // missing date
            "FULL,321,2025-02-02\n");
  auto cfg = MakeValidConfig();
  cfg["data_streams"][0]["symbology_filepath"] = bad.string();
  AppConfig r = Parse(cfg);
  ASSERT_EQ(r.data_configs[0].data_symbology.size(), 1u);
  EXPECT_EQ(r.data_configs[0].data_symbology[0].symbol, "FULL");
}
 
}  
