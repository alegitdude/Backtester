#include "ConfigParser.h"
#include "../../../utils/TimeUtils.h"
#include "Types.h"
#include <nlohmann/json.hpp>
#include <fstream>
   
namespace backtester{

AppConfig ParseConfigToObj(std::string& config_path){
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
	
	AppConfig config;
	config.end_time = time::StringToEpoch(data["end_time"], "EST");
	config.execution_latency = data["execution_latency"];
	config.initial_cash = data["initial_cash"];
	config.log_file_path = data["log_file_path"];
	config.report_output_dir = data["report_output_dir"];
	config.start_time = time::StringToEpoch(data["start_time"], "EST");
	config.strategy_name = data["strategy_name"];
	config.traded_symbol = data["traded_symbol"];	
	for (const auto& item : data["data_streams"]) {
		DataSourceConfig data_config;
		data_config.symbol = item["symbol"];
		data_config.filepath = item["filepath"];
		data_config.format = StringToDataFormat(item["format"]); 
		config.data_streams.push_back(data_config);
	};
	return config;
};

DataFormat StringToDataFormat(const std::string& str) {
	if (str == "MBO") return DataFormat::MBO;
    if (str == "OHLCV") return DataFormat::OHLCV;
    throw std::invalid_argument("Invalid format: " + str);
};

}