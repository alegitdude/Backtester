#pragma once
#include "Types.h"
#include "../utils/StringUtils.h"
#include <nlohmann/json.hpp>
#include <filesystem>

namespace backtester {

const std::vector<std::string> kRequiredConfigFields = {
    "start_time",
    "end_time", 
    "traded_instrument",
    "data_streams",
    "strategies"
};

const std::vector<std::string> kOptionalConfigFields = {
    "execution_latency",
    "initial_cash",
    "log_file_path",
    "report_output_dir"
};

const std::vector<std::string> kRequiredTradedInstrFields = {
	"instrument_id",
  "instrument_type",
  "tick_size",
  "tick_value",
  "margin_requirement"
};

const std::vector<std::string> kRiskLimitsFields = {
  "risk_mode",
  "max_position_size",
  "max_risk_per_trade_pct", 
  "max_portfolio_delta",
  "max_drawdown_pct",
  "max_delta_per_trade" 
};

const std::vector<std::string> kRequiredDataStreamFields = {
    "symbology_filepath",
    "data_filepath",
    "schema",
    "encoding",
    "compression",
    "price_format",
    "timestamp_format"
};

const std::vector<std::string> kOptionalDataStreamFields = {
    "data_source_name"
};

AppConfig ParseConfigToObj(std::filesystem::path& config_path); 

AppConfig ParseConfigFromJson(const nlohmann::json& data);

std::vector<Symbol> ParseDataSymbols(const std::string& filepath);
std::vector<Strategy> ParseStrategies(const nlohmann::json& data);
std::vector<TradedInstrument> ParseTradedInstrs(const nlohmann::json& data);
RiskLimits ParseRiskLimits(const nlohmann::json& data);

inline DataSchema StrToDataSchema(const std::string& str) {
  if (str == "MBO" || str == "mbo") return DataSchema::MBO;
  if (str == "OHLCV" || str == "ohlcv") return DataSchema::OHLCV;
  throw std::invalid_argument("Invalid schema: " + str);
};

inline Encoding StrToEncoding(const std::string& str) {
  if (str == "CSV" || str == "csv") return Encoding::CSV;
  if (str == "DBN" || str == "dbn") return Encoding::DBN;
  if (str == "JSON" || str == "json") return Encoding::JSON;
  throw std::invalid_argument("Invalid encoding value: " + str);
};

inline Compression StrToCompression(const std::string& str) {
	if (str == "ZSTD" || str == "zstd") return Compression::ZSTD;
  if (str == "NONE" || str == "none") return Compression::NONE;
  throw std::invalid_argument("Invalid compression value: " + str);
};

inline PriceFormat StrToPriceFormat(const std::string& str) {
	if (str == "FIXPNTINT" || str == "fixpntint") return PriceFormat::FIXPNTINT;
  if (str == "DECIMAL" || str == "decimal") return PriceFormat::DECIMAL;
  throw std::invalid_argument("Invalid PriceFormat: " + str);
};

inline TmStampFormat StrToTSFormat(const std::string& str) {
	if (str == "UNIX" || str == "unix") return TmStampFormat::UNIX;
	if (str == "ISO" || str == "iso") return TmStampFormat::ISO;
	throw std::invalid_argument("Invalid schema: " + str);
};

inline InstrumentType ParseInstrType(const std::string& str) {
  if (str == "FUT" || str == "fut") return InstrumentType::FUT;
  if (str == "STOCK" || str == "STOCK") return InstrumentType::STOCK;
  if (str == "OPTION" || str == "OPTION") return InstrumentType::OPTION;
	throw std::invalid_argument("Invalid instrument type: " + str);
}

inline RiskMode ParseRiskMode(const std::string& str){
  if (stringUtils::ToLower(str) == "percentofacct") return RiskMode::PercentOfAcct;
  if (stringUtils::ToLower(str) == "posSizeindollars") return RiskMode::PosSizeInDollars;
  return RiskMode::PercentOfAcct;
}

};