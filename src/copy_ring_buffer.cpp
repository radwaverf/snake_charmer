#include <spdlog/spdlog.h>
#include <snake_charmer/copy_ring_buffer.h>

namespace snake_charmer {

CopyRingBuffer::CopyRingBuffer(
        const size_t elem_size,
        const size_t max_elems_per_write,
        const size_t max_elems_per_read,
        const size_t slack,
        const std::string& loglevel
) :
        RingBuffer(elem_size, max_elems_per_write, max_elems_per_read, slack, loglevel),
        write_index(0),
        read_index(0) {
}

int CopyRingBuffer::write(const char* elem_ptr, const size_t elems_this_write) {
    if(elems_this_write > max_elems_per_write) {
        logger.error("requested too many elems this write: {} vs {}",
                elems_this_write, max_elems_per_write);
        return EMSGSIZE;
    }
    std::unique_lock<std::mutex> lock(buf_mutex);
    if(write_index + elems_this_write - read_index > num_elems) {
        logger.warn("insufficient slack");
        return ENOBUFS;
    }
    logger.debug("writing elems {} to {} == byte offsets {} to {} == indices {} to {}",
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

int CopyRingBuffer::read(
        char* elem_ptr,
        const size_t elems_this_read,
        const std::chrono::microseconds& timeout,
        const int64_t advance_size
    ) {
    if(elems_this_read> max_elems_per_read) {
        logger.error("requested too many elems this read: {} vs {}",
                elems_this_read, max_elems_per_read);
        return EMSGSIZE;
    }
    std::unique_lock<std::mutex> lock(buf_mutex);
    while(read_index + elems_this_read > write_index) {
        std::cv_status status = buf_cv.wait_for(lock, timeout);
        if (status == std::cv_status::timeout) {
            logger.debug("timeout");
            return ENOMSG;
        }
    }
    logger.debug("reading elems {} to {} == byte offsets {} to {} == indices {} to {}",
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

}
