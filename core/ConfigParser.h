#pragma once
#include "Types.h"
#include <filesystem>

class ConfigParser {
 public: 
  static const Config ParseConfigToObj(std::string config_path);
};