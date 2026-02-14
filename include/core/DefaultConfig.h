#include "Types.h"

namespace backtester{

AppConfig GetDefaultConfig() {
    AppConfig res;
    res.start_time = 1762300800000000000; // Cash open 11-5-25
    res.end_time = 1762305154417285368; //
    res.execution_latency_ms = 200;
    res.initial_cash = 100000'000'000'000;
    res.log_file_path = "../logs";
    res.report_output_dir = "../include/reporting";

    Strategy mov_avg_cross;
    mov_avg_cross.name = "MovAvgCrossMin";
    mov_avg_cross.params = {2, 5};
    mov_avg_cross.max_lob_lvl = 1;
    std::vector<Strategy> strategies = {mov_avg_cross};
    res.strategies = strategies;

    res.max_lob_lvl = 1;

    TradedInstrument traded_instr;
    traded_instr.instrument_id = 294973;
    traded_instr.instrument_type = InstrumentType::FUT;
    traded_instr.tick_size = 250000000;
    traded_instr.tick_value = 12'500'000'000;
    traded_instr.margin_req = 16500'000000000;
    res.traded_instruments = {traded_instr};

    RiskLimits risk_limits;
    risk_limits.risk_mode = RiskMode::PercentOfAcct;
    risk_limits.max_position_size = 0;
    risk_limits.max_risk_per_trade_pct = 20'000'000;
    risk_limits.max_portfolio_delta = 0;
    risk_limits.max_drawdown_pct = 10'000'000;
    risk_limits.max_delta_per_trade = 0;
    res.risk_limits = risk_limits;

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
    res.data_configs = {stream};

    std::vector<uint32_t> active_instrs = {42140860, 42005050, 42008149, 42018261, 
        294973, 42033138, 42007293, 42005583, 42006493, 42006263, 42140863,
        42007065, 42008507, 42016712, 42140879, 42140856, 42016422, 42006622, 10252,
        42007174, 42004134, 42009453, 42018017, 42004092, 42000977, 42004810, 42140878,
        42004613, 42140864, 42009476, 17740, 42140874, 42004653, 42000746, 42018129, 42140861,
        42004904, 42140870, 42008091, 42002687, 42011265};
    res.active_instruments = active_instrs;
    
    return res;
}
    

}
