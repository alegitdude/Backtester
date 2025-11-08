#include<iostream>
#include <ctime>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

struct OHLCVBar {
    int64_t timestamp;
    uint32_t instrumentId;
    double open, high, low, close, volume;    
}; 

struct DbOHCLV1sRow{
    std::string tsEvent;
    int rtype;
    int publisherId;
    int instrumentId;
    double open, high, low, close;
    int volume;
    std::string symbol;
};

struct MboMsg {
    uint64_t ts_recv;		//The capture-server-received timestamp expressed as the number of nanoseconds since the UNIX epoch. See ts_recv.
    uint64_t ts_event;		//The matching-engine-received timestamp expressed as the number of nanoseconds since the UNIX epoch. See ts_event.
    uint8_t rtype;		    //A sentinel value indicating the record type. Always 160 in the MBO schema. See Rtype.
    uint16_t publisher_id;		//The publisher ID assigned by Databento, which denotes the dataset and venue. See Publishers.
    uint32_t instrument_id;		//The numeric instrument ID. See Instrument identifiers.
    char action;		//The event action. Can be Add, Cancel, Modify, cleaR book, Trade, Fill, or None. See Action.
    char side;		//The side that initiates the event. Can be Ask for a sell order (or sell aggressor in a trade), Bid for a buy order (or buy aggressor in a trade), or None where no side is specified. See Side.
    int64_t price;		//The order price where every 1 unit corresponds to 1e-9, i.e. 1/1,000,000,000 or 0.000000001. See Prices.
    uint32_t size;		//The order quantity.
    uint8_t channel_id;		  //The channel ID assigned by Databento as an incrementing integer starting at zero.
    uint64_t order_id;		//The order ID assigned by the venue.
    uint8_t flags;		//A bit field indicating event end, message characteristics, and data quality. See Flags.
    int32_t ts_in_delta;		//The matching-engine-sending timestamp expressed as the number of nanoseconds before ts_recv. See ts_in_delta.
    uint32_t sequence;		//The message sequence number assigned at the venue.	
};

// namespace {

// class DtaBntoOHLC1sCsvLdr {
//     public:
//         static std::vector<OHLCVBar> load_csv(const std::string& filePath){
//             std::ifstream file(filePath);
//             if(!file.is_open()) {
//                 throw std::runtime_error("Failed to open file:" + filePath);
//             }

//             std::vector<OHLCVBar> bars;

//             std::string line;
//             std::getline(file, line);

//             while (std::getline(file, line)){
//                 DbOHCLV1sRow bar = parse_line(line);
//             }
//         }

//         static DbOHCLV1sRow parse_line(std::string& line)
//         {
//             return line;
//         }

// }


// }
long long StringToEpoch(const std::string& time_str, 
		                            const std::string& timezone);
long long StringToEpoch(const std::string& time_str, const std::string& timezone = "UTC") {
    int year, month, day, hour, minute, second;
    uint32_t nanoseconds = 0;
    std::cout << year << month << day << hour << minute << second << std::endl;
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


bool ParseTimeString(const std::string& time_str, int& year, int& month, 
  int& day, int& hour, int& minute, int& second, uint32_t& nanoseconds);

bool ParseTimeString(const std::string& time_str,
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