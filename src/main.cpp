//#include "core/EventQueue.cpp"
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

namespace PrintHelper {
    inline void BacktestStart(){
        std::cout << "Backtester Program Started" << std::endl;
    };
    inline void CantOpenConfig(){
        std::cout << "Could not open config, check path" << std::endl;
    };
    inline void BadFilePath(){
        std::cout << "Could not open file, check path" << std::endl;
    };
    inline void WrongArgNums(){
        std::cout << R"(Please include the correct number of 
                        arguments to run backtester)" << std::endl;
    };
};

void printHelp() {
    std::cout << "A settings file in json format is required to use this backtester" << std::endl;
    std::cout << "Here are the optional arguments for running a backtest" << std::endl;
}

void SetupLogging() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime_t = std::chrono::system_clock::to_time_t(now);
    std::string time_string = std::ctime(&currentTime_t);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("../logs/" + time_string + ".simulation.log", true);
    auto logger = std::make_shared<spdlog::logger>("main_logger", file_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
}


int main(int argc, char* argv[]) {
	const int maxArgs = 3;
    PrintHelper::BacktestStart();

    // if(argc < 2 || argc > maxArgs){
    //     PrintHelper::WrongArgNums();
    //     printHelp();
    //     return 0;
    // }

    std::filesystem::path config_path;
    std::filesystem::path default_settings = "../config/config_1.json";

    for (int i = 1; i < argc; ++i) { // Start from 1 to skip program name
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0; 
        }
        if(i == 1){ //ConfigFilePath
            if(!std::filesystem::exists(arg)){
                std::cout << "Config file path does not exist!" << std::endl;
                std::cout << arg << std::endl;
                config_path = default_settings;
            }
            config_path = std::filesystem::path(arg);
            if(config_path.extension() != "json" && config_path.extension() != "yaml"){
                std::cout << "Config file has wrong file extension! Needs to be json" << std::endl;
                std::cout << arg << std::endl;
                config_path = default_settings;
            }
        }
        if(std::filesystem::is_empty(config_path)){
            config_path = default_settings;
        }
    }
    
    ///  Argument Parsing & Initial Setup 
    ///  Initialize Logger 
    SetupLogging();
    spdlog::info("Logger Initialized");
    ///  Configuration Loading 
    spdlog::info("Loading Confgiuration File");
    ///  Component Initialization 
    const backtester::AppConfig config = backtester::ParseConfigToObj(config_path);
    ///  Create central EventQueue
    backtester::EventQueue event_queue;
    /// Initialize Data Reader(s) and possibly DataReaderManager?
	backtester::DataReaderManager data_reader_manager(event_queue);

    /// Initialize Market State Manager
    backtester::MarketStateManager market_state_manager;
    std::vector<uint32_t> traded_instr_ids;
    traded_instr_ids.reserve(config.traded_instruments.size());
    std::transform(config.traded_instruments.begin(), config.traded_instruments.end(), std::back_inserter(ids),
               [](const auto& obj) { return obj.instrument_id; });

    market_state_manager.Initialize(config.active_instruments, traded_instr_ids);

    /// Initialize Portfolio Manager
	backtester::PortfolioManager portfolio_manager(config);
    /// Initialize Report Generator
	backtester::ReportGenerator report_generator;
    /// Initialize Execution Handler
	backtester::ExecutionHandler execution_handler;
    /// Initialize Strategy Manager and specific Strategy
    backtester::StrategyManager strategy_manager(execution_handler, config.strategies);
    strategy_manager.InitiailizeStrategies();
    
    // Read the first event from each data source and add to the EventQueue
    spdlog::info("Populating initial events from data sources...");
    data_reader_manager.RegisterAndInitStreams(config.data_configs);
    
    // Initialize Backtester class
    backtester::Backtester backtester(event_queue, data_reader_manager, market_state_manager,
                          portfolio_manager, report_generator, execution_handler,
                          strategy_manager);
    
    backtester.RunLoop(config);
    
    return 0;
}




