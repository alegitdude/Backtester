#include "../../include/utils/TimeUtils.h"
#include <iostream>
#include <stdexcept>
#include <charconv>
#include <optional>

namespace backtester {

    namespace time {

        // MARK: EpochToString

        std::string EpochToString(uint64_t epoch_nanos, const std::string& timezone) {
            uint64_t epoch_seconds = epoch_nanos / 1000000000ULL;
            uint32_t nanos = epoch_nanos % 1000000000ULL;

            int tz_offset = GetTimezoneOffset(timezone);
            epoch_seconds += tz_offset;

            time_t time = static_cast<time_t>(epoch_seconds);  // time_t = long int

            // Convert to tm struct (always use UTC since we already applied offset)
            struct tm tm_info;
            if (!gmtime_r(&time, &tm_info)) {
                return "Invalid time";
            }
            // struct tm* tm_info = gmtime(&time);
            // if (!tm_info) {
            //     return "Invalid time";
            // }

            // Format the time string
            std::ostringstream oss;
            oss << std::setfill('0')
                << std::setw(4) << (tm_info.tm_year + 1900) << "-"
                << std::setw(2) << (tm_info.tm_mon + 1) << "-"
                << std::setw(2) << tm_info.tm_mday << " "
                << std::setw(2) << tm_info.tm_hour << ":"
                << std::setw(2) << tm_info.tm_min << ":"
                << std::setw(2) << tm_info.tm_sec << "."
                << std::setw(9) << nanos;
            // std::ostringstream oss;
            // oss << std::setfill('0')
            //     << std::setw(4) << (tm_info->tm_year + 1900) << "-"
            //     << std::setw(2) << (tm_info->tm_mon + 1) << "-"
            //     << std::setw(2) << tm_info->tm_mday << " "
            //     << std::setw(2) << tm_info->tm_hour << ":"
            //     << std::setw(2) << tm_info->tm_min << ":"
            //     << std::setw(2) << tm_info->tm_sec << "."
            //     << std::setw(9) << nanos;

            return oss.str();
        };

        TimeParseResult ParseIsoToUnix(std::string_view s) {
            struct tm t = { 0 };
            const char* data = s.data();

            // Helper to check from_chars results
            auto check = [&](const std::from_chars_result& res, std::string_view field) -> bool {
                if (res.ec != std::errc{}) return false;
                return true;
                };

            // Parsing YYYY-MM-DD HH:MM:SS
            if (!check(std::from_chars(data, data + 4, t.tm_year), "year"))  return { 0, "Invalid Year", false };
            if (!check(std::from_chars(data + 5, data + 7, t.tm_mon), "month")) return { 0, "Invalid Month", false };
            if (!check(std::from_chars(data + 8, data + 10, t.tm_mday), "day"))   return { 0, "Invalid Day", false };
            if (!check(std::from_chars(data + 11, data + 13, t.tm_hour), "hour"))  return { 0, "Invalid Hour", false };
            if (!check(std::from_chars(data + 14, data + 16, t.tm_min), "min"))   return { 0, "Invalid Minute", false };
            if (!check(std::from_chars(data + 17, data + 19, t.tm_sec), "sec"))   return { 0, "Invalid Second", false };

            t.tm_year -= 1900;
            t.tm_mon -= 1;

            if (t.tm_mon < 0 || t.tm_mon > 11) return { 0, "Month out of range", false };
            if (t.tm_mday < 1 || t.tm_mday > 31) return { 0, "Day out of range", false };
            if (t.tm_hour < 0 || t.tm_hour > 23) return { 0, "Hour out of range", false };
            if (t.tm_min < 0 || t.tm_min > 59) return { 0, "Minute out of range", false };
            if (t.tm_sec < 0 || t.tm_sec > 59) return { 0, "Second out of range", false };

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

            return { static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL + nanos, "", true };
        }

        // MARK: Get Timezone offset

        int GetTimezoneOffset(const std::string& timezone) {
            if (timezone == "UTC" || timezone == "GMT") {
                return 0;
            }
            else if (timezone == "EST" || timezone == "America/New_York") {
                return -5 * 3600; // EST is UTC-5 (ignoring DST for simplicity)
            }
            else if (timezone == "PST" || timezone == "America/Los_Angeles") {
                return -8 * 3600; // PST is UTC-8
            }
            else if (timezone == "CST" || timezone == "America/Chicago") {
                return -6 * 3600; // CST is UTC-6
            }
            else if (timezone == "MST" || timezone == "America/Denver") {
                return -7 * 3600; // MST is UTC-7
            }
            else if (timezone == "CET" || timezone == "Europe/Paris") {
                return 1 * 3600; // CET is UTC+1
            }
            else if (timezone == "JST" || timezone == "Asia/Tokyo") {
                return 9 * 3600; // JST is UTC+9
            }
            else if (timezone == "HKT" || timezone == "Asia/Hong_Kong") {
                return 8 * 3600; // HKT is UTC+8
            }
            else if (timezone == "EDT") {
                return -4 * 3600;
            }
            else if (timezone == "CDT") {
                return -5 * 3600;
            }
            else if (timezone == "PDT") {
                return -7 * 3600;
            }
            else {
                std::cerr << "Warning: Unknown timezone '" << timezone << "', using UTC" << std::endl;
                return 0;
            }
        }
    }
}