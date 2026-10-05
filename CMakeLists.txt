cmake_minimum_required(VERSION 3.20)
project(LimitOrderBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(lob INTERFACE)
target_include_directories(lob INTERFACE src)

foreach(t bench generate_stream differential_test)
  add_executable(${t} tools/${t}.cpp)
  target_link_libraries(${t} PRIVATE lob)
endforeach()

target_compile_options(bench PRIVATE -O3 -march=native)