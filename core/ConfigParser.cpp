#include "ConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>

const Config ConfigParser::ParseConfigToObj(std::string config_path){
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
	config.data_format = data["data_format"];
	config.end_time = data["end_time"];
	config.execution_latency = data["execution_latency"];
	config.initial_cash = data["initial_cash"];
	config.log_file_path = data["log_file_path"];
	config.report_output_dir = data["report_output_dir"];
	config.start_time = data["start_time"];
	config.strategy_name = data["strategy_name"];
	config.traded_symbol = data["traded_symbol"];	

	return config;
}