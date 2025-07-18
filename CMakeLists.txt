cmake_minimum_required(VERSION 3.10.0)
project(Backtester VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)
   FetchContent_Declare(
       databento
       GIT_REPOSITORY https://github.com/databento/databento-cpp
       GIT_TAG HEAD
   )
   FetchContent_MakeAvailable(databento)
add_executable(Backtester main.cpp)

target_link_libraries(Backtester PRIVATE databento::databento)

