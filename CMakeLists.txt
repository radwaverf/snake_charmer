cmake_minimum_required(VERSION 3.16.3)
project(snake_charmer VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

# Dependencies
find_package(doctest)
find_package(spdlog REQUIRED)

# Library
include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB HEADER_FILES include/snake_charmer/*.h)
file(GLOB SRC_FILES src/*.cpp)

add_library(snake_charmer "${SRC_FILES}")
set_target_properties(snake_charmer PROPERTIES PUBLIC_HEADER "${HEADER_FILES}")
install(TARGETS snake_charmer
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/snake_charmer
)

# Tests
if(doctest_FOUND)
    add_subdirectory(tests)
endif(doctest_FOUND)
