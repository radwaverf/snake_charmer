cmake_minimum_required(VERSION 3.16.3)
project(snake_charmer VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

# Dependencies
find_package(doctest)
find_package(spdlog REQUIRED)

# Library
add_library(snake_charmer
    src/ring_buffer.cpp
    src/copy_ring_buffer.cpp
    src/direct_ring_buffer.cpp
)

# Tests
if(doctest_FOUND)
    add_subdirectory(tests)
endif(doctest_FOUND)
