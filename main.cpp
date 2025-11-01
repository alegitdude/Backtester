#include "core/EventQueue.cpp"
#include "core/Backtester.h"
#include "core/ConfigParser.h"
#include "core/Types.h"
#include "market_state/MarketStateManager.h"
#include "strategy/StrategyManager.h"
#include "data_ingestion/DataReaderManager.h"
#include "execution/ExecutionHandler.h"
#include "portfolio/PortfolioManager.h"
#include "reporting/ReportGenerator.h"
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <iostream>
#include <vector>
#include <string>
#include <charconv>
#include <memory>
#include <fstream>

class DataLoader{
    private:
        static int64_t nanosToSeconds(int64_t nanos){
            return nanos / 1'000'000'000;
        };

    public:
        static int64_t parseDbIsoFormat(const char* str){
            // Assumed format of 2025-06-25T08:00:10.000000000Z --DB has same format for every OHLCV
            //                   YYYY-MM-DDTHH:MM:SS.sssz

            // Convert all datetime str values to ints
            int year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + 
               (str[2] - '0') * 10 + (str[3] - '0');
            int month = (str[5] - '0') * 10 + (str[6] - '0');
            int day = (str[8] - '0') * 10 + (str[9] - '0');
            int hour = (str[11] - '0') * 10 + (str[12] - '0');
            int minute = (str[14] - '0') * 10 + (str[15] - '0');
            int second = (str[17] - '0') * 10 + (str[18] - '0');

            // Convert to tm struct
            std::tm tm = {};
            tm.tm_year = year - 1900;  // tm_year is years since 1900
            tm.tm_mon = month - 1;      // tm_mon is 0-11
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;

            return timegm(&tm); 
        }

        static std::vector<OHLCBar> loadCsv(const std::string& filePath, DataInterval& interval){
            //Try to open the file
            FILE* file = fopen(filePath.c_str(), "r");
            //Check if the filepath is real and openable + opened
            if (!file){
                throw std::runtime_error("Failed to open file at:" + filePath);
            }

            //Declare return object
            std::vector<OHLCBar> bars;
            //Pre allocate the amount of time in the bars
            //Seconds for one day 234000, TODO generalize later
            bars.reserve(234000);

            //Skip headers
            char line[512];
            fgets(line, sizeof(line), file);  // Skip header
        
            while (fgets(line, sizeof(line), file)) {       
                OHLCBar bar;
                char ts_event[64];
                // Skip rtype, publisher_id, instrument_id
                if (sscanf(line, "%63[^,],%*d,%*d,%*d,%lf,%lf,%lf,%lf,%lf",
                        ts_event,
                        &bar.open,
                        &bar.high,
                        &bar.low,
                        &bar.close,
                        &bar.volume) == 6) {
                    
                        bar.timestamp = parseDbIsoFormat(ts_event);
                        bars.push_back(bar);
                    }
               
            }   

            fclose(file);
            return bars;     
        };
    };
    


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
    std::string config_path_string = GetEnvVarConfigPath();
    Config config = ConfigParser::ParseConfigToObj(config_path_string);
    
    ///  Initialize Logger 
    setup_logging();
    spdlog::info("Logger Initialized");
    ///  Configuration Loading 
    spdlog::info("Loading Confgiuration File");
    ///  Component Initialization 

    ///  Create central EventQueue
    EventQueue event_queue;
    /// Initialize Data Reader(s) and possibly DataReaderManager?
	DataReaderManager data_reader_manager(event_queue);
    /// Initialize Market State Manager
    MarketStateManager market_state_manager;
    /// Initialize Portfolio Manager
	PortfolioManager portfolio_manager(10000);
    /// Initialize Report Generator
	ReportGenerator report_generator;
    /// Initialize Execution Handler
	ExecutionHandler execution_handler;
    /// Initialize Strategy Manager and specific Strategy
    StrategyManager strategy_manager;
    
    // Read the first event from each data source and add to the EventQueue
    spdlog::info("Populating initial events from data sources...");
    data_reader_manager.register_and_init_streams(config.data_symbs_and_paths); // This method reads one event from each reader and pushes to event_queue
    
    // Initialize Backtester class
    Backtester backtester(event_queue, data_reader_manager, market_state_manager,
                          portfolio_manager, report_generator, execution_handler,
                          strategy_manager);
    
    return 0;
}


namespace ConfigParser {
  const Config ParseConfigToObj(std::string& config_path){
		auto config_abs_path = std::filesystem::path(config_path); 
		
		using json = nlohmann::json;

		if (std::filesystem::exists(config_abs_path)) {
				std::cout << "The file '" << config_abs_path << "' exists." << std::endl;
		} else {
				std::cout << "The file '" << config_abs_path << "' does not exist." << std::endl;
				throw std::runtime_error("Config file not found at: " + config_path);
		}

		std::ifstream f(config_abs_path);
		if (!f.is_open()) {
			throw std::runtime_error("Config file does not open: ");
		}

		json data = json::parse(f);
		//std::cout << data.dump(4) << std::endl;  /// For testing 
		
	    Config config;
		//config.data_format = data["data_format"];
		config.end_time = data["end_time"];
		config.execution_latency = data["execution_latency"];
		config.initial_cash = data["initial_cash"];
		config.log_file_path = data["log_file_path"];
		config.report_output_dir = data["report_output_dir"];
		config.start_time = data["start_time"];
		config.strategy_name = data["strategy_name"];
		config.traded_symbol = data["traded_symbol"];	
        config.data_symbs_and_paths = data["data_symbs_and_paths"];
        
		return config;
  }
}