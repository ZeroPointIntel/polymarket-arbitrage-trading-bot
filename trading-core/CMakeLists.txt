cmake_minimum_required(VERSION 3.15)
project(trading-core CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for performance and strict warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto -Wall -Wextra -Wpedantic -Wno-unused-parameter")
endif()

# Find dependencies provided by Conan
find_package(Boost REQUIRED COMPONENTS system thread json)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    src/state/StateStore.cpp
    src/feeds/BinanceFeed.cpp
    src/feeds/PolymarketFeed.cpp
)

add_executable(trading-core ${SOURCES})

# Include directories
target_include_directories(trading-core PRIVATE src)

# Disable spdlog internal exceptions to bypass Apple Clang 15 C++20 consteval bugs
target_compile_definitions(trading-core PRIVATE SPDLOG_NO_EXCEPTIONS)

# Link dependencies
target_link_libraries(trading-core PRIVATE
    Boost::headers
    Boost::system
    Boost::thread
    Boost::json
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)
