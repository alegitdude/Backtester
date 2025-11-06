#pragma once
#include <string>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace backtester {

namespace time {

std::string EpochToString(uint64_t epoch_nanos, 
		                               const std::string& time_zone);

long long StringToEpoch(const std::string& time_str, 
		                            const std::string& timezone);

int GetTimezoneOffset(const std::string& timezone);

bool ParseTimeString(
		const std::string& time_str, int& year, int& month, int& day,
    int& hour, int& minute, int& second, uint32_t& nanoseconds);

}
}