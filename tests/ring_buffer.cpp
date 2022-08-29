#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <vector>
#include <spdlog/spdlog.h>
#include "ring_buffer.h"
#include <chrono>

TEST_CASE("testing the ring_buffer") {
    spdlog::set_level(spdlog::level::debug);
    // Each element is a vector of 1234 floating point values
    std::vector<float> elem(1234);
    // we want our ring_buffer to hold at least 5 elems
    size_t min_num_elems = 5;
    RingBuffer ring_buffer(elem.size() * sizeof(float), min_num_elems);
    // Assuming that the pagesize is 4096, check that the buffer size is that
    // big
    CHECK(ring_buffer.get_buffer_size_bytes() == 4096*7);
    CHECK(ring_buffer.get_buffer_size_elems() >= min_num_elems);

    // Check that when we directly modify the head/tail of the buffer, the
    // tail/head has the same change
    # if TESTING==1
    char* tail = ring_buffer._direct(ring_buffer.get_buffer_size_bytes());
    char* head = ring_buffer._direct(0);
    *head = 0;
    CHECK(*tail != 51);  // arbitrary number
    CHECK(*head == *tail);  // must be the same always
    *tail = 51;  // arbitrary number;
    CHECK(*tail == 51);
    CHECK(*head == *tail);
    # endif

    // Test that we can write to the buffer min_num_elems times without reads
    int rc=0;
    for (size_t n = 0; n < min_num_elems; n++) {
        std::fill(elem.begin(), elem.end(), static_cast<float>(n));
        rc = ring_buffer.write(reinterpret_cast<const char*>(elem.data()));
        CHECK(rc == 0);
    }
    // Test that we can't write to the buffer any more
    std::fill(elem.begin(), elem.end(), static_cast<float>(min_num_elems));
    rc = ring_buffer.write(reinterpret_cast<const char*>(elem.data()));
    CHECK(rc == ENOBUFS);

    // Test that we can read the values, and they match what we expect
    for (size_t n = 0; n < min_num_elems; n++) {
        rc = ring_buffer.read(reinterpret_cast<char*>(elem.data()), std::chrono::microseconds(1));
        CHECK(rc == 0);
        CHECK(elem[0] == n);
        CHECK(elem[1233] == n);
    }
}
