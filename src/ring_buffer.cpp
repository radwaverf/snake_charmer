#include <algorithm>
#include <assert.h>
#include <cstring>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <spdlog/spdlog.h>
#include "ring_buffer.h"

RingBuffer::RingBuffer(
        const size_t elem_size,
        const size_t max_elems_per_write,
        const size_t max_elems_per_read,
        const size_t slack
) :
        elem_size(elem_size),
        max_elems_per_write(max_elems_per_write),
        max_elems_per_read(max_elems_per_read),
        slack(slack),
        write_index(0),
        read_index(0)
{
    const size_t min_buffer_size = (slack * max_elems_per_read + max_elems_per_write) * elem_size;
    spdlog::debug("Min buffer size: {}", min_buffer_size);
    const size_t pagesize_bytes = getpagesize();
    spdlog::debug("Page size: {}", pagesize_bytes);
    // the buffer_size must be a multiple of the page size
    buf_size = (
            (min_buffer_size / pagesize_bytes) + 1
    ) * pagesize_bytes;
    num_elems = buf_size / elem_size;
    spdlog::debug("Actual buffer size: {} bytes = {} elems", buf_size, num_elems);
    buf_overlap = (
            std::max(max_elems_per_read, max_elems_per_write)
            * elem_size / pagesize_bytes + 1
    ) * pagesize_bytes;

    // get virtual address space of (size = buf_size + buf_overlap) for our buffer
    buf_ptr = static_cast<char*>(mmap(NULL, buf_size + buf_overlap, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    // get a temporary file fd (physical store)
    const auto fd = fileno(tmpfile ());
    // set it's size appropriately. We need exactly `sz` bytes as underlying memory
    ftruncate(fd, buf_size);

    // now map first half of our buffer to underlying buffer
    mmap(buf_ptr, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);

    // similarly map overlap of our buffer
    mmap(buf_ptr + buf_size, buf_overlap, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
}

RingBuffer::~RingBuffer() {
    munmap(buf_ptr, buf_size + buf_overlap);
}

size_t RingBuffer::get_buffer_size_elems() {
    return num_elems;
}
size_t RingBuffer::get_buffer_size_bytes() {
    return buf_size;
}

int RingBuffer::write(const char* elem_ptr, const size_t elems_this_write) {
    if(elems_this_write > max_elems_per_write) {
        spdlog::error("requested too many elems this write: {} vs {}",
                elems_this_write, max_elems_per_write);
        return EMSGSIZE;
    }
    std::unique_lock<std::mutex> lock(buf_mutex);
    if(write_index + elems_this_write - read_index > num_elems) {
        spdlog::warn("insufficient slack");
        return ENOBUFS;
    }
    spdlog::debug("writing elems {} to {} == byte offsets {} to {} == indices {} to {}",
            write_index,
            write_index+elems_this_write,
            write_index*elem_size % buf_size,
            (write_index+elems_this_write)*elem_size % buf_size,
            write_index*elem_size,
            (write_index+elems_this_write)*elem_size
    );
    memcpy(
        buf_ptr + (write_index*elem_size) % buf_size,
        elem_ptr,
        elem_size * elems_this_write
    );
    write_index += elems_this_write;
    buf_cv.notify_one();
    return 0;
}

int RingBuffer::read(
        char* elem_ptr,
        const size_t elems_this_read,
        const std::chrono::microseconds& timeout,
        const int64_t advance_size
    ) {
    if(elems_this_read> max_elems_per_read) {
        spdlog::error("requested too many elems this read: {} vs {}",
                elems_this_read, max_elems_per_read);
        return EMSGSIZE;
    }
    std::unique_lock<std::mutex> lock(buf_mutex);
    while(read_index + elems_this_read > write_index) {
        std::cv_status status = buf_cv.wait_for(lock, timeout);
        if (status == std::cv_status::timeout) {
            spdlog::debug("timeout");
            return ENOMSG;
        }
    }
    spdlog::debug("reading elems {} to {} == byte offsets {} to {} == indices {} to {}",
            read_index,
            read_index+elems_this_read,
            read_index*elem_size % buf_size,
            (read_index+elems_this_read)*elem_size % buf_size,
            read_index*elem_size,
            (read_index+elems_this_read)*elem_size
    );
    memcpy(
        elem_ptr,
        buf_ptr + (read_index*elem_size) % buf_size,
        elem_size * elems_this_read
    );
    if(advance_size < 0) {
        read_index += elems_this_read;
    } else {
        read_index += advance_size;
    }
    return 0;
}
# if TESTING==1
char* RingBuffer::_direct(const size_t byte_offset) {
    std::unique_lock<std::mutex> lock(buf_mutex);
    return buf_ptr + byte_offset;
}
# endif
