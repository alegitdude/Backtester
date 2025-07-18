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