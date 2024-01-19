#pragma once

#include <mutex>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>


namespace snake_charmer {

/**
 * Generic ring buffer.
 *
 * RingBuffer uses a concept similar to the one laid out in
 * https://abhinavag.medium.com/a-fast-circular-ring-buffer-4d102ef4d4a3
 * but shrinks the size of the overlap of the buffer to reduce the memory
 * burden.
 *
 * It is designed to allow writers to write up to max_elems_per_write to
 * the buffer per write() call, and allow readers to read up to
 * max_elems_per_read to the buffer per read() call. 
 *
 * In order to assist with handling irregular thread scheduling, slack is
 * introduced. Essentially, there should be sufficient slack in the buffer that
 * the writers are not blocked due to slow readers. Slack is defined here to be
 *
 * (buf_size/elem_size - max_elems_per_write)/max_elems_per_read
 *
 * That slack should be greater than the instantaneous
 * (max_elems_per_write * max_writes_per_sec) / (min_elems_per_read * min_reads_per_sec)
 * This can be thought of as the slack should be sufficient so that when the
 * writers are writting maximally fast, and the readers are reading minimally
 * fast, the writers don't loop over the readers. There must be some amount
 * of load balancing in the system so that this "instantaneous" condition
 * doesn't go on for too long.
 */

# define TESTING 1
class RingBuffer {
    public:
        /**
         * Constructor.
         *
         * The size of the buffer will be
         * (slack * max_elems_per_read + max_elems_per_write) * elem_size
         *
         * This is because the "slack" in the buffer is
         *
         * (buf_size/elem_size - max_elems_per_write)/max_elems_per_read
         *
         * @param elem_size the size of elements in bytes
         * @param max_elems_per_write the maximum number of elements written per write
         * @param max_elems_per_read the maximum number of elements read per read
         * @param slack amount of "slack" in the buffer
         *
         */
        RingBuffer(
                const size_t elem_size,
                const size_t max_elems_per_write,
                const size_t max_elems_per_read,
                const size_t slack,
                const std::string loglevel
        );
        ~RingBuffer();
        
        /**
         * Get the buffer size in units of elem_size
         */
        size_t get_buffer_size_elems();
        /**
         * Get the buffer size in units of bytes
         */
        size_t get_buffer_size_bytes();

        # if TESTING==1
        char* _direct(const size_t byte_offset);
        # endif

        /**
         * Get the size of elements in bytes
         */
        size_t get_elem_size();

        /**
         * Get the max number of elements per write
        */
        size_t get_max_elems_per_write();

        /**
         * Get the max number of elements per read
        */
        size_t get_max_elems_per_read();

    protected:
        const size_t elem_size;
        const size_t max_elems_per_write;
        const size_t max_elems_per_read;
        const size_t slack;
        
        size_t num_elems;
        char* buf_ptr;
        size_t buf_size;
        size_t buf_overlap;
        
        std::mutex buf_mutex;
        std::shared_ptr<spdlog::logger> logger;
        std::shared_ptr<spdlog::sinks::stdout_sink_mt> log_sink;
};

}; // snake_charmer
