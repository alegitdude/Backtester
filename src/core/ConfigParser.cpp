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
	AppConfig config;
	// MARK: Start Time
	std::string start_str = GetRequired<std::string>(data, "start_time", "Global Settings");
	auto start_result = time::ParseIsoToUnix(start_str); //TODO double check est
	if (!start_result.success) {
    	throw std::runtime_error(fmt::format("Config 'start_time' error: {} in string '{}'", 
                             start_result.error_msg, data["start_time"]));
	}
	config.start_time = start_result.unix_nanos;

	// MARK: End Time
	std::string end_str = GetRequired<std::string>(data, "end_time", "Global Settings");
	auto end_result = time::ParseIsoToUnix(end_str);
	if (!end_result.success) {
    	throw std::runtime_error(fmt::format("Config 'end_time' error: {} in string '{}'", 
                             start_result.error_msg, data["end_time"]));
	}
	config.end_time = end_result.unix_nanos;

	if(config.start_time >= config.end_time){
		throw std::runtime_error(fmt::format("Config start_time {} is equal to or greater than end_time {}. Backtest can't run.", 
                             config.start_time, config.end_time));
	}

	// MARK: Execution Latency
    config.execution_latency_ms = GetOptional<uint64_t>(data, "execution_latency_ms",
		"Global Settings").value_or(default_config.execution_latency_ms);   

	// MARK: Initial Cash
	config.execution_latency_ms = GetOptional<uint64_t>(data, "initial_cash",
		"Global Settings").value_or(default_config.initial_cash); 
	if(config.initial_cash < 1){
		spdlog::warn("Initial Cash below 1, using default config initial cash");
		config.initial_cash = default_config.initial_cash;
	}

	// MARK: Log File Path
	config.log_file_path = GetOptional<uint64_t>(data, "log_file_path",
		"Global Settings").value_or(default_config.log_file_path); 
	
	// MARK: Report Output Directory
	config.report_output_dir = GetOptional<uint64_t>(data, "report_output_dir",
		"Global Settings").value_or(default_config.report_output_dir); 

	// MARK: Strategies
	if (!data.contains("strategies") || !data["strategies"].is_array() ||
		!data["strategies"].size() > 0) {
        throw std::runtime_error(
			"Config Error: 'strategies' must be a JSON array with at least one element.");
    }
	config.strategies = ParseStrategies(data["strategies"]);

	// MARK: Traded Instruments
	if (!data.contains("traded_instruments") || !data["traded_instruments"].is_array() ||
		!data["traded_instruments"].size() > 0) {
        throw std::runtime_error(
			"Config Error: 'traded_instruments' must be a JSON array with at least one element.");
    }
	config.traded_instruments = ParseTradedInstrs(data["traded_instruments"]);	

	// MARK: Risk Limits
	if (!data.contains("risk_limits")){
		spdlog::warn("No parsable risk limits detected, using default");
		config.risk_limits = default_config.risk_limits;
	} else {
		config.risk_limits = ParseRiskLimits(data["risk_limits"]);
	}

	// MARK: Data Streams
	if (!data.contains("data_streams") || !data["data_streams"].is_array() ||
		!data["data_streams"].size() > 0) {
        throw std::runtime_error(
			"Config Error: 'strategies' must be a JSON array with at least one element.");
    }
	std::string context = "data streams";
	for (const auto& stream : data["data_streams"]) {
		DataSourceConfig data_config;
		data_config.data_source_name = GetRequired<std::string>(stream, "data_source_name", context);
		data_config.data_symbology = ParseDataSymbols(GetRequired<std::string>(stream, "symbology_filepath", context));
		data_config.data_filepath =  GetRequired<std::string>(stream, "data_filepath", context);
		data_config.schema = StrToDataSchema(GetRequired<std::string>(stream, "schema", context));
		data_config.encoding = StrToEncoding(GetRequired<std::string>(stream, "encoding", context));
		data_config.compression = StrToCompression(GetRequired<std::string>(stream, "compression", context));
		data_config.price_format = StrToPriceFormat(GetRequired<std::string>(stream, "price_format", context));
		data_config.ts_format = StrToTSFormat(GetRequired<std::string>(stream, "ts_format", context)); 
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
	std::string context = "Strategy";
	for (const auto& item : data) {
		Strategy strat;	
		strat.name = GetRequired<std::string>(data, "name", context);
		strat.params = GetRequired<std::vector<int>>(data, "params", context);
		strat.max_lob_lvl = GetRequired<std::size_t>(data, "max_lob_lvl", context);
		res.push_back(strat);
	}	
	return res; 
}

std::vector<TradedInstrument> ParseTradedInstrs(const nlohmann::json& data) {
	std::vector<TradedInstrument> res;
	if (!data.is_array() || !data.size() > 0) throw std::runtime_error(
		"Config Error: 'traded_instruments' must be an array with at least one element.");

	for(const auto& item : data){
		TradedInstrument instr;
		std::string context = "Traded Instruments";
		instr.instrument_id = GetRequired<uint32_t>(data, "instrument_id", 
			"Traded Instruments");data["instrument_id"];
		instr.instrument_type = ParseInstrType(GetRequired<std::string>(data, 
			"instrument_type", "Traded Instruments"));
		instr.tick_size = numericUtils::doubleToFixedPoint(GetRequired<double>( // TODO check if number is reasonable
			data, "tick_size", "Traded Instruments"));
		instr.tick_value = numericUtils::doubleToFixedPoint(GetRequired<double>(
			data, "tick_value", "Traded Instruments"));
		instr.margin_req = numericUtils::doubleToFixedPoint(GetRequired<double>(
			data, "margin_req", "Traded Instruments"));

		res.push_back(instr);
	}
	return res;
}

RiskLimits ParseRiskLimits(const nlohmann::json& data) {
	RiskLimits res;
	std::string context = "Risk Limits";
	for(std::string field : kRiskLimitsFields){
		if(!data.contains(field)){
			spdlog::warn("field {} of riskLimits is not parsable, using default risk limits ", field);	
			return default_config.risk_limits;
		}
	}
	std::string context = "Risk Limits";

	res.risk_mode = ParseRiskMode(GetOptional<std::string>(data, 
		"risk_mode", context).value_or("PercentOfAcct"));
	res.max_position_size = numericUtils::doubleToFixedPoint(GetOptional<double>(data, 
		"max_position_size", context).value_or(3));
	res.max_risk_per_trade_pct = numericUtils::doubleToFixedPoint(GetOptional<double>(data, 
		"max_risk_per_trade_pct", context).value_or(.02));
	res.max_portfolio_delta = numericUtils::doubleToFixedPoint(GetOptional<double>(data, 
		"max_portfolio_delta", context).value_or(0));
	res.max_drawdown_pct = numericUtils::doubleToFixedPoint(GetOptional<double>(data, 
		"max_drawdown_pct", context).value_or(.2));
	res.max_delta_per_trade = numericUtils::doubleToFixedPoint(GetOptional<double>(data, 
		"max_delta_per_trade", context).value_or(0));

	return res;
}


template <typename T>
T GetRequired(const nlohmann::json& j, const std::string& key, const std::string& context) {
    if (!j.contains(key)) {
        throw std::runtime_error(fmt::format("Config Error: Missing '{}' in {}", key, context));
    }

    const auto& val = j.at(key);
	// Number check
	if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<T, bool>) {
		if (val.is_number()) {
			// Check for negative integers OR negative floats being assigned to an unsigned type
			if ((val.is_number_integer() && !val.is_number_unsigned()) || 
				(val.is_number_float() && val.get<double>() < 0)) {
				throw std::runtime_error(fmt::format(
					"Config Error: Negative value {} assigned to unsigned key '{}' in {}",
					val.dump(), key, context
				));
			}
		}
	}

    try {
        return val.get<T>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(fmt::format(
            "Config Error: Invalid type for '{}' in {}. Expected {}, but found {}.",
            key, context, typeid(T).name(), val.type_name()
        ));
    }
}

template <typename T>
std::optional<T> GetOptional(const nlohmann::json& j, const std::string& key, const std::string& context) {
    if (!j.contains(key) || j.at(key).is_null()) {
        return std::nullopt;
    }

    const auto& val = j.at(key);

    if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<T, bool>) {
        if (val.is_number()) {
            if ((val.is_number_integer() && !val.is_number_unsigned()) || 
                (val.is_number_float() && val.get<double>() < 0)) {
                spdlog::warn("Config: Key '{}' in {} is negative but expected unsigned. Using default.", key, context);
                return std::nullopt;
            }
        }
    }

    try {
        return val.get<T>();
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("Config: Key '{}' in {} has invalid type. Using default.", key, context);
        return std::nullopt;
    }
}

}