#include "IDataReader.h"
#include <string>
#include <fstream>

namespace backtester {

class CsvReader : public IDataReader {
 public:
    CsvReader() = default;

    ~CsvReader();

    bool Open(const std::string& filename) override;
    
    void Close() override;
    
    bool ReadLine(std::string& line) override; 

 private:
    std::ifstream file_;
};

}