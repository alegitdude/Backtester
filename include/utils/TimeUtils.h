#pragma once
#include "../core/Types.h"
#include <string>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <optional>

namespace backtester {

namespace time {

constexpr uint64_t k1MinuteNs = 60'000'000'000LL;
constexpr uint64_t k2MinuteNs = 120'000'000'000LL;
constexpr uint64_t k5MinuteNs = 300'000'000'000LL;

struct TimeOfDay {
    uint8_t hour;
    uint8_t minute;
};

inline TimeOfDay GetTimeOfDay(uint64_t epoch_nanos) {
    uint64_t seconds = epoch_nanos / 1'000'000'000ULL;
    uint64_t day_seconds = seconds % 86400;
    return {
        static_cast<uint8_t>(day_seconds / 3600),
        static_cast<uint8_t>((day_seconds % 3600) / 60)
    };
}

std::string EpochToString(uint64_t epoch_nanos, 
		                               const std::string& time_zone = "UTC");

int GetTimezoneOffset(const std::string& timezone);

TimeParseResult ParseIsoToUnix(std::string_view s);

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