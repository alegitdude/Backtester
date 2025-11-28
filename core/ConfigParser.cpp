#include "ConfigParser.h"
#include "../../../utils/TimeUtils.h"
#include "Types.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
   
namespace backtester{

AppConfig ParseConfigToObj(std::filesystem::path& config_path){
	using json = nlohmann::json;

	if (std::filesystem::exists(config_path)) {
		std::cout << "The file '" << config_path.c_str() << "' exists." << std::endl;
	} else {
		std::cout << "The file '" << config_path.c_str() << "' does not exist." << std::endl;
		throw std::runtime_error("Config file not found at: " + config_path.generic_string());
	}

	std::ifstream f(config_path);
	if (!f.is_open()) {
		throw std::runtime_error("Config file does not open: ");
	}
	
	json data = json::parse(f);
	//std::cout << data.dump(4) << std::endl;  /// For testing 
		
	AppConfig config;
	config.end_time = time::ParseIsoToUnix(data["end_time"]); // TODO Double check est

	config.execution_latency = data["execution_latency"];
	config.initial_cash = data["initial_cash"];
	config.log_file_path = data["log_folder_path"];
	config.report_output_dir = data["report_output_dir"];
	config.start_time = time::ParseIsoToUnix(data["start_time"]); // TODO Double check est
	config.strategy_name = data["strategy_name"];
	config.traded_symbol = data["traded_symbol"];	

	for (const auto& item : data["data_streams"]) {
		DataSourceConfig data_config;
		data_config.symbol = item["symbol"];
		data_config.filepath = item["filepath"];
		data_config.schema = StrToDataSchema(item["schema"]);
		data_config.encoding = StrToEncoding(item["encoding"]);
		data_config.compression = StrToCompression(item["compression"]);
		data_config.price_format = StrToPriceFormat(item["price_format"]);
		data_config.ts_format = StrToTSFormat(item["timestamp_format"]); 
		config.data_streams.push_back(data_config);
	};
	
	return config;
};

}