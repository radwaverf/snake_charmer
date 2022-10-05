cmake_minimum_required(VERSION 3.16.3)
project(snake_charmer VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

# Dependencies
find_package(doctest REQUIRED)
find_package(spdlog REQUIRED)

# Library
add_library(snake_charmer
    src/ring_buffer.cpp
    src/copy_ring_buffer.cpp
    src/direct_ring_buffer.cpp
)

# Tests

add_executable(test_copy_ring_buffer tests/copy_ring_buffer.cpp)
target_link_libraries(test_copy_ring_buffer PRIVATE
    doctest::doctest
    snake_charmer
)
target_include_directories(test_copy_ring_buffer PUBLIC 
    ${DOCTEST_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

add_executable(test_direct_ring_buffer tests/direct_ring_buffer.cpp)
target_link_libraries(test_direct_ring_buffer PRIVATE
    doctest::doctest
    snake_charmer
)
target_include_directories(test_direct_ring_buffer PUBLIC 
    ${DOCTEST_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
