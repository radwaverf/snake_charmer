#include <condition_variable>
#include "ring_buffer.h"

namespace snake_charmer {

class CopyRingBuffer : public RingBuffer {
    public:
        const static std::chrono::microseconds DEFAULT_TIMEOUT;
        CopyRingBuffer(
                const size_t elem_size,
                const size_t max_elems_per_write,
                const size_t max_elems_per_read,
                const size_t slack,
                std::string loglevel
        );
        /**
         * Write elem_size bytes to the buffer via memcpy
         *
         * elem_ptr pointer from where elements will be copied
         * elems_this_write number of elements to write
         *
         * Returns 0 if successful.
         * Returns ENOBUFS if buffer full
         */
        int write(
            const char* elem_ptr,
            const size_t elems_this_write,
            const std::chrono::microseconds& timeout = DEFAULT_TIMEOUT
        );

        /**
         * Read elem_size bytes from the buffer via memcpy
         * Waits a maximum duration of timeout for data to arrive.
         *
         * elem_ptr pointer to where elements will be copied
         * elems_this_read number of elements to read
         * timeout number of microseconds to wait for data
         * advance_size if >= 0, the number of elems to advance the read pointer
         *
         * Returns 0 if successful.
         * Returns ENOMSG if buffer empty
         */
        int read(
            char* elem_ptr,
            const size_t elems_this_read,
            const std::chrono::microseconds& timeout = DEFAULT_TIMEOUT,
            const int64_t advance_size = -1
        );

    private:
        // thread safety
        std::condition_variable buf_cv;
        
        // writer/reader indices
        size_t write_index;
        size_t read_index;
};

}; // namespace snake_charmer
