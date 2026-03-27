#include "../../include/data_ingestion/CsvZstReader.h"
#include "spdlog/spdlog.h"
#include <iostream>
// Include file stream for reading binary files
#include <fstream>
// Include string stream for parsing CSV lines
#include <sstream>
#include <vector>
#include <string>
#include <zstd.h>

namespace backtester {
    
CsvZstReader::~CsvZstReader() {
    Close();
}

// MARK: OPEN
bool CsvZstReader::Open(const std::string& filename) {
    // Reset state variables
    input_pos_ = 0;
    input_valid_size_ = 0;
    output_pos_ = 0;
    output_size_ = 0;
    eof_reached_ = false;

    // Binary required for compressed data
    file_.open(filename, std::ios::binary);

    if (!file_.is_open()) return false;

    // Create ZSTD decompression stream context
    dstream_ = ZSTD_createDStream();
    if (!dstream_) return false;

    // Initialize the decompression stream and check for errors
    size_t const init_result = ZSTD_initDStream(dstream_);
    return !ZSTD_isError(init_result);
}
// MARK: CLOSE
void CsvZstReader::Close() {
    // Close file if it's open
    if (file_.is_open()) file_.close();
    // Free ZSTD decompression context if it exists
    if (dstream_) {
        ZSTD_freeDStream(dstream_);
        dstream_ = nullptr;
    }
}

// MARK: READLINE
bool CsvZstReader::ReadLine(std::string& line) {
    line.clear();
    
    while (true) {
        // 1. Scan for newline
        for (size_t i = output_pos_; i < output_size_; ++i) {
            if (output_buffer_[i] == '\n') {
                line.append(output_buffer_.data() + output_pos_, i - output_pos_);
                
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                output_pos_ = i + 1; 
                return true;
            }
        }
        
        // 2. No newline found - consume EVERYTHING remaining in buffer
        if (output_pos_ < output_size_) {
            line.append(output_buffer_.data() + output_pos_, output_size_ - output_pos_);
            
            // Mark this data as consumed.
            // If you don't do this, FillBuffer thinks the buffer is full of unread data,
            // returns true immediately, and you append the same text again forever.
            output_pos_ = output_size_; 
        }
        
        // 3. Buffer is exhausted, try to fill it with more data
        if (!FillBuffer()) {
            // EOF reached. Return true if we have any data left in 'line'
            return !line.empty();
        }
    }
}

// MARK: FILLBUFFER
// Reads compressed data from file and decompresses it into output buffer
bool CsvZstReader::FillBuffer() {
    // If output still has data (rare logic case, but safe to check)
    if (output_pos_ < output_size_) return true;

    // Reset output pointers
    output_pos_ = 0;
    output_size_ = 0;

    ZSTD_outBuffer output = {output_buffer_.data(), output_buffer_.size(), 0};

    // Loop until we have produced some output or truly hit EOF
    while (output.pos == 0) {
        
        // 1. REFILL INPUT: Only read from disk if we have consumed previous input
        if (input_pos_ >= input_valid_size_) {
            if (file_.eof()) {
                eof_reached_ = true;
                return false;
            }

            file_.read(input_buffer_.data(), input_buffer_.size());
            input_valid_size_ = file_.gcount();
            input_pos_ = 0; // Reset position to start of buffer

            if (input_valid_size_ == 0) {
                eof_reached_ = true;
                return false;
            }
        }

        // 2. DECOMPRESS: Set up input pointing to CURRENT position
        ZSTD_inBuffer input = {input_buffer_.data(), input_valid_size_, input_pos_};

        size_t ret = ZSTD_decompressStream(dstream_, &output, &input);
        
        if (ZSTD_isError(ret)) {
            spdlog::warn("zstd error in FillBuffer");
            return false;
        }

        // 3. UPDATE STATE: Update class member with how much ZSTD consumed
        input_pos_ = input.pos; 

        // If we produced output, we are done with this step
        if (output.pos > 0) {
            output_size_ = output.pos;
            return true;
        }
    }
    return false;
}


// User calls readLine()
//     
// Scan current buffer for '\n'
//     
// No newline found?
//     
// Save partial data to line
//     
// Call fillBuffer() ← HERE
//     
// Read compressed data from disk
//     
// Decompress into output buffer
//     
// Continue scanning for newlines
}