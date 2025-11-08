#pragma once
#include "Types.h"
#include <filesystem>

namespace backtester {
  AppConfig ParseConfigToObj(std::string& config_path); 

  inline DataSchema StrToDataSchema(const std::string& str) {
	if (str == "MBO") return DataSchema::MBO;
    if (str == "OHLCV") return DataSchema::OHLCV;
    throw std::invalid_argument("Invalid schema: " + str);
};

inline Encoding StrToEncoding(const std::string& str) {
	if (str == "CSV") return Encoding::CSV;
    if (str == "DBN") return Encoding::DBN;
	if (str == "JSON") return Encoding::JSON;
    throw std::invalid_argument("Invalid encoding value: " + str);
};

inline Compression StrToCompression(const std::string& str) {
	if (str == "ZSTD") return Compression::ZSTD;
    if (str == "NONE") return Compression::NONE;
    throw std::invalid_argument("Invalid compression value: " + str);
};

inline PriceFormat StrToPriceFormat(const std::string& str) {
	if (str == "FIXPNTINT") return PriceFormat::FIXPNTINT;
    if (str == "DECIMAL") return PriceFormat::DECIMAL;
    throw std::invalid_argument("Invalid PriceFormat: " + str);
};

inline TmStampFormat StrToTSFormat(const std::string& str) {
	if (str == "UNIX") return TmStampFormat::UNIX;
	if (str == "ISO") return TmStampFormat::ISO;
	throw std::invalid_argument("Invalid schema: " + str);
};

};