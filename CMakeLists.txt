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
  GIT_TAG        v1.14.1 
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

FetchContent_Declare(
    ryml
    GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    GIT_TAG v0.10.0 
    GIT_SHALLOW FALSE [1.1]
)

FetchContent_MakeAvailable(googletest)

FetchContent_MakeAvailable(databento json spdlog ryml)
  
add_executable(Backtester src/main.cpp src/data_ingestion/CsvZstReader.cpp 
src/data_ingestion/DataReaderManager.cpp src/core/EventQueue.cpp src/core/ConfigParser.cpp 
src/utils/TimeUtils.cpp src/core/Backtester.cpp src/market_state/MarketStateManager.cpp
src/market_state/OrderBook.cpp src/market_state/InstrumentState.cpp)

add_executable(tests test/core/ConfigParser_test.cpp src/data_ingestion/CsvZstReader.cpp
src/market_state/OrderBook.cpp src/data_ingestion/DataReaderManager.cpp src/core/EventQueue.cpp
src/market_state/MarketStateManager.cpp src/market_state/InstrumentState.cpp src/core/ConfigParser.cpp
src/utils/TimeUtils.cpp include/core/Constants.h src/utils/NumericUtils.cpp src/utils/StringUtils.cpp)

target_link_libraries(tests GTest::gtest_main zstd spdlog nlohmann_json::nlohmann_json)
target_link_libraries(Backtester PRIVATE databento::databento nlohmann_json::nlohmann_json spdlog ryml::ryml)

