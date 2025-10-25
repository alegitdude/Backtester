#include "Types.h"
#include <filesystem>

class ConfigParser {
 public: 
  static Config ParseConfigToObj(std::string config_path); 
};