#pragma once
#include <string>
class IDataReader {
 public:

    virtual ~IDataReader() = default;

    virtual bool Open(const std::string& filename) = 0;
    
    virtual void Close() = 0;
    
    virtual bool ReadLine(std::string& line) = 0; 
};