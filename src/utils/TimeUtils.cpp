#include "../../include/utils/TimeUtils.h"
#include <iostream>
#include <stdexcept>
#include <charconv>
#include <optional>

namespace backtester {

namespace time {

// MARK: EpochToString

std::string EpochToString(uint64_t epoch_nanos, const std::string& timezone = "UTC") {
	uint64_t epoch_seconds = epoch_nanos / 1000000000ULL;
	uint32_t nanos = epoch_nanos % 1000000000ULL;

	int tz_offset = GetTimezoneOffset(timezone);
	epoch_seconds += tz_offset;
	
	time_t time = static_cast<time_t>(epoch_seconds);  // time_t = long int

	// Convert to tm struct (always use UTC since we already applied offset)
	struct tm* tm_info = gmtime(&time);
	if (!tm_info) {
			return "Invalid time";
	}
	
	// Format the time string
	std::ostringstream oss;
	oss << std::setfill('0')
			<< std::setw(4) << (tm_info->tm_year + 1900) << "-"
			<< std::setw(2) << (tm_info->tm_mon + 1) << "-"
			<< std::setw(2) << tm_info->tm_mday << " "
			<< std::setw(2) << tm_info->tm_hour << ":"
			<< std::setw(2) << tm_info->tm_min << ":"
			<< std::setw(2) << tm_info->tm_sec << "."
			<< std::setw(9) << nanos;
	
	return oss.str();
};

// MARK: Parse Iso To Unix

// uint64_t ParseIsoToUnix(std::string str) {
//     const char* s = str.c_str();
//   //2022-01-03T00:00:00.000000000Z
//   //012345678901234567890123456789
//     struct tm t = {0};
//     t.tm_year = fast_atoi_4(s) - 1900; // tm_year is years since 1900
//     t.tm_mon  = fast_atoi_2(s + 5) - 1; // tm_mon is 0-11
//     t.tm_mday = fast_atoi_2(s + 8);
//     t.tm_hour = fast_atoi_2(s + 11);
//     t.tm_min  = fast_atoi_2(s + 14);
//     t.tm_sec  = fast_atoi_2(s + 17);
//     t.tm_isdst = 0; // Not in daylight saving time 

//     time_t epoch_seconds = timegm(&t);

//     const char* p_nanos = s + 20; 
//     uint32_t nanos = 0;

//     for (int i = 0; i < 9; ++i) {
//         nanos = nanos * 10 + (p_nanos[i] - '0');
//     }
//     return static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL + nanos;
// }

TimeParseResult ParseIsoToUnix(std::string_view s) {
    struct tm t = {0};
    const char* data = s.data();
    
    // Helper to check from_chars results
    auto check = [&](const std::from_chars_result& res, std::string_view field) -> bool {
        if (res.ec != std::errc{}) return false;
        return true;
    };

    // Parsing YYYY-MM-DD HH:MM:SS
    if (!check(std::from_chars(data,      data + 4,  t.tm_year), "year"))  return {0, "Invalid Year", false};
    if (!check(std::from_chars(data + 5,  data + 7,  t.tm_mon),  "month")) return {0, "Invalid Month", false};
    if (!check(std::from_chars(data + 8,  data + 10, t.tm_mday), "day"))   return {0, "Invalid Day", false};
    if (!check(std::from_chars(data + 11, data + 13, t.tm_hour), "hour"))  return {0, "Invalid Hour", false};
    if (!check(std::from_chars(data + 14, data + 16, t.tm_min),  "min"))   return {0, "Invalid Minute", false};
    if (!check(std::from_chars(data + 17, data + 19, t.tm_sec),  "sec"))   return {0, "Invalid Second", false};

    t.tm_year -= 1900;
    t.tm_mon -= 1;
    
    time_t epoch_seconds = timegm(&t); // Ensure UTC interpretation

    uint64_t nanos = 0;
    if (s.length() > 19 && s[19] == '.') {
        const char* nano_start = data + 20;
        const char* nano_end = data + s.length();
        uint32_t raw_val = 0;
        auto res = std::from_chars(nano_start, nano_end, raw_val);
        
        if (res.ec == std::errc{}) {
            // SCALE THE NANOS: .1 should be 100,000,000
            size_t digits = res.ptr - nano_start;
            nanos = raw_val;
            for (size_t i = digits; i < 9; ++i) nanos *= 10;
        }
    }

    return {static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL + nanos, "", true};
}

// std::optional<uint64_t> ParseIsoToUnix(const std::string& s) {
//     struct tm t = {0};
//     const char* data = s.data();
//     auto [ptr, ec] = std::from_chars(data, data + 4, t.tm_year);
//         if (ec != std::errc{}) {
//         return std::nullopt;  
//     }
//     t.tm_year -= 1900;
//     std::from_chars(data + 5,  data + 7, t.tm_mon);  t.tm_mon -= 1;
//     std::from_chars(data + 8,  data + 10, t.tm_mday);
//     std::from_chars(data + 11, data + 13, t.tm_hour);
//     std::from_chars(data + 14, data + 16, t.tm_min);
//     std::from_chars(data + 17, data + 19, t.tm_sec);

//     time_t epoch_seconds = timegm(&t); // Use _mkgmtime on Windows

//     uint32_t nanos = 0;
//     // Check if subseconds exist before parsing
//     if (s.length() > 20 && s[19] == '.') {
//         std::from_chars(data + 20, data + 29, nanos);
//     }

//     return static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL + nanos;
// }

// MARK: Get Timezone offset

int GetTimezoneOffset(const std::string& timezone) {
    // Common timezone offsets (simplified - in production use a library like date.h)
    if (timezone == "UTC" || timezone == "GMT") {
        return 0;
    } else if (timezone == "EST" || timezone == "America/New_York") {
        return -5 * 3600; // EST is UTC-5 (ignoring DST for simplicity)
    } else if (timezone == "PST" || timezone == "America/Los_Angeles") {
        return -8 * 3600; // PST is UTC-8
    } else if (timezone == "CST" || timezone == "America/Chicago") {
        return -6 * 3600; // CST is UTC-6
    } else if (timezone == "MST" || timezone == "America/Denver") {
        return -7 * 3600; // MST is UTC-7
    } else if (timezone == "CET" || timezone == "Europe/Paris") {
        return 1 * 3600; // CET is UTC+1
    } else if (timezone == "JST" || timezone == "Asia/Tokyo") {
        return 9 * 3600; // JST is UTC+9
    } else if (timezone == "HKT" || timezone == "Asia/Hong_Kong") {
        return 8 * 3600; // HKT is UTC+8
    } else {
        std::cerr << "Warning: Unknown timezone '" << timezone << "', using UTC" << std::endl;
        return 0;
    }
}
}
}