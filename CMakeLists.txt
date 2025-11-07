cmake_minimum_required(VERSION 3.10.0)
project(Backtester VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    databento
    GIT_REPOSITORY https://github.com/databento/databento-cpp
    GIT_TAG HEAD
)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.14.1 # Recommended to use a specific version tag
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

FetchContent_MakeAvailable(googletest)

FetchContent_MakeAvailable(databento json spdlog)
  
add_executable(Backtester main.cpp data_ingestion/CsvZstReader.cpp 
data_ingestion/DataReaderManager.cpp core/EventQueue.cpp core/ConfigParser.cpp utils/TimeUtils.cpp)

add_executable(tests test/data_ingestion/CsvZstReader_test.cpp 
data_ingestion/CsvZstReader.cpp utils/TimeUtils.cpp)


target_link_libraries(tests GTest::gtest_main zstd)
target_link_libraries(Backtester PRIVATE databento::databento nlohmann_json::nlohmann_json spdlog)

