#include <stdlib.h>
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
    buf_ptr = aligned_alloc(pagesize_bytes, buf_size);
}

RingBuffer::~RingBuffer() {
    free(buf_ptr);
}
