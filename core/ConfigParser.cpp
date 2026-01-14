#include "ConfigParser.h"
#include "../utils/TimeUtils.h"
#include "Types.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

namespace backtester{

std::vector<Symbol> ParseDataSymbols(const std::string& filepath){
        std::vector<Symbol> instruments;
        
        std::ifstream file(filepath, std::ios::in);

        if (!file.is_open()) {
            std::cerr << "Could not open the file!" << std::endl;
            return instruments;
        }

        std::string line;
		uint32_t count = 0;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string symbol, instrument, date;
            Symbol row;
			count++;
			if(count == 1) {continue;}
            if (std::getline(ss, symbol, ',') &&
                std::getline(ss, instrument, ',') &&
                std::getline(ss, date)) { 
                
                 try {
                    row.symbol = symbol;
                    row.instrument_id = static_cast<uint32_t>(std::stoul(instrument));
                    row.date = date;
                    
                    instruments.push_back(row);
                } catch (const std::exception& e) {
                    // Handles cases where column 2 is not a valid number
                    std::cerr << "Skipping row due to conversion error: " << e.what() << std::endl;
                }
            }
        }

        file.close();
        return instruments;
}

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
		data_config.data_symbology = ParseDataSymbols(item["symbol_filepath"]);
		data_config.data_filepath = item["data_filepath"];
		data_config.schema = StrToDataSchema(item["schema"]);
		data_config.encoding = StrToEncoding(item["encoding"]);
		data_config.compression = StrToCompression(item["compression"]);
		data_config.price_format = StrToPriceFormat(item["price_format"]);
		data_config.ts_format = StrToTSFormat(item["timestamp_format"]); 
		config.data_streams.push_back(data_config);
	};

	for(const auto& data_stream : config.data_streams){
		for(const auto& symbology : data_stream.data_symbology){
			config.active_instruments.push_back(symbology.instrument_id);
		}
	}
	return config;
};

}