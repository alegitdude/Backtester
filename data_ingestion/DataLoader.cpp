

class DataLoader{
    private:
        static int64_t nanosToSeconds(int64_t nanos){
            return nanos / 1'000'000'000;
        };

    public:
        static int64_t parseDbIsoFormat(const char* str){
            // Assumed format of 2025-06-25T08:00:10.000000000Z --DB has same format for every OHLCV
            //                   YYYY-MM-DDTHH:MM:SS.sssz

            // Convert all datetime str values to ints
            int year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + 
               (str[2] - '0') * 10 + (str[3] - '0');
            int month = (str[5] - '0') * 10 + (str[6] - '0');
            int day = (str[8] - '0') * 10 + (str[9] - '0');
            int hour = (str[11] - '0') * 10 + (str[12] - '0');
            int minute = (str[14] - '0') * 10 + (str[15] - '0');
            int second = (str[17] - '0') * 10 + (str[18] - '0');

            // Convert to tm struct
            std::tm tm = {};
            tm.tm_year = year - 1900;  // tm_year is years since 1900
            tm.tm_mon = month - 1;      // tm_mon is 0-11
            tm.tm_mday = day;
            tm.tm_hour = hour;
            tm.tm_min = minute;
            tm.tm_sec = second;

            return timegm(&tm); 
        }

        static std::vector<OHLCBar> loadCsv(const std::string& filePath, DataInterval& interval){
            //Try to open the file
            FILE* file = fopen(filePath.c_str(), "r");
            //Check if the filepath is real and openable + opened
            if (!file){
                throw std::runtime_error("Failed to open file at:" + filePath);
            }

            //Declare return object
            std::vector<OHLCBar> bars;
            //Pre allocate the amount of time in the bars
            //Seconds for one day 234000, TODO generalize later
            bars.reserve(234000);

            //Skip headers
            char line[512];
            fgets(line, sizeof(line), file);  // Skip header
        
            while (fgets(line, sizeof(line), file)) {       
                OHLCBar bar;
                char ts_event[64];
                // Skip rtype, publisher_id, instrument_id
                if (sscanf(line, "%63[^,],%*d,%*d,%*d,%lf,%lf,%lf,%lf,%lf",
                        ts_event,
                        &bar.open,
                        &bar.high,
                        &bar.low,
                        &bar.close,
                        &bar.volume) == 6) {
                    
                        bar.timestamp = parseDbIsoFormat(ts_event);
                        bars.push_back(bar);
                    }
               
            }   

            fclose(file);
            return bars;     
        };
    };