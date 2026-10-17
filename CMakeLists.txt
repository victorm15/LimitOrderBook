cmake_minimum_required(VERSION 3.20)
project(LimitOrderBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(lob INTERFACE)
target_include_directories(lob INTERFACE src)
target_compile_options(lob INTERFACE -Wall -Wextra -march=native)

foreach(t benchmark generate_stream differential_test book_stats noise)
  add_executable(${t} tools/${t}.cpp)
  target_link_libraries(${t} PRIVATE lob)
endforeach()

target_compile_definitions(book_stats PRIVATE BOOK_STATS)