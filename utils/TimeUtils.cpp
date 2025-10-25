#include "TimeUtils.h"
#include <iostream>
#include <stdexcept>

std::string TimeUtils::EpochToString(uint64_t epoch_nanos, const std::string& timezone = "UTC") {
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

uint64_t TimeUtils::StringToEpoch(const std::string& time_str, const std::string& timezone) {
    int year, month, day, hour, minute, second;
    uint32_t nanoseconds = 0;
    
    // Parse the time string
    if (!ParseTimeString(time_str, year, month, day, hour, minute, second, nanoseconds)) {
        throw std::invalid_argument("Invalid time format. Expected: YYYY-MM-DD HH:MM:SS[.nnnnnnnnn]");
    }
    
    // Create tm struct
    struct tm tm_info = {};
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = month - 1;
    tm_info.tm_mday = day;
    tm_info.tm_hour = hour;
    tm_info.tm_min = minute;
    tm_info.tm_sec = second;
    
    // Convert to epoch seconds (treat as UTC)
    time_t epoch_seconds = timegm(&tm_info);
    if (epoch_seconds == -1) {
        throw std::runtime_error("Failed to convert time to epoch");
    }
    
    // Apply timezone offset (subtract because we're converting FROM local TO UTC)
    int tz_offset = GetTimezoneOffset(timezone);
    epoch_seconds -= tz_offset;
    
    // Convert to nanoseconds and add nanosecond component
    uint64_t epoch_nanos = static_cast<uint64_t>(epoch_seconds) * 1000000000ULL + nanoseconds;
    
    return epoch_nanos;
}

int TimeUtils::GetTimezoneOffset(const std::string& timezone) {
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

bool TimeUtils::ParseTimeString(const std::string& time_str,
                                int& year, int& month, int& day,
                                int& hour, int& minute, int& second,
                                uint32_t& nanoseconds) {
    // Expected formats:
    // "YYYY-MM-DD HH:MM:SS"
    // "YYYY-MM-DD HH:MM:SS.nnnnnnnnn"
    
    std::istringstream iss(time_str);
    char dash1, dash2, space, colon1, colon2, dot;
    
    // Parse date part
    iss >> year >> dash1 >> month >> dash2 >> day;
    if (dash1 != '-' || dash2 != '-') {
        return false;
    }
    
    // Parse time part
    iss >> space >> hour >> colon1 >> minute >> colon2 >> second;
    if (space != ' ' || colon1 != ':' || colon2 != ':') {
        return false;
    }
    
    // Try to parse nanoseconds (optional)
    if (iss >> dot) {
        if (dot != '.') {
            return false;
        }
        
        std::string nanos_str;
        iss >> nanos_str;
        
        // Pad or truncate to 9 digits
        if (nanos_str.length() > 9) {
            nanos_str = nanos_str.substr(0, 9);
        } else {
            nanos_str.append(9 - nanos_str.length(), '0');
        }
        
        nanoseconds = std::stoul(nanos_str);
    } else {
        nanoseconds = 0;
    }
    
    // Basic validation
    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || 
				second > 59) {
          return false;
    }
    
    return true;
}