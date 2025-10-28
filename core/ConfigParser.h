#pragma once
#include "Types.h"
#include <filesystem>

namespace ConfigParser {
  const Config ParseConfigToObj(std::string& config_path);  
};