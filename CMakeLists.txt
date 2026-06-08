cmake_minimum_required(VERSION 3.10.0)
project(Backtester VERSION 0.1.0 LANGUAGES C CXX)
 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
 
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
 
include(FetchContent)
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
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest spdlog json)
 
add_compile_options(-g -fno-omit-frame-pointer)
 
file(GLOB USER_STRATEGY_SOURCES CONFIGURE_DEPENDS
  "src/strategy/user_strategies/*.cpp")
 
add_library(CoreLogic STATIC
  src/data_ingestion/CsvZstReader.cpp
  src/market_state/OrderBook.cpp
  src/data_ingestion/DataReaderManager.cpp
  src/core/EventQueue.cpp
  src/market_state/MarketStateManager.cpp
  src/market_state/InstrumentState.cpp
  src/core/ConfigParser.cpp
  src/utils/TimeUtils.cpp
  src/portfolio/PortfolioManager.cpp
  src/strategy/StrategyManager.cpp
  src/execution/ExecutionHandler.cpp
  src/core/Backtester.cpp
  src/reporting/ReportGenerator.cpp
  src/utils/NumericUtils.cpp
  src/utils/StringUtils.cpp
  ${USER_STRATEGY_SOURCES}
)

target_include_directories(CoreLogic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(CoreLogic PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(CoreLogic 
  PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog 
  PRIVATE zstd
)
target_compile_options(CoreLogic PRIVATE -O3 -g -fno-omit-frame-pointer)
 
## Main executable
add_executable(Backtester src/main.cpp)
target_link_libraries(Backtester PRIVATE
  -Wl,--whole-archive CoreLogic -Wl,--no-whole-archive)

## Tests
add_executable(tests
  test/core/ConfigParser_test.cpp
  test/data_ingestion/CsvZstReader_test.cpp
  test/portfolio/PortfolioManager_test.cpp
  test/utils/TimeUtils_test.cpp
  test/execution/ExecutionHandler_test.cpp
  test/market_state/OrderBook_test.cpp
)
target_compile_definitions(tests PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/test_data"
  PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(tests PRIVATE GTest::gtest_main CoreLogic)
 
## Harnesses
add_executable(reader_perf_harness benchmarks/CsvZst_Ingestion_Harness.cpp)
target_link_libraries(reader_perf_harness PRIVATE spdlog::spdlog zstd CoreLogic)
target_compile_options(reader_perf_harness PRIVATE -O3 -g -fno-omit-frame-pointer)
target_compile_definitions(reader_perf_harness PRIVATE
  BENCH_CONFIG_DEFAULT="${CMAKE_SOURCE_DIR}/config/demo.json")

add_executable(orderbook_perf_harness benchmarks/OrderBook_Perf_Harness.cpp)
target_link_libraries(orderbook_perf_harness PRIVATE spdlog::spdlog zstd CoreLogic)
target_compile_options(orderbook_perf_harness PRIVATE -O3 -g -fno-omit-frame-pointer)
target_compile_definitions(orderbook_perf_harness PRIVATE
  BENCH_CONFIG_DEFAULT="${CMAKE_SOURCE_DIR}/config/demo.json")
