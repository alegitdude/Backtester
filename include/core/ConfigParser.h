#pragma once
#include "Types.h"
#include "../utils/StringUtils.h"
#include "spdlog/spdlog.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <optional>

namespace backtester {

AppConfig ParseConfigToObj(const std::filesystem::path& config_path); 

AppConfig ParseConfigFromJson(const nlohmann::json& data, std::filesystem::path config_path);

std::vector<Symbol> ParseDataSymbols(const std::string& filepath);
std::vector<Strategy> ParseStrategies(const nlohmann::json& data);
std::vector<TradedInstrument> ParseTradedInstrs(const nlohmann::json& data);
RiskLimits ParseRiskLimits(const nlohmann::json& data);
CommissionStruct ParseCommissions(const nlohmann::json& data);

inline DataSchema StrToDataSchema(const std::string& str) {
  if (str == "MBO" || str == "mbo") return DataSchema::MBO;
  if (str == "OHLCV" || str == "ohlcv") return DataSchema::OHLCV;
  spdlog::error("Invalid/unparsable data schema in data stream config: {}", str);
  throw std::invalid_argument("Invalid schema: " + str);
};

inline Encoding StrToEncoding(const std::string& str) {
  if (str == "CSV" || str == "csv") return Encoding::CSV;
  if (str == "DBN" || str == "dbn") return Encoding::DBN;
  if (str == "JSON" || str == "json") return Encoding::JSON;
  spdlog::error("Invalid/unparsable data encoding in data stream config: {}", str);
  throw std::invalid_argument("Invalid encoding value: " + str);
};

inline Compression StrToCompression(const std::string& str) {
	if (str == "ZSTD" || str == "zstd") return Compression::ZSTD;
  if (str == "NONE" || str == "none") return Compression::NONE;
  spdlog::error("Invalid/unparsable compression type in data stream config: {}", str);
  throw std::invalid_argument("Invalid compression value: " + str);
};

inline PriceFormat StrToPriceFormat(const std::string& str) {
	if (str == "FIXPNTINT" || str == "fixpntint") return PriceFormat::FIXPNTINT;
  if (str == "DECIMAL" || str == "decimal") return PriceFormat::DECIMAL;
  spdlog::error("Invalid/unparsable price format in data stream config: {}", str);
  throw std::invalid_argument("Invalid PriceFormat: " + str);
};

inline TmStampFormat StrToTSFormat(const std::string& str) {
	if (str == "UNIX" || str == "unix") return TmStampFormat::UNIX;
	if (str == "ISO" || str == "iso") return TmStampFormat::ISO;
  spdlog::error("Invalid/unparsable timestamp format in data stream config: {}", str);
	throw std::invalid_argument("Invalid schema: " + str);
};

inline InstrumentType ParseInstrType(const std::string& str) {
  if (str == "FUT" || str == "fut") return InstrumentType::FUT;
  if (str == "STOCK" || str == "STOCK") return InstrumentType::STOCK;
  if (str == "OPTION" || str == "OPTION") return InstrumentType::OPTION;
  spdlog::error("Invalid/unparsable instrument type in data stream config: {}", str);
	throw std::invalid_argument("Invalid instrument type: " + str);
}

inline RiskMode ParseRiskMode(const std::string& str){
  if (stringUtils::ToLower(str) == "percentofacct") return RiskMode::PercentOfAcct;
  if (stringUtils::ToLower(str) == "posSizeindollars") return RiskMode::PosSizeInDollars;
  spdlog::warn("Invalid/unparsable risk mode in data stream config: {} - using PercentOfAcct", str);
  return RiskMode::PercentOfAcct;
}

template <typename T>
T GetRequired(const nlohmann::json& j, const std::string& key, const std::string& context);

template <typename T>
std::optional<T> GetOptional(const nlohmann::json& j, const std::string& key, const std::string& context);

};