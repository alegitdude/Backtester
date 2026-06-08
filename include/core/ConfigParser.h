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
  std::string tmp_str = stringUtils::ToLower(str);
  if (tmp_str == "mbo") return DataSchema::MBO;
  if (tmp_str == "ohlcv") return DataSchema::OHLCV;
  spdlog::error("Invalid/unparsable data schema in data stream config: {}", str);
  throw std::invalid_argument("Invalid schema: " + str);
};

inline Encoding StrToEncoding(const std::string& str) {
  std::string tmp_str = stringUtils::ToLower(str);
  if (tmp_str == "csv") return Encoding::CSV;
  if (tmp_str == "dbn") return Encoding::DBN;
  if (tmp_str == "json") return Encoding::JSON;
  spdlog::error("Invalid/unparsable data encoding in data stream config: {}", str);
  throw std::invalid_argument("Invalid encoding value: " + str);
};

inline Compression StrToCompression(const std::string& str) {
  std::string tmp_str = stringUtils::ToLower(str);
	if (tmp_str == "zstd") return Compression::ZSTD;
  if (tmp_str == "none") return Compression::NONE;
  spdlog::error("Invalid/unparsable compression type in data stream config: {}", str);
  throw std::invalid_argument("Invalid compression value: " + str);
};

inline PriceFormat StrToPriceFormat(const std::string& str) {
  std::string tmp_str = stringUtils::ToLower(str);
	if (tmp_str == "fixpntint") return PriceFormat::FIXPNTINT;
  if (tmp_str == "decimal") return PriceFormat::DECIMAL;
  spdlog::error("Invalid/unparsable price format in data stream config: {}", str);
  throw std::invalid_argument("Invalid PriceFormat: " + str);
};

inline TmStampFormat StrToTSFormat(const std::string& str) {
  std::string tmp_str = stringUtils::ToLower(str);
	if (tmp_str == "unix") return TmStampFormat::UNIX;
	if (tmp_str == "iso") return TmStampFormat::ISO;
  spdlog::error("Invalid/unparsable timestamp format in data stream config: {}", str);
	throw std::invalid_argument("Invalid schema: " + str);
};

inline InstrumentType ParseInstrType(const std::string& str) {
  std::string tmp_str = stringUtils::ToLower(str);
  if (tmp_str == "fut") return InstrumentType::FUT;
  if (tmp_str == "stock") return InstrumentType::STOCK;
  if (tmp_str == "option") return InstrumentType::OPTION;
  spdlog::error("Invalid/unparsable instrument type in data stream config: {}", str);
	throw std::invalid_argument("Invalid instrument type: " + str);
}

inline RiskMode ParseRiskMode(const std::string& str){
  std::string tmp_str = stringUtils::ToLower(str);
  if (tmp_str == "percentofacct") return RiskMode::PercentOfAcct;
  if (tmp_str == "possizeindollars") return RiskMode::PosSizeInDollars;
  spdlog::warn("Invalid/unparsable risk mode in data stream config: {} - using PercentOfAcct", str);
  return RiskMode::PercentOfAcct;
}

template <typename T>
T GetRequired(const nlohmann::json& j, const std::string& key, const std::string& context);

template <typename T>
std::optional<T> GetOptional(const nlohmann::json& j, const std::string& key, const std::string& context);

};