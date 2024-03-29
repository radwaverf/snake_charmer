#include <algorithm>
#include <assert.h>
#include <cstring>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <spdlog/spdlog.h>
#include <snake_charmer/ring_buffer.h>


namespace snake_charmer {

RingBuffer::RingBuffer(
        const size_t elem_size,
        const size_t max_elems_per_write,
        const size_t max_elems_per_read,
        const size_t slack,
        std::string loglevel
) :
        elem_size(elem_size),
        max_elems_per_write(max_elems_per_write),
        max_elems_per_read(max_elems_per_read),
        slack(slack)
{
    log_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    logger = std::make_shared<spdlog::logger>("RingBuffer", log_sink);
    logger->trace("using level {}", loglevel);
    if(loglevel.empty()) {
        logger->set_level(spdlog::level::from_str("error"));
    } else {
        logger->set_level(spdlog::level::from_str(loglevel));
    }
    logger->trace("using level {}", loglevel);

    const size_t min_buffer_size = (slack * max_elems_per_read + max_elems_per_write) * elem_size;
    logger->debug("Min buffer size: {}", min_buffer_size);
    const size_t pagesize_bytes = getpagesize();
    logger->debug("Page size: {}", pagesize_bytes);
    // the buffer_size must be a multiple of the page size
    buf_size = (
            (min_buffer_size / pagesize_bytes) + 1
    ) * pagesize_bytes;
    num_elems = buf_size / elem_size;
    logger->debug("Actual buffer size: {} bytes = {} elems", buf_size, num_elems);
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
size_t RingBuffer::get_elem_size() {
    return elem_size;
}
size_t RingBuffer::get_max_elems_per_write() {
    return max_elems_per_write;
}
size_t RingBuffer::get_max_elems_per_read() {
    return max_elems_per_read;
}

# if TESTING==1
char* RingBuffer::_direct(const size_t byte_offset) {
    std::unique_lock<std::mutex> lock(buf_mutex);
    return buf_ptr + byte_offset;
}
# endif

}
