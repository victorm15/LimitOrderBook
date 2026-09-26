cmake_minimum_required(VERSION 3.20)
project(LimitOrderBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(lob src/main.cpp)

target_include_directories(lob PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_options(lob PRIVATE -Wall -Wextra -Wpedantic)