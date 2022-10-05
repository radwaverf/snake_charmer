add_executable(test_copy_ring_buffer copy_ring_buffer.cpp)
target_link_libraries(test_copy_ring_buffer PRIVATE
    doctest::doctest
    snake_charmer
)
target_include_directories(test_copy_ring_buffer PUBLIC 
    ${DOCTEST_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

add_executable(test_direct_ring_buffer direct_ring_buffer.cpp)
target_link_libraries(test_direct_ring_buffer PRIVATE
    doctest::doctest
    snake_charmer
)
target_include_directories(test_direct_ring_buffer PUBLIC 
    ${DOCTEST_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

