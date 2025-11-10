//#include "core/EventQueue.cpp"
#include "core/Backtester.h"
#include "core/ConfigParser.h"
#include "core/Types.h"
#include "market_state/MarketStateManager.h"
#include "strategy/StrategyManager.h"
#include "execution/ExecutionHandler.h"
#include "portfolio/PortfolioManager.h"
#include "reporting/ReportGenerator.h"
#include "core/ConfigParser.h"
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <iostream>
#include <vector>
#include <string>
#include <charconv>
#include <memory>
#include <fstream>

    
std::string GetEnvVarConfigPath () {
    namespace fs = std::filesystem;
    fs::path current_dir = fs::current_path();
    fs::path parent_dir = current_dir.parent_path();
    fs::path env = ".env";
    std::ifstream file(parent_dir / env);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open environment variable file:";
        return "";
    }
    std::string csvFilePath;
    std::string line;
    while (std::getline(file, line)) {
        size_t equalsPos = line.find('=');
        if (equalsPos != std::string::npos) {
            std::string key = line.substr(0, equalsPos - 1);
            std::string value = line.substr(equalsPos + 2);
            if(key == "PATH_TO_CONFIG"){
                return value;
            }
        }
    }
    file.close();
    return "";
}

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
    std::cout << "Here are the required arguments for running a backtest" << std::endl;
    std::cout << "--MboFilePath" << std::endl;
    std::cout << "--ResultsFolderPath" << std::endl;
    std::cout << "--Strategy" << std::endl;
    std::cout << "Here are the optional arguments for running a backtest" << std::endl;
}

void setup_logging() {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("simulation.log", true);
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

    // std::filesystem::path config_path;
    // for (int i = 1; i < argc; ++i) { // Start from 1 to skip program name
    //     std::string arg = argv[i];

    //     if (arg == "--help" || arg == "-h") {
    //         printHelp();
    //         return 0; 
    //     }
    //     if(i == 1){ //ConfigFilePath
    //         if(!std::filesystem::exists(arg)){
    //             std::cout << "Config file path does not exist!" << std::endl;
    //             std::cout << arg << std::endl;
    //             return 0;
    //         }
    //         config_path = std::filesystem::path(arg);
    //         // if(mboPath.extension() != "csv" || mboPath.extension() != "dbn"){
    //         //     std::cout << "Mbo File has wrong file extension!" << std::endl;
    //         //     std::cout << arg << std::endl;
    //         //     return 0;
    //         // }
    //     }
    // }
    
    ///  Argument Parsing & Initial Setup 
    //std::string config_path_string = GetEnvVarConfigPath();
    std::string config_path_string = "../config/config_1.json";
    const backtester::AppConfig config = backtester::ParseConfigToObj(config_path_string);
    ///  Initialize Logger 
    setup_logging();
    spdlog::info("Logger Initialized");
    ///  Configuration Loading 
    spdlog::info("Loading Confgiuration File");
    ///  Component Initialization 

    ///  Create central EventQueue
    backtester::EventQueue event_queue;
    /// Initialize Data Reader(s) and possibly DataReaderManager?
	backtester::DataReaderManager data_reader_manager(event_queue);
    /// Initialize Market State Manager
    backtester::MarketStateManager market_state_manager;
    /// Initialize Portfolio Manager
	backtester::PortfolioManager portfolio_manager(10000);
    /// Initialize Report Generator
	backtester::ReportGenerator report_generator;
    /// Initialize Execution Handler
	backtester::ExecutionHandler execution_handler;
    /// Initialize Strategy Manager and specific Strategy
    backtester::StrategyManager strategy_manager;
    
    // Read the first event from each data source and add to the EventQueue
    spdlog::info("Populating initial events from data sources...");
    data_reader_manager.RegisterAndInitStreams(config.data_streams);
    
    // Initialize Backtester class
    backtester::Backtester backtester(event_queue, data_reader_manager, market_state_manager,
                          portfolio_manager, report_generator, execution_handler,
                          strategy_manager);
    
    backtester.RunLoop(config);
    
    return 0;
}




