#include "../../include/utils/TimeUtils.h"
#include <iostream>
#include <stdexcept>
#include <charconv>

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

uint64_t ParseIsoToUnix(const std::string& s) {
    struct tm t = {0};
    const char* data = s.data();

    // Use from_chars for slightly better safety at near-identical speed
    std::from_chars(data,      data + 4, t.tm_year); t.tm_year -= 1900;
    std::from_chars(data + 5,  data + 7, t.tm_mon);  t.tm_mon -= 1;
    std::from_chars(data + 8,  data + 10, t.tm_mday);
    std::from_chars(data + 11, data + 13, t.tm_hour);
    std::from_chars(data + 14, data + 16, t.tm_min);
    std::from_chars(data + 17, data + 19, t.tm_sec);

    time_t epoch_seconds = timegm(&t); // Use _mkgmtime on Windows

    uint32_t nanos = 0;
    // Check if subseconds exist before parsing
    if (s.length() > 20 && s[19] == '.') {
        std::from_chars(data + 20, data + 29, nanos);
    }

    return static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL + nanos;
}

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