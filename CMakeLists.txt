cmake_minimum_required(VERSION 3.16.3)
project(snake_charmer VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

find_package(doctest REQUIRED)
add_executable(factorial tests/factorial.cpp)
target_link_libraries(factorial PRIVATE doctest::doctest)
target_include_directories(factorial PUBLIC ${DOCTEST_INCLUDE_DIR})
