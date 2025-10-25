#pragma once
#include <string>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

class TimeUtils {
 public:
  static std::string EpochToString(uint64_t epoch_nanos, 
		                               const std::string& time_zone);

	static uint64_t StringToEpoch(const std::string& time_str, 
		                            const std::string& timezone);

 private:
  static int GetTimezoneOffset(const std::string& timezone);

	static bool TimeUtils::ParseTimeString(
		const std::string& time_str, int& year, int& month, int& day,
    int& hour, int& minute, int& second, uint32_t& nanoseconds);
};