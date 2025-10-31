#include "CsvZstReader.h"
#include <iostream>
// Include file stream for reading binary files
#include <fstream>
// Include string stream for parsing CSV lines
#include <sstream>
#include <vector>
#include <string>
#include <zstd.h>

    
CsvZstReader::~CsvZstReader() {
    close();
}

bool CsvZstReader::open(const std::string& filename) {
    // Reset state variables
    output_pos_ = 0;
    output_size_ = 0;
    eof_reached_ = false;

    // Binary required for compressed data)
    file_.open(filename, std::ios::binary);

    if (!file_.is_open()) return false;

    // Create ZSTD decompression stream context
    dstream_ = ZSTD_createDStream();
    if (!dstream_) return false;

    // Initialize the decompression stream and check for errors
    return ZSTD_initDStream(dstream_) != ZSTD_isError(ZSTD_initDStream(dstream_));
}
void CsvZstReader::close() {
    // Close file if it's open
    if (file_.is_open()) file_.close();
    // Free ZSTD decompression context if it exists
    if (dstream_) {
        ZSTD_freeDStream(dstream_);
        dstream_ = nullptr;
    }
}

bool CsvZstReader::readLine(std::string& line) {
    // Clear the output line to start fresh
    line.clear();
    
    // Loop until we find a complete line or reach end of file
    while (true) {
        // Scan through decompressed buffer looking for newline character
        for (size_t i = output_pos_; i < output_size_; ++i) {
            if (output_buffer_[i] == '\n') {
                // Found newline - extract line from buffer
                line.assign(output_buffer_.data() + output_pos_, i - output_pos_);
                // Move position past the newline for next read
                output_pos_ = i + 1;
                return true;
            }
        }
        
        // No newline found - append remaining buffer data to line
        if (output_pos_ < output_size_) {
            line.append(output_buffer_.data() + output_pos_, output_size_ - output_pos_);
        }
        
        // Need more data - decompress next chunk
        if (!fillBuffer()) {
            // No more data available - return true if we have a partial line
            return !line.empty(); // Return partial line if exists
        }
    }
}


// Reads compressed data from file and decompresses it into output buffer
bool CsvZstReader::fillBuffer() {
    if (eof_reached_) return false;
    
    // Read next chunk of compressed data from file
    file_.read(input_buffer_.data(), input_buffer_.size());
    // Get actual number of bytes read (may be less than requested)
    size_t bytes_read = file_.gcount();

    if (bytes_read == 0) {
        eof_reached_ = true;
        return false;
    }
    
    // Set up ZSTD input buffer structure pointing to compressed data
    ZSTD_inBuffer input = {input_buffer_.data(), bytes_read, 0};
    // Set up ZSTD output buffer structure pointing to decompression buffer
    ZSTD_outBuffer output = {output_buffer_.data(), output_buffer_.size(), 0};
    
    // Perform decompression of this chunk
    size_t ret = ZSTD_decompressStream(dstream_, &output, &input);
    // Check if decompression failed
    if (ZSTD_isError(ret)) return false;
    
    // Update buffer state with amount of decompressed data
    output_size_ = output.pos;
    // Reset read position to start of new data
    output_pos_ = 0;
    
    // Check if we've processed all input and reached end of file
    if (file_.eof() && input.pos == input.size) {
        eof_reached_ = true;
    }
    
    // Return true if we got some decompressed data
    return output_size_ > 0;
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