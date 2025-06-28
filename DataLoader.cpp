#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <charconv>

struct OHLCBar {
    double open, high, low, close, volume;
    long timestamp;
}; 

class DataLoader{
   public:
        static std::vector<OHLCBar> loadCsv(const std::string& filePath){
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


        }
};