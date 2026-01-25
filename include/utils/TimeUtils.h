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

int GetTimezoneOffset(const std::string& timezone);

uint64_t ParseIsoToUnix(const std::string& str);

inline int fast_atoi_2(const char* s) {
return (s[0] - '0') * 10 + (s[1] - '0');
}

inline int fast_atoi_4(const char* s) {
    return (s[0] - '0') * 1000 +
           (s[1] - '0') * 100  +
           (s[2] - '0') * 10   +
           (s[3] - '0');
}

}
}