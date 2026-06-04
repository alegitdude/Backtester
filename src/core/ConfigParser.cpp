#include "../../include/core/ConfigParser.h"
#include "../../include/utils/TimeUtils.h"
#include "../../include/utils/NumericUtils.h"
#include "../../include/core/Types.h"
#include <fstream>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

namespace backtester {

	namespace {
		const std::filesystem::path kRootFolder = PROJECT_ROOT_DIR;
    std::filesystem::path kDefaultLogDir = kRootFolder / "logs";
		std::filesystem::path kDefaultReportDir = kRootFolder / "reports";
		
		constexpr uint64_t kDefaultInitialCash = 100'000;
		constexpr uint64_t kDefaultExecLatencyMs = 200;
		constexpr uint64_t kDefaultSnapshotIntervalMs = 1'000;
		constexpr uint64_t kOneDayInMs = 86'400'000;
		constexpr uint64_t kFxdPntMultiplier = 1'000'000'000;
		constexpr uint64_t kDefaultMaxPositionSize = 3;
		constexpr double kDefaultMaxRiskPerTradePct = .02;
		constexpr double kDefaultMaxPortfolioDelta = 0;
		constexpr double kDefaultMaxDrawdownPct= .2;
		constexpr double kDefaultMaxDeltaPerTrade = 0;
		constexpr double kDefaultFutPerContract = 2.17;
		constexpr double kDefaultStockClearingFee = 0.35;
		constexpr double kDefaultStockOrderMin = 0.035;
		constexpr double kDefaultStockPerShare = 0.0002;
		constexpr double kDefaultRiskFreeRate = 0.05;

		constexpr RiskLimits kDefaultRiskLimits = {RiskMode::PercentOfAcct, 
																							 kDefaultMaxPositionSize,
																							 20'000'000, 
																							 0, 
																							 200'000'000, 
																							 0};
	}

	std::vector<Symbol> ParseDataSymbols(const std::string& filepath) {
		std::vector<Symbol> instruments;
		std::ifstream file(filepath, std::ios::in);

		if (!file.is_open()) {
			spdlog::error("Can not open data symbols file at: {}", filepath);
			std::cerr << "Could not data symbols file!" << std::endl;
			return instruments;
		}
		std::string line;
		uint32_t count = 0;

		while (std::getline(file, line)) {
			std::stringstream ss(line);
			std::string symbol, instrument, date;
			Symbol row;
			count++;
			if (count == 1) { continue; } // Skip header line
			if (std::getline(ss, symbol, ',') &&
				std::getline(ss, instrument, ',') &&
				std::getline(ss, date)) {

				try {
					row.symbol = symbol;
					row.instrument_id = static_cast<uint32_t>(std::stoul(instrument));
					row.date = date;

					instruments.push_back(row);
				}
				catch (const std::exception& e) {
					// Handles cases where column 2 is not a valid number
					std::cerr << "Skipping row due to conversion error: " << e.what() << std::endl;
				}
			}
		}

		file.close();
		return instruments;
	}

	AppConfig ParseConfigFromJson(const nlohmann::json& data, std::filesystem::path config_path) {
		AppConfig config;
		config_path = std::filesystem::absolute(config_path).lexically_normal();
		std::filesystem::path config_dir = config_path.parent_path();

    auto resolve = [&](std::string& p) {
        std::filesystem::path fp(p);
        if (fp.is_relative()) {
            p = (config_dir / fp).lexically_normal().string();
        }
    };

		// MARK: Start Time
		std::string start_str = GetRequired<std::string>(data, "start_time", "Global Settings");
		auto start_result = time::ParseIsoToUnix(start_str); //TODO double check est
		if (!start_result.success) {
			throw std::runtime_error(fmt::format("Config 'start_time' error: {} in string ",
				start_result.error_msg));
		}
		config.start_time = start_result.unix_nanos;

		// MARK: End Time
		std::string end_str = GetRequired<std::string>(data, "end_time", "Global Settings");
		auto end_result = time::ParseIsoToUnix(end_str);
		if (!end_result.success) {
			throw std::runtime_error(fmt::format("Config 'end_time' error: {} in string ",
				end_result.error_msg));
		}
		config.end_time = end_result.unix_nanos;

		if (config.start_time >= config.end_time) {
			throw std::runtime_error(fmt::format("Config start_time {} is equal to or greater than end_time {}. Backtest can't run.",
				config.start_time, config.end_time));
		}

		// MARK: Execution Latency
		config.execution_latency_ms = GetOptional<uint64_t>(data, "execution_latency_ms",
			"Global Settings").value_or(kDefaultExecLatencyMs);

		// MARK: Snapshot Interval
		auto interval_ms = GetOptional<uint64_t>(data, "snapshot_interval_ms",
			"Global Settings").value_or(kDefaultSnapshotIntervalMs);
		if(interval_ms > kOneDayInMs ){
			spdlog::warn("Snapshot interval exceeds one day, using one day as default");
			interval_ms = kOneDayInMs;
		}
		config.snapshot_interval_ns = interval_ms * 1'000'000;

		// MARK: Initial Cash
		config.initial_cash = GetOptional<uint64_t>(data, "initial_cash",
			"Global Settings").value_or(kDefaultInitialCash);
		if (config.initial_cash < 1) {
			spdlog::warn("Initial Cash below 1, using default config initial cash");
			config.initial_cash = kDefaultInitialCash;
		}
		config.initial_cash *= kFxdPntMultiplier;

		// MARK: Log File Path
		config.log_file_path = GetOptional<std::string>(data, "log_file_path",
			"Global Settings").value_or("");
		if(config.log_file_path == "") config.log_file_path = kRootFolder / "logs";
		resolve(config.log_file_path);

		// MARK: Report Output Directory
		config.report_output_dir = GetOptional<std::string>(data, "report_output_dir",
			"Global Settings").value_or("");
		if(config.report_output_dir == "") config.report_output_dir = kDefaultReportDir;
		resolve(config.report_output_dir);

		// MARK: Risk-Free Rate
		config.risk_free_rate = GetOptional<double>(data, "risk_free_rate",
			"Global Settings").value_or(kDefaultRiskFreeRate);

		// MARK: Strategies
		if (!data.contains("strategies") || !data["strategies"].is_array() ||
			!(data["strategies"].size() > 0)) {
			throw std::runtime_error(
				"Config Error: 'strategies' must be a JSON array with at least one element.");
		}
		config.strategies = ParseStrategies(data["strategies"]);

		// MARK: Traded Instruments
		if (!data.contains("traded_instruments") || !data["traded_instruments"].is_array() ||
			!(data["traded_instruments"].size() > 0)) {
			throw std::runtime_error(
				"Config Error: 'traded_instruments' must be a JSON array with at least one element.");
		}
		config.traded_instruments = ParseTradedInstrs(data["traded_instruments"]);

		// MARK: Risk Limits
		if (!data.contains("risk_limits")) {
			spdlog::warn("No parsable risk limits detected, using default");
			config.risk_limits = kDefaultRiskLimits;
		}
		else {
			config.risk_limits = ParseRiskLimits(data["risk_limits"]);
		}

		// MARK: Data Streams
		if (!data.contains("data_streams") || !data["data_streams"].is_array() ||
			!(data["data_streams"].size() > 0)) {
			throw std::runtime_error(
				"Config Error: 'data_streams' must be a JSON array with at least one element.");
		}
		std::string context = "data streams";
		for (const auto& stream : data["data_streams"]) {
			DataSourceConfig data_config;
			data_config.data_source_name = GetRequired<std::string>(stream, "data_source_name", context);
			auto tmp_sym_path = GetRequired<std::string>(stream, "symbology_filepath", context);
			resolve(tmp_sym_path);
			data_config.data_symbology = ParseDataSymbols(tmp_sym_path);

			data_config.data_filepath = GetRequired<std::string>(stream, "data_filepath", context);
			resolve(data_config.data_filepath);

			data_config.schema = StrToDataSchema(GetRequired<std::string>(stream, "schema", context));
			data_config.encoding = StrToEncoding(GetRequired<std::string>(stream, "encoding", context));
			data_config.compression = StrToCompression(GetRequired<std::string>(stream, "compression", context));
			data_config.price_format = StrToPriceFormat(GetRequired<std::string>(stream, "price_format", context));
			data_config.ts_format = StrToTSFormat(GetRequired<std::string>(stream, "timestamp_format", context));
			config.data_configs.push_back(data_config);
		};

		for (const auto& data_stream : config.data_configs) {
			for (const auto& symbology : data_stream.data_symbology) {
				config.active_instruments.push_back(symbology.instrument_id);
			}
		}

		// MARK:Commissions 
		if (!data.contains("commissions")) {
			spdlog::warn("No parsable commissions settings detected, using default");
			config.commission_struct = ParseCommissions({});
		}
		else {
			config.commission_struct = ParseCommissions(data["commissions"]);
		}

		return config;
	}

	AppConfig ParseConfigToObj(const std::filesystem::path& config_path) {
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
		try {
			data = json::parse(f);
		}
		catch (json::exception e) {
			throw std::runtime_error("Config file is not valid JSON at: " + config_path.generic_string());
		}

		return ParseConfigFromJson(data, config_path);
	}

	std::vector<Strategy> ParseStrategies(const nlohmann::json& strategies) {
		std::vector<Strategy> res;
		std::string context = "Strategy";
		for (const auto& strategy : strategies) {
			Strategy strat;
			strat.name = GetRequired<std::string>(strategy, "name", context);
			strat.params = GetRequired<std::vector<int>>(strategy, "params", context);
			strat.traded_instr_id = GetRequired<std::size_t>(strategy, "traded_instr_id", context);
			strat.max_lob_lvl = GetRequired<std::size_t>(strategy, "max_lob_lvl", context);
			res.push_back(strat);
		}
		return res;
	}

	std::vector<TradedInstrument> ParseTradedInstrs(const nlohmann::json& data) {
		std::vector<TradedInstrument> res;
		if (!data.is_array() || !(data.size() > 0)) throw std::runtime_error(
			"Config Error: 'traded_instruments' must be an array with at least one element.");

		for (const auto& item : data) {
			TradedInstrument instr;
			std::string context = "Traded Instruments";
			instr.instrument_id = GetRequired<uint32_t>(item, "instrument_id",
				"Traded Instruments");
			instr.instrument_type = ParseInstrType(GetRequired<std::string>(item,
				"instrument_type", "Traded Instruments"));
			instr.tick_size = numericUtils::doubleToFixedPoint(GetRequired<double>( // TODO check if number is reasonable
				item, "tick_size", "Traded Instruments"));
			if (instr.tick_size == 0) {
				throw std::runtime_error(fmt::format("Tick size cannot be 0, error for instrument {}", instr.instrument_id));
			}
			instr.tick_value = numericUtils::doubleToFixedPoint(GetRequired<double>(
				item, "tick_value", "Traded Instruments"));
			if (instr.tick_value == 0) {
				throw std::runtime_error(fmt::format("Tick value cannot be 0, error for instrument {}", instr.instrument_id));
			}
			instr.init_margin_req = numericUtils::doubleToFixedPoint(GetRequired<double>(
				item, "init_margin_req", "Traded Instruments"));
			instr.main_margin_req = numericUtils::doubleToFixedPoint(GetRequired<double>(
				item, "main_margin_req", "Traded Instruments"));

			res.push_back(instr);
		}
		return res;
	}

	RiskLimits ParseRiskLimits(const nlohmann::json& data) {
		RiskLimits res;

		std::string context = "Risk Limits";
		res.risk_mode = ParseRiskMode(GetOptional<std::string>(data,
			"risk_mode", context).value_or(""));
		res.max_position_size = GetOptional<int64_t>(data,
			"max_position_size", context).value_or(kDefaultMaxPositionSize);
		res.max_risk_per_trade_pct = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"max_risk_per_trade_pct", context).value_or(kDefaultMaxRiskPerTradePct));
		res.max_portfolio_delta = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"max_portfolio_delta", context).value_or(kDefaultMaxPortfolioDelta));
		res.max_drawdown_pct = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"max_drawdown_pct", context).value_or(kDefaultMaxDrawdownPct));
		res.max_delta_per_trade = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"max_delta_per_trade", context).value_or(kDefaultMaxDeltaPerTrade));

		return res;
	}

  CommissionStruct ParseCommissions(const nlohmann::json& data) {
		CommissionStruct res;

		std::string context = "Commissions";
		res.fut_per_contract = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"fut_per_contract", context).value_or(kDefaultFutPerContract));
		res.stock_clearing_fee = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"stock_clearing_fee", context).value_or(kDefaultStockClearingFee));
		res.stock_order_min = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"stock_order_min", context).value_or(kDefaultStockOrderMin));
		res.stock_per_share = numericUtils::doubleToFixedPoint(GetOptional<double>(data,
			"stock_per_share", context).value_or(kDefaultStockPerShare));

		return res;
	}

	template <typename T>
	T GetRequired(const nlohmann::json& j, const std::string& key, const std::string& context) {
		if (!j.contains(key)) {
			throw std::runtime_error(fmt::format("Config Error: Missing '{}' in {}", key, context));
		}

		const auto& val = j.at(key);
		if (val.is_number() && val.get<double>() < 0) {
			throw std::runtime_error(fmt::format(
				"Config Error: Negative value {} assigned to unsigned key '{}' in {}",
				val.dump(), key, context));
		}

		try {
			return val.get<T>();
		}
		catch (const nlohmann::json::exception& e) {
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
		if (val.is_number() && val.get<double>() < 0) {
			spdlog::warn("Config: Key '{}' in {} is negative. Using default.", key, context);
			return std::nullopt;
		}

		try {
			return val.get<T>();
		}
		catch (const nlohmann::json::exception& e) {
			spdlog::warn("Config: Key '{}' in {} has invalid type. Using default.", key, context);
			return std::nullopt;
		}
	}

}