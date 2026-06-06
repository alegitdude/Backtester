#include "../include/core/Backtester.h"
#include "../include/core/ConfigParser.h"
#include "../include/core/Types.h"
#include "../include/market_state/MarketStateManager.h"
#include "../include/strategy/StrategyManager.h"
#include "../include/execution/ExecutionHandler.h"
#include "../include/portfolio/PortfolioManager.h"
#include "../include/reporting/ReportGenerator.h"
#include "../include/core/ConfigParser.h"
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <iostream>
#include <vector>
#include <string>
#include <charconv>
#include <memory>
#include <fstream>

void SetupLogging(std::string log_path) {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime_t = std::chrono::system_clock::to_time_t(now);
    std::string time_string = std::ctime(&currentTime_t);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path 
        + "/" + time_string + ".log", true);
    auto logger = std::make_shared<spdlog::logger>("main_logger", file_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::info);
}

int main(int argc, char* argv[]) {
    spdlog::info("Backtester Program Started");

    if (argc != 2) {
        spdlog::error(R"(Usage: {} <path to config.json> 
            Try running the included demo from the build folder: 
            {} ../config/demo.json)", 
            argv[0], argv[0]);
        return 1;
    }

    std::filesystem::path config_path;    
    std::string arg = argv[1];
    if (arg == "-h" || arg == "--help") {
        spdlog::info(R"(Usage: {} [path to config.json]
            Runs a backtest with the specified configuration.)");
        return 0;
    }
    config_path = arg;

    if (!std::filesystem::exists(config_path)) {
        spdlog::error("Config file not found: {}", config_path.generic_string());
        return 1;
    }
    if (config_path.extension() != ".json") {
        spdlog::error("Config file must have .json extension: {}", 
            config_path.generic_string());
        return 1;
    }
    
    ///  Configuration Loading 
    spdlog::info("Loading Configuration File");
    const backtester::AppConfig config = backtester::ParseConfigToObj(config_path);

    ///  Initialize Logger 
    spdlog::info("Logs outputting to: {}", config.log_file_path);
    SetupLogging(config.log_file_path);
    spdlog::info("Logger Initialized");

    ///  Create central EventQueue
    backtester::EventQueue event_queue;
    /// Initialize DataReaderManager
    backtester::DataReaderManager data_reader_manager;
    /// Initialize Market State Manager
    backtester::MarketStateManager market_state_manager;
    market_state_manager.Initialize(config.active_instruments);

    /// Initialize Portfolio Manager
    backtester::PortfolioManager portfolio_manager(config);
    /// Initialize Report Generator
    backtester::ReportGenerator report_generator(config);
    /// Initialize Execution Handler
    backtester::ExecutionHandler execution_handler(event_queue, config);
    /// Initialize Strategy Manager and Strategies
    backtester::StrategyManager strategy_manager(config);
    strategy_manager.InitializeStrategies(market_state_manager);

    if (!data_reader_manager.RegisterAndInitStreams(config.data_configs)) {
        throw std::runtime_error("Problem parsing data configuration, check logs");
        return 1;
    };

    // Initialize Backtester class
    backtester::Backtester backtester(event_queue, data_reader_manager, market_state_manager,
        portfolio_manager, report_generator, execution_handler,
        strategy_manager);

    backtester.RunLoop(config);

    return 0;
}




