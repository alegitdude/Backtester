#pragma once
#include "Types.h"
#include <filesystem>

namespace backtester {
  AppConfig ParseConfigToObj(std::string& config_path); 

  DataFormat StringToDataFormat(const std::string& str); 
};