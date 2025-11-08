#pragma once
#include "Types.h"
#include <filesystem>

namespace backtester {
  AppConfig ParseConfigToObj(std::string& config_path); 

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

};