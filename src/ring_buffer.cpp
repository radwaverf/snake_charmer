#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ring_buffer.h"

RingBuffer::RingBuffer(const size_t elem_size, const size_t num_elems) :
        elem_size(elem_size)
{
    const size_t pagesize_bytes = getpagesize();
    const size_t min_buffer_size = elem_size * num_elems;
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
