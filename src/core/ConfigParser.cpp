#include "../../include/core/ConfigParser.h"
#include "../../include/utils/TimeUtils.h"
#include "../../include/utils/NumericUtils.h"
#include "../../include/core/Types.h"
#include "../../include/core/DefaultConfig.h"
#include "spdlog/spdlog.h"
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

AppConfig ParseConfigFromJson(const nlohmann::json& data){
	std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    for (const auto& field : kRequiredConfigFields) {
        if (!data.contains(field)) {
            errors.push_back("Missing required field: '" + field + "'");
        }
    }
    
    for (const auto& field : kOptionalConfigFields) {
        if (!data.contains(field)) {
            warnings.push_back("'" + field + "' not specified, using default");
        }
    }
    
    if (data.contains("data_streams")) {
        int index = 0;
        for (const auto& stream : data["data_streams"]) {
            std::string prefix = "data_streams[" + std::to_string(index) + "]";
            
            for (const auto& field : kRequiredDataStreamFields) {
                if (!stream.contains(field)) {
                    errors.push_back(prefix + ": missing required field '" + field + "'");
                }
            }
            
            for (const auto& field : kOptionalDataStreamFields) {
                if (!stream.contains(field)) {
                    warnings.push_back(prefix + ": '" + field + "' not specified, using default");
                }
            }
            index++;
        }
    } 
    
    for (const auto& w : warnings) {
        spdlog::warn("{}", w);
    }
    
    if (!errors.empty()) {
        std::string msg = "Config validation failed:\n";
        for (const auto& e : errors) {
            msg += "  - " + e + "\n";
        }
        throw std::runtime_error(msg);
    }

	AppConfig config;
	config.start_time = time::ParseIsoToUnix(data["start_time"]); // TODO Double check est
	config.end_time = time::ParseIsoToUnix(data["end_time"]); // TODO Double check est

	config.execution_latency_ms = data["execution_latency"];
	config.initial_cash = data["initial_cash"];
	config.log_file_path = data["log_file_path"];
	config.report_output_dir = data["report_output_dir"];
	
	config.strategies = ParseStrategies(data["strategies"]);
	config.traded_instruments = ParseTradedInstrs(data["traded_instruments"]);	

	config.risk_limits = ParseRiskLimits(data["risk_limits"]);

	for (const auto& item : data["data_streams"]) {
		DataSourceConfig data_config;
		data_config.data_source_name = item["data_source_name"];
		data_config.data_symbology = ParseDataSymbols(item["symbology_filepath"]);
		data_config.data_filepath = item["data_filepath"];
		data_config.schema = StrToDataSchema(item["schema"]);
		data_config.encoding = StrToEncoding(item["encoding"]);
		data_config.compression = StrToCompression(item["compression"]);
		data_config.price_format = StrToPriceFormat(item["price_format"]);
		data_config.ts_format = StrToTSFormat(item["timestamp_format"]); 
		config.data_configs.push_back(data_config);
	};

	for(const auto& data_stream : config.data_configs){
		for(const auto& symbology : data_stream.data_symbology){
			config.active_instruments.push_back(symbology.instrument_id);
		}
	}
	return config;
}

AppConfig ParseConfigToObj(std::filesystem::path& config_path){
	using json = nlohmann::json;

	if (!std::filesystem::exists(config_path)) {
		std::cout << "The file '" << config_path.c_str() << "' does not exist." << std::endl;
		throw std::runtime_error("Config file not found at: " + config_path.generic_string());
	}

	std::ifstream f(config_path);
	if (!f.is_open()) {
		throw std::runtime_error("Config file does not open at: " + config_path.generic_string());
	}
	
	json data; 
	try{
		data = json::parse(f);
	}
	catch(json::exception e){
		throw std::runtime_error("Config file is not valid JSON at: " + config_path.generic_string());
	}
	return ParseConfigFromJson(data);
}

std::vector<Strategy> ParseStrategies(const nlohmann::json& data) {
	std::vector<Strategy> res;
	if(data.contains("")){}

	for (const auto& item : data) {
		Strategy strat;
		strat.name = item["name"];
		std::vector<int> params;
		for(const auto& param : item["params"]){
			params.push_back(param);
		}
		strat.params = params;
		strat.max_lob_lvl = item["max_lob_lvl"];
		res.push_back(strat);
	}	
	return res; 
}

std::vector<TradedInstrument> ParseTradedInstrs(const nlohmann::json& data) {
	std::vector<TradedInstrument> res;
	for(const auto& item : data){
		for(std::string field : kRequiredTradedInstrFields){
			if(!data.contains(field)){
				spdlog::error("field {} of traded instrument is not parsable ", field);	
				throw std::runtime_error("Traded Instrument in config file is not valid: ");
			}
		}	
		TradedInstrument instr;
		instr.instrument_id = data["instrument_id"];
		instr.instrument_type = ParseInstrType(data["instrument_type"]);
		instr.tick_size = data["tick_size"];
		instr.tick_value = data["tick_value"];
		instr.margin_req = data["margin_req"];

		res.push_back(instr);
	}
	return res;
}

RiskLimits ParseRiskLimits(const nlohmann::json& data) {
	std::vector<std::string> absent_fields;
	RiskLimits res;
	for(std::string field : kRiskLimitsFields){
		if(!data.contains(field)){
			spdlog::warn("field {} of riskLimits is not parsable, using default risk limits ", field);	
			AppConfig config = GetDefaultConfig();
			return config.risk_limits;
		}
	}

	res.risk_mode = ParseRiskMode(data["risk_mode"]);
	res.max_position_size = numericUtils::doubleToFixedPoint(data["max_position_size"]);
	res.max_risk_per_trade_pct = numericUtils::doubleToFixedPoint(data["max_risk_per_trade_pct"]);
	res.max_portfolio_delta = numericUtils::doubleToFixedPoint(data["max_portfolio_delta"]);
	res.max_drawdown_pct = numericUtils::doubleToFixedPoint(data["max_drawdown_pct"]);
	res.max_delta_per_trade = numericUtils::doubleToFixedPoint(data["max_delta_per_trade"]);

	return res;
}

}