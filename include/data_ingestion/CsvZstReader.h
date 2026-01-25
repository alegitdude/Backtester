#pragma once

#include <iostream>
// Include file stream for reading binary files
#include <fstream>
// Include string stream for parsing CSV lines
#include <sstream>
#include <vector>
#include <string>
#include <zstd.h>

namespace backtester {

class CsvZstReader { 
 public:
    CsvZstReader() : dstream_(nullptr), output_pos_(0), output_size_(0), 
		                 eof_reached_(false) {
        						 input_buffer_.resize(input_buf_size);
        						 output_buffer_.resize(output_buf_size);
    }
    
    ~CsvZstReader();

    bool Open(const std::string& filename);
    
    void Close();
    
    bool ReadLine(std::string& line); 

 private:
    std::ifstream file_;
    ZSTD_DStream* dstream_;
    std::vector<char> input_buffer_;
    std::vector<char> output_buffer_;
    size_t input_pos_;
    size_t input_valid_size_;
    // Current read position in the output buffer
    size_t output_pos_;
    // Amount of data in the output buffer
    size_t output_size_;
    // Flag to track if we've reached end of file
    bool eof_reached_;
    
    size_t input_buf_size = ZSTD_DStreamInSize();
    size_t output_buf_size = ZSTD_DStreamOutSize();

    // Reads compressed data from file and decompresses it into output buffer
    bool FillBuffer(); 
};

}