cmake_minimum_required(VERSION 3.10.0)
project(Backtester VERSION 0.1.0 LANGUAGES C CXX)

# add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
# add_link_options(-fsanitize=address)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# FetchContent_Declare(
#     databento
#     GIT_REPOSITORY https://github.com/databento/databento-cpp
#     GIT_TAG HEAD
# )

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

# FetchContent_Declare(
#     ryml
#     GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
#     GIT_TAG v0.10.0 
#     GIT_SHALLOW FALSE [1.1]
# )

FetchContent_MakeAvailable(googletest)

FetchContent_MakeAvailable(json spdlog)

add_library(CoreLogic src/data_ingestion/CsvZstReader.cpp src/market_state/OrderBook.cpp 
src/data_ingestion/DataReaderManager.cpp src/core/EventQueue.cpp 
src/market_state/MarketStateManager.cpp src/market_state/InstrumentState.cpp 
src/core/ConfigParser.cpp src/utils/TimeUtils.cpp src/portfolio/PortfolioManager.cpp
src/strategy/StrategyManager.cpp src/execution/ExecutionHandler.cpp src/core/Backtester.cpp
src/reporting/ReportGenerator.cpp
include/core/Constants.h src/utils/NumericUtils.cpp src/utils/StringUtils.cpp
include/strategy/IStrategy.h include/strategy/StrategyRegistry.h include/core/DefaultConfig.h
include/market_state/IMarketDataProvider.h src/strategy/user_strategies/MovAvgCross.cpp)


add_executable(Backtester src/main.cpp)

add_executable(tests test/utils/TimeUtils_test.cpp )


target_link_libraries(CoreLogic PRIVATE spdlog::spdlog zstd PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(tests PUBLIC nlohmann_json::nlohmann_json PRIVATE GTest::gtest_main zstd spdlog CoreLogic)
target_link_libraries(Backtester PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json -Wl,--whole-archive CoreLogic -Wl,--no-whole-archive)

add_executable(reader_perf_harness benchmarks/CsvZst_Ingestion_Harness.cpp src/data_ingestion/CsvZstReader.cpp 
src/data_ingestion/DataReaderManager.cpp)
target_link_libraries(reader_perf_harness PRIVATE spdlog::spdlog zstd)
target_compile_options(reader_perf_harness PRIVATE -O3 -g -fno-omit-frame-pointer)