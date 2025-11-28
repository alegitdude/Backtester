#include "../../data_ingestion/CsvZstReader.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <zstd.h>

#include <iostream>
#include <vector>
#include <string>
#include <charconv>
#include <memory>
#include <fstream>

namespace backtester {

class CsvZstReaderTest : public ::testing::Test {
 protected:
//   void SetUp() override {
//   }

//   void TearDown() override {
//   }
  std::string path_to_test_data = "../test/test_data/";
  std::string path_to_simplezst = path_to_test_data + "simple.zst";
  std::string path_to_emptyzst = path_to_test_data + "empty.zst";
  std::string path_to_multilinezst = path_to_test_data + "multiline.zst";

  void createZstFile(const std::filesystem::path& filename, 
                   const std::string& content, 
                   int compressionLevel = 1) {
    // Validate compression level
    if (compressionLevel < 1 || compressionLevel > ZSTD_maxCLevel()) {
        throw std::invalid_argument("Invalid compression level");
    }
    
    // Calculate buffer size for compressed data
    const size_t maxCompressedSize = ZSTD_compressBound(content.size());
    std::vector<char> compressedBuffer(maxCompressedSize);
    
    // Compress the content directly (no temp file needed)
    const size_t compressedSize = ZSTD_compress(
        compressedBuffer.data(), 
        maxCompressedSize,
        content.data(), 
        content.size(), 
        compressionLevel
    );
    
    // Check for compression errors with descriptive message
    if (ZSTD_isError(compressedSize)) {
        throw std::runtime_error(
            std::string("Zstd compression failed: ") + 
            ZSTD_getErrorName(compressedSize)
        );
    }
    
    // Write compressed data to file with error checking
    std::ofstream output(filename, std::ios::binary);
    if (!output) {
        throw std::runtime_error(
            "Failed to open output file: " + filename.string()
        );
    }
    
    output.write(compressedBuffer.data(), compressedSize);
    if (!output) {
        throw std::runtime_error(
            "Failed to write compressed data to: " + filename.string()
        );
    }
  }
};

TEST_F(CsvZstReaderTest, OpenValidFile) {
    CsvZstReader reader;
    EXPECT_TRUE(reader.open(path_to_simplezst));
}

TEST_F(CsvZstReaderTest, ReadSingleLine) {    
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_simplezst));
    
    std::string line;
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "single line");
    
    // Should return false on second read (EOF)
    EXPECT_FALSE(reader.readLine(line));
}

// Test opening a non-existent file
TEST_F(CsvZstReaderTest, OpenNonExistentFile) {
    CsvZstReader reader;
    EXPECT_FALSE(reader.open("test_data/does_not_exist.zst"));
}

// Test opening an invalid/corrupt zst file
TEST_F(CsvZstReaderTest, OpenInvalidZstFile) {
    // Create a file that's not actually zst compressed
    std::ofstream file("test_data/invalid.zst");
    file << "This is not compressed data";
    file.close();
    
    CsvZstReader reader;
    // May open successfully but fail on read
    reader.open("test_data/invalid.zst");
    std::string line;
    EXPECT_FALSE(reader.readLine(line));
}

// Test reading multiple lines // MARK: NEED MORE testing
TEST_F(CsvZstReaderTest, ReadMultipleLines) {
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_multilinezst));
    
    std::string line;
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line1");
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line2");
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line3");
    
    EXPECT_FALSE(reader.readLine(line));
}

// // Test reading empty file
TEST_F(CsvZstReaderTest, ReadEmptyFile) {
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_emptyzst));
    
    std::string line;
    EXPECT_FALSE(reader.readLine(line));
}

// // Test file with no trailing newline /// MARK: FAIL!!!!!!!!!!!!!!
TEST_F(CsvZstReaderTest, ReadFileNoTrailingNewline) {
    createZstFile(path_to_test_data + "notrailingNL", "line1\nline2");
    
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_test_data + "notrailingNL"));
    
    std::string line;
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line1");
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line2");
    
    EXPECT_FALSE(reader.readLine(line));
}

// // Test reading empty lines
TEST_F(CsvZstReaderTest, ReadEmptyLines) {
    createZstFile(path_to_test_data + "empty_lines.zst", "line1\n\nline3\n");
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_test_data + "empty_lines.zst"));
    
    std::string line;
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line1");
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "");  
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line3");
}

// // Test reading very long line (larger than buffer)
TEST_F(CsvZstReaderTest, ReadLongLine) {
    std::string long_line(100000, 'a');  // 100KB line
    createZstFile(path_to_test_data + "long.zst", long_line + "\n");
    
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_test_data + "long.zst"));
    
    std::string line;
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, long_line);
}

// // Test close method
TEST_F(CsvZstReaderTest, CloseFile) {
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_simplezst));
    
    std::string line;
    EXPECT_TRUE(reader.readLine(line));
    
    reader.close();
    
    // After close, readLine should fail
    EXPECT_FALSE(reader.readLine(line));
}

// // Test multiple close calls (should be safe)
TEST_F(CsvZstReaderTest, MultipleClose) {
    CsvZstReader reader;
    reader.close();
    reader.close();  // Should not crash
    SUCCEED();
}

// // Test opening file multiple times 
TEST_F(CsvZstReaderTest, ReopenFile) {
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_multilinezst));
    
    std::string line;
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line1");
    
    // Close and reopen
    reader.close();
    ASSERT_TRUE(reader.open(path_to_multilinezst));
    
    // Should read from beginning again
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "line1");
}

// Test reading without opening file
TEST_F(CsvZstReaderTest, ReadWithoutOpen) {
    CsvZstReader reader;
    std::string line;
    EXPECT_FALSE(reader.readLine(line));
}

// // Test destructor cleanup (memory leak test - use valgrind)
TEST_F(CsvZstReaderTest, DestructorCleanup) {
    
    {
        CsvZstReader reader;
        reader.open(path_to_multilinezst);
        // Reader goes out of scope, destructor should clean up
    }
    SUCCEED();  // If no crash/leak, test passes
}

// // Test with CSV-like content
TEST_F(CsvZstReaderTest, ReadCsvContent) {
    std::string csv = "name,age,city\n"
                      "Alice,30,NYC\n"
                      "Bob,25,LA\n";
    createZstFile(path_to_test_data + "csvdata.zst", csv);
    
    CsvZstReader reader;
    ASSERT_TRUE(reader.open(path_to_test_data + "csvdata.zst"));
    
    std::string line;
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "name,age,city");
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "Alice,30,NYC");
    
    EXPECT_TRUE(reader.readLine(line));
    EXPECT_EQ(line, "Bob,25,LA");
}

TEST_F(CsvZstReaderTest, ReadRealMBOFile) {
    CsvZstReader reader;
    reader.open(path_to_test_data + "futures_mbo.csv.zst");
    int lines = 0;
    std::string line;
     while (reader.readLine(line)) {  // This returns bool
        lines++;  // Need to increment inside the loop
    }
    EXPECT_EQ(lines, 21);  // Known sample size
}

}