#include <condition_variable>
#include <mutex>

/**
 * A single-writer, single-reader ring buffer.
 *
 * Contains at least num_elems elements of size elem_size bytes.
 * Writer will always write `elem_size` bytes at a time.
 * Reader will always read `elem_size` bytes at a time.
 */

# define TESTING 1
class RingBuffer {
    public:
        RingBuffer(const size_t elem_size, const size_t num_elems);
        ~RingBuffer();
        
        /**
         * Get the buffer size in units of elem_size
         */
        size_t get_buffer_size_elems();
        /**
         * Get the buffer size in units of bytes
         */
        size_t get_buffer_size_bytes();

        /**
         * Write elem_size bytes to the buffer.
         *
         * Returns 0 if successful.
         * Returns ENOBUFS if buffer full
         */
        int write(const char* elem);

        /**
         * Read elem_size bytes from the buffer.
         * Waits a maximum duration of rel_time for data to arrive.
         *
         * Returns 0 if successful.
         * Returns ENOMSG if buffer empty
         */
        int read(char* elem, const std::chrono::microseconds& rel_time);

        # if TESTING==1
        char* _direct(const size_t byte_offset);
        # endif

    private:
        const size_t elem_size;
        size_t num_elems;
        char* buf_ptr;
        size_t buf_size;
        
        // thread safety
        std::mutex buf_mutex;
        std::condition_variable buf_cv;

        // writer/reader indices
        size_t write_index;
        size_t read_index;
};
