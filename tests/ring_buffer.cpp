#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <vector>
#include <spdlog/spdlog.h>
#include "ring_buffer.h"
#include <chrono>

TEST_CASE("testing the ring_buffer") {
    spdlog::set_level(spdlog::level::debug);
    // Each element is a vector of 1234 floating point values
    size_t elem_size=1234;
    std::vector<float> elem(elem_size);
    // We want to write/read up to 3 elems at a time, with a slack of 2
    size_t max_elems_per_write = 3;
    size_t max_elems_per_read = 3;
    size_t slack = 2;
    RingBuffer ring_buffer(
        elem.size() * sizeof(float),
        max_elems_per_write,
        max_elems_per_read,
        slack
    );
    // Assuming that the pagesize is 4096, check that the buffer size is that
    // big
    CHECK(ring_buffer.get_buffer_size_bytes() >= elem_size*sizeof(float)*9);
    CHECK(ring_buffer.get_buffer_size_elems() == 9);
    CHECK(ring_buffer.get_buffer_size_elems() >= max_elems_per_write);
    CHECK(ring_buffer.get_buffer_size_elems() >= max_elems_per_read);
    CHECK(ring_buffer.get_buffer_size_elems() >= slack);

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
    for (size_t n = 0; n < ring_buffer.get_buffer_size_elems(); n++) {
        std::fill(elem.begin(), elem.end(), static_cast<float>(n));
        rc = ring_buffer.write(
            reinterpret_cast<const char*>(elem.data()),
            1
        );
        CHECK(rc == 0);
    }
    // Test that we can't write to the buffer any more
    std::fill(elem.begin(), elem.end(), 999.);
    rc = ring_buffer.write(reinterpret_cast<const char*>(elem.data()), 1);
    CHECK(rc == ENOBUFS);

    // Test that we can read the values, and they match what we expect
    for (size_t n = 0; n < ring_buffer.get_buffer_size_elems(); n++) {
        rc = ring_buffer.read(
            reinterpret_cast<char*>(elem.data()),
            1,
            std::chrono::microseconds(1)
        );
        CHECK(rc == 0);
        CHECK(elem[0] == n);
        CHECK(elem[1233] == n);
    }

    // Test that we can write/read when straddling the end of the buffer
    //   first, nearly fill the buffer
    for (size_t n = 0; n < ring_buffer.get_buffer_size_elems() - 1; n++) {
        std::fill(elem.begin(), elem.end(), static_cast<float>(n));
        rc = ring_buffer.write(
            reinterpret_cast<const char*>(elem.data()),
            1
        );
        CHECK(rc == 0);
    }
    //   next, read enough so that we can do a max-sized write
    elem.resize(elem_size*max_elems_per_write);
    rc = ring_buffer.read(
        reinterpret_cast<char*>(elem.data()),
        max_elems_per_write - 1,
        std::chrono::microseconds(1)
    );
    CHECK(rc == 0);
    //   next, prep a buffer to write when straddled
    for (int64_t n = 0; n < max_elems_per_write; n++) {
        std::fill(
            elem.begin() + elem_size*n,
            elem.begin() + elem_size*(n+1),
            static_cast<float>(-n)
        );
    }
    //   do the write
    rc = ring_buffer.write(
        reinterpret_cast<const char*>(elem.data()),
        max_elems_per_write
    );
    CHECK(rc == 0);
    //   read to up prior write index
    for (int64_t n = 0; n<ring_buffer.get_buffer_size_elems() - max_elems_per_write; n++) {
        rc = ring_buffer.read(
            reinterpret_cast<char*>(elem.data()),
            1,
            std::chrono::microseconds(1)
        );
        CHECK(rc == 0);
    }
    //   read back over the straddle
    rc = ring_buffer.read(
        reinterpret_cast<char*>(elem.data()),
        max_elems_per_write,
        std::chrono::microseconds(1)
    );
    CHECK(rc == 0);
    for (int64_t n = 0; n < max_elems_per_write; n++) {
        CHECK(elem[n*elem_size] == -n);
        CHECK(elem[(n+1)*elem_size-1] == -n);
    }

}
