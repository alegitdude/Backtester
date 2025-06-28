#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <charconv>

struct Position {
    double entryPrice;
    long entryTime;
    int quantity;

    double UnrealizedPnL(double currentPrice){
        return quantity * (currentPrice - entryPrice);
    }
};

struct Trade {
    double entryPrice, exitPrice;
    long entryTime, exitTime;
    int quantity;
};


struct OHLCBar {
    double open, high, low, close, volume;
    long timestamp;
}; 

class DataLoader{
   public:
        //std::vector<OHLCBar> 
        static void loadCsv(const std::string& filePath){
            //Try to open the file
            std::ifstream file(filePath);
            //Check if the filepath is real and openable + opened
            if (!file.is_open()){
                throw std::runtime_error("Failed to open file at:" + filePath);
            }

            //Declare return object
            std::vector<OHLCBar> bars;

            std::string line;

            //Skip headers
            //std::getline(file, line);
            int count = 0;
            while(std::getline(file, line)){
                //Skip empty lines
                if(line.empty()) continue;
                if(count > 9){
                    break;
                }
                std::cout<< line;
                count++;
                //OHLCBar bar;
                
            }


        };
};

std::string GetCSVTestEnvVar () {
     std::ifstream file(".env");

    if (!file.is_open()) {
        std::cerr << "Error: Could not open environment variable file:";
        return "";
    }
    std::string csvFilePath;
    std::string line;
    while (std::getline(file, line)) {
        size_t equalsPos = line.find('=');
        if (equalsPos != std::string::npos) {
            std::string key = line.substr(0, equalsPos - 1);
            std::string value = line.substr(equalsPos + 2);
            if(key == "PATH_TO_QQQ_OHLCV_CSV"){
                return value;
            }
        }
    }
    file.close();
    return "";
}

int main(int, char**){
    std::cout << "Hello, from Backtester!\n";
    DataLoader dataLoader;
    dataLoader.loadCsv(GetCSVTestEnvVar());
    return 0;
}
