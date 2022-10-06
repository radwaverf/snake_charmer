#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <vector>
#include <spdlog/spdlog.h>
#include <snake_charmer/direct_ring_buffer.h>
#include <chrono>
#include <string.h>

using namespace snake_charmer;

TEST_CASE("testing the direct_ring_buffer") {
    spdlog::set_level(spdlog::level::debug);
    // Each element is a vector of 1234 floating point values
    size_t elem_size=1234;
    std::vector<float> elem(elem_size);
    // We want to write/read up to 3 elems at a time, with a slack of 2
    size_t max_elems_per_write = 3;
    size_t max_elems_per_read = 3;
    size_t slack = 2;
    DirectRingBuffer ring_buffer(
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

    char* buf_ptr;
    // test that grab without add results in ENXIO
    CHECK(ring_buffer.grab_write(buf_ptr, 1, 0) == ENXIO);
    

    spdlog::info("Error Codes");
    spdlog::info("\t{}: {}", ENXIO, strerror(ENXIO));
    spdlog::info("\t{}: {}", EBUSY, strerror(EBUSY));
    spdlog::info("\t{}: {}", EINVAL, strerror(EINVAL));
    spdlog::info("\t{}: {}", ENOBUFS, strerror(ENOBUFS));

    // Test that we can write to the buffer min_num_elems times without reads
    size_t writer_0 = ring_buffer.add_writer();
    spdlog::info("writer id: {}", writer_0);
    int rc=0;
    for (size_t n = 0; n < ring_buffer.get_buffer_size_elems(); n++) {
        std::fill(elem.begin(), elem.end(), static_cast<float>(n));
        rc = ring_buffer.grab_write(
            buf_ptr,
            1,
            writer_0
        );
        CHECK(rc == 0);
        memcpy(buf_ptr, elem.data(), elem.size() * sizeof(float));
        CHECK(reinterpret_cast<float*>(buf_ptr)[0] == n);
        CHECK(reinterpret_cast<float*>(buf_ptr)[elem.size()-1] == n);
        rc = ring_buffer.release_write(
            writer_0
        );
        CHECK(rc == 0);
    }
    // Test that we can't write to the buffer any more
    rc = ring_buffer.grab_write(buf_ptr, 1, writer_0);
    CHECK(rc == ENOBUFS);
    
    // Test that we can read the values, and they match what we expect
    size_t reader_0 = ring_buffer.add_reader();
    float* elem_ptr;
    for (size_t n = 0; n < ring_buffer.get_buffer_size_elems(); n++) {
        rc = ring_buffer.grab_read(
            buf_ptr,
            1,
            reader_0,
            std::chrono::microseconds(1)
        );
        CHECK(rc == 0);
        elem_ptr = reinterpret_cast<float*>(buf_ptr);
        CHECK(elem_ptr[0] == n);
        CHECK(elem_ptr[1233] == n);
        rc = ring_buffer.release_read(
            reader_0
        );
        CHECK(rc == 0);
    }







    // Test that we can write/read when straddling the end of the buffer
    //   first, nearly fill the buffer
    for (size_t n = 0; n < ring_buffer.get_buffer_size_elems() - 1; n++) {
        std::fill(elem.begin(), elem.end(), static_cast<float>(n));
        rc = ring_buffer.grab_write(
            buf_ptr,
            1,
            writer_0
        );
        CHECK(rc == 0);
        memcpy(buf_ptr, elem.data(), elem.size() * sizeof(float));
        rc = ring_buffer.release_write(
            writer_0
        );
    }
    //   next, read enough so that we can do a max-sized write
    rc = ring_buffer.grab_read(
        buf_ptr,
        max_elems_per_write - 1,
        reader_0,
        std::chrono::microseconds(1)
    );
    CHECK(rc == 0);
    rc = ring_buffer.release_read(
        reader_0
    );
    CHECK(rc == 0);
    //   next, prep a buffer to write when straddled
    elem.resize(elem_size*max_elems_per_write);
    for (int64_t n = 0; n < max_elems_per_write; n++) {
        std::fill(
            elem.begin() + elem_size*n,
            elem.begin() + elem_size*(n+1),
            static_cast<float>(-n)
        );
    }
    //   do the write
    rc = ring_buffer.grab_write(
        buf_ptr,
        max_elems_per_write,
        writer_0
    );
    CHECK(rc == 0);
    memcpy(buf_ptr, elem.data(), elem.size() * sizeof(float));
    rc = ring_buffer.release_write(
        writer_0
    );
    CHECK(rc == 0);
    //   read to up prior write index
    for (int64_t n = 0; n<ring_buffer.get_buffer_size_elems() - max_elems_per_write; n++) {
        rc = ring_buffer.grab_read(
            buf_ptr,
            1,
            reader_0,
            std::chrono::microseconds(1)
        );
        CHECK(rc == 0);
        rc = ring_buffer.release_read(
            reader_0
        );
        CHECK(rc == 0);
    }
    //   read back over the straddle
    rc = ring_buffer.grab_read(
        buf_ptr,
        max_elems_per_write,
        reader_0,
        std::chrono::microseconds(1)
    );
    CHECK(rc == 0);
    rc = ring_buffer.release_read(
        reader_0
    );
    CHECK(rc == 0);
    elem_ptr = reinterpret_cast<float*>(buf_ptr);
    for (int64_t n = 0; n < max_elems_per_write; n++) {
        CHECK(elem_ptr[n*elem_size] == -n);
        CHECK(elem_ptr[(n+1)*elem_size-1] == -n);
    }
}

