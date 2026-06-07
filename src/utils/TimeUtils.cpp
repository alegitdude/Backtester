#include "utils/TimeUtils.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <stdexcept>
#include <charconv>
#include <optional>

namespace backtester {

    namespace time {
        TimeParseResult IsoFail(std::string_view s, size_t pos, std::string msg) {
            std::string caret(pos + 1, ' ');  // +1 for the opening quote
            caret += '^';
            return { 0, fmt::format("{}\n  \"{}\"\n  {}", msg, s, caret), false };
        }
        TimeParseResult IsoFail(std::string_view s, std::string msg) {
            return { 0, fmt::format("{} (in \"{}\")", msg, s), false };
        }

        TimeParseResult ParseIsoToUnix(std::string_view s) {
            struct tm t = { 0 };
            const char* data = s.data();

            if (s.length() < 19) {
                return IsoFail(s, fmt::format(
                    "timestamp too short: need at least 19 chars" 
                    "'YYYY-MM-DDTHH:MM:SS', got {}", s.length()));
            }

            if (s[4] != '-') return IsoFail(s, 4, fmt::format(
                "expected '-' between year and month, found '{}'", s[4]));
            if (s[7] != '-') return IsoFail(s, 7, fmt::format(
                "expected '-' between month and day, found '{}'", s[7]));
            if (s[10] != 'T' && s[10] != ' ')
                return IsoFail(s, 10, fmt::format(
                    "expected 'T' or ' ' between date and time, found '{}'", s[10]));
            if (s[13] != ':') return IsoFail(s, 13, fmt::format(
                "expected ':' between hour and minute, found '{}'", s[13]));
            if (s[16] != ':') return IsoFail(s, 16, fmt::format(
                "expected ':' between minute and second, found '{}'", s[16]));

            auto field = [&](size_t begin, size_t width, int& out,
                const char* name) -> std::optional<TimeParseResult> {
                    auto res = std::from_chars(data + begin, data + begin + width, out);
                    if (res.ec != std::errc{} || res.ptr != data + begin + width) {
                        return IsoFail(s, begin, fmt::format("{} must be {} digits, found \"{}\"",
                            name, width, std::string(s.substr(begin, width))));
                    }
                    return std::nullopt;
                };

            if (auto e = field(0, 4, t.tm_year, "year"))   return *e;
            if (auto e = field(5, 2, t.tm_mon, "month"))  return *e;
            if (auto e = field(8, 2, t.tm_mday, "day"))    return *e;
            if (auto e = field(11, 2, t.tm_hour, "hour"))   return *e;
            if (auto e = field(14, 2, t.tm_min, "minute")) return *e;
            if (auto e = field(17, 2, t.tm_sec, "second")) return *e;

            t.tm_year -= 1900;
            t.tm_mon -= 1;

            if (t.tm_mon < 0 || t.tm_mon  > 11) return IsoFail(s, 5, fmt::format(
                "month out of range: {} (expected 01-12)", t.tm_mon + 1));
            if (t.tm_mday < 1 || t.tm_mday > 31) return IsoFail(s, 8, fmt::format(
                "day out of range: {} (expected 01-31)", t.tm_mday));
            if (t.tm_hour < 0 || t.tm_hour > 23) return IsoFail(s, 11, fmt::format(
                "hour out of range: {} (expected 00-23)", t.tm_hour));
            if (t.tm_min < 0 || t.tm_min  > 59) return IsoFail(s, 14, fmt::format(
                "minute out of range: {} (expected 00-59)", t.tm_min));
            if (t.tm_sec < 0 || t.tm_sec  > 59) return IsoFail(s, 17, fmt::format(
                "second out of range: {} (expected 00-59)", t.tm_sec));

            time_t epoch_seconds = timegm(&t);

            uint64_t nanos = 0;
            size_t pos = 19;
            if (pos < s.length() && s[pos] == '.') {
                const char* nano_start = data + pos + 1;
                const char* nano_end = data + s.length();
                uint32_t raw_val = 0;
                auto res = std::from_chars(nano_start, nano_end, raw_val);
                if (res.ec != std::errc{}) {
                    return IsoFail(s, pos + 1, "fractional seconds after '.'" 
                        "must be 1-9 digits");
                }
                size_t digits = static_cast<size_t>(res.ptr - nano_start);
                if (digits > 9) {
                    return IsoFail(s, pos + 1, fmt::format(
                        "too many fractional digits: {} (max 9)", digits));
                }
                nanos = raw_val;
                for (size_t i = digits; i < 9; ++i) nanos *= 10;
                pos = static_cast<size_t>(res.ptr - data);
            }

            if (pos < s.length()) {
                if (s[pos] == 'Z' && pos + 1 == s.length()) {
                    // ok: UTC designator
                }
                else if (std::isdigit(static_cast<unsigned char>(s[pos]))) {
                    return IsoFail(s, pos,
                        "unexpected digit; if this is fractional seconds," 
                        "add a '.' after the seconds "
                        "(e.g. \"...:SS.nnnnnnnnn\")");
                }
                else if (s[pos] == '+' || s[pos] == '-') {
                    return IsoFail(s, pos,
                        "timezone offsets are not supported;" 
                        "use UTC (trailing 'Z' or no suffix)");
                }
                else {
                    return IsoFail(s, pos,
                        fmt::format("unexpected trailing characters: \"{}\"", 
                            std::string(s.substr(pos))));
                }
            }

            return { static_cast<uint64_t>(epoch_seconds) * 1'000'000'000ULL + nanos, "", true };
        }

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