#include <assert.h>
#include <cstring>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <spdlog/spdlog.h>
#include "ring_buffer.h"

RingBuffer::RingBuffer(const size_t elem_size, const size_t num_elems) :
        elem_size(elem_size),
        write_index(0),
        read_index(0)
{
    spdlog::debug("Element size: {}", elem_size);
    spdlog::debug("Num elements: {}", num_elems);
    const size_t min_buffer_size = elem_size * num_elems;
    spdlog::debug("Min buffer size: {}", min_buffer_size);
    const size_t pagesize_bytes = getpagesize();
    spdlog::debug("Page size: {}", pagesize_bytes);
    // the buffer_size must be a multiple of the page size
    buf_size = (
            (min_buffer_size / pagesize_bytes) + 1
    ) * pagesize_bytes;
    this->num_elems = buf_size / elem_size;

    // get virtual address space of (size = 2 * buf_size) for our buffer
    buf_ptr = static_cast<char*>(mmap(NULL, 2 * buf_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    // get a temporary file fd (physical store)
    const auto fd = fileno(tmpfile ());
    // set it's size appropriately. We need exactly `sz` bytes as underlying memory
    ftruncate(fd, buf_size);

    // now map first half of our buffer to underlying buffer
    mmap(buf_ptr, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);

    // similarly map second half of our buffer
    mmap(buf_ptr + buf_size, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
}

RingBuffer::~RingBuffer() {
    munmap(buf_ptr, buf_size * 2);
}

size_t RingBuffer::get_buffer_size_elems() {
    return num_elems;
}
size_t RingBuffer::get_buffer_size_bytes() {
    return buf_size;
}

int RingBuffer::write(const char* elem) {
    std::unique_lock<std::mutex> lock(buf_mutex);
    if(write_index - read_index >= num_elems) {
        return ENOBUFS;
    }
    memcpy(
        buf_ptr + (write_index*elem_size) % buf_size,
        elem,
        elem_size
    );
    write_index++;
    buf_cv.notify_one();
    return 0;
}

int RingBuffer::read(char* elem, const std::chrono::microseconds& rel_time) {
    std::unique_lock<std::mutex> lock(buf_mutex);
    while(read_index == write_index) {
        std::cv_status status = buf_cv.wait_for(lock, rel_time);
        if (status == std::cv_status::timeout) {
            return ENOMSG;
        }
    }
    memcpy(
        elem,
        buf_ptr + (read_index*elem_size) % buf_size,
        elem_size
    );
    read_index++;
    return 0;
}
# if TESTING==1
char* RingBuffer::_direct(const size_t byte_offset) {
    std::unique_lock<std::mutex> lock(buf_mutex);
    return buf_ptr + byte_offset;
}
# endif
