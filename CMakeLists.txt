cmake_minimum_required(VERSION 3.16.3)
project(snake_charmer VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

# Dependencies
find_package(doctest)
find_package(spdlog REQUIRED)

# Library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB HEADER_FILES include/${PROJECT_NAME}/*.h)
file(GLOB SRC_FILES src/*.cpp)

add_library(${PROJECT_NAME} "${SRC_FILES}")
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${HEADER_FILES}")
target_link_libraries(${PROJECT_NAME}
    spdlog::spdlog
)
if (WIN32)
    # onecore provides VirtualAlloc
    target_link_libraries(${PROJECT_NAME} onecore)
endif (WIN32)
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
)

install(FILES ${PROJECT_NAME}Config.cmake DESTINATION lib/cmake/${PROJECT_NAME})

# Tests
if(doctest_FOUND)
    add_subdirectory(tests)
endif(doctest_FOUND)
