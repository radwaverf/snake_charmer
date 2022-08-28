#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <vector>

#include "ring_buffer.h"

TEST_CASE("testing the ring_buffer") {
    // Each element is a vector of 1234 floating point values
    std::vector<float> elem(1234);
    for(size_t n=0; n<elem.size(); n++) {
        elem[n] = n;
    }
    // we want our ring_buffer to hold at least 5 elems
    size_t min_num_elems = 5;
    RingBuffer ring_buffer = RingBuffer(elem.size() * sizeof(float), min_num_elems);
    // Assuming that the pagesize is 4096, check that the buffer size is that
    // big
    CHECK(ring_buffer.get_buffer_size_bytes() == 4096*7);
    CHECK(ring_buffer.get_buffer_size_elems() >= min_num_elems);
}
