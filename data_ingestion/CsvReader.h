#include <string>
#include <fstream>

namespace backtester {

class CsvReader {
 public:
    CsvReader() = default;

    ~CsvReader();

    bool Open(const std::string& filename);
    
    void Close();
    
    bool ReadLine(std::string& line); 

 private:
    std::ifstream file_;
};

}