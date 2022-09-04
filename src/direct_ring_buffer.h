#include "ring_buffer.h"
#include <condition_variable>
#include <vector>
#include <map>
#include <memory>


enum IndexFunction {
    Write = 0,
    Read = 1
};

/**
 * Defines read/write indices for use within a Buffer
 * .first is the first item being accessed
 * .second is 1 more than the last item being accessed
 */
struct BufferIndex {
    BufferIndex(
        const size_t id,
        const size_t start,
        const size_t end,
        const IndexFunction function,
        const bool in_use
    ) : 
        id(id), start(start), end(end), function(function), in_use(in_use)
    {};
    size_t id;
    size_t start;
    size_t end;
    IndexFunction function;
    bool in_use;
};

using BufferIndexPtr = std::shared_ptr<BufferIndex>;
using BufferIndices = std::map<size_t, BufferIndexPtr>;


/**
 * 
 */
class DirectRingBuffer : public RingBuffer {
    public:
        DirectRingBuffer(
                const size_t elem_size,
                const size_t max_elems_per_write,
                const size_t max_elems_per_read,
                const size_t slack
        );

        /**
         * Add a writer
         *
         * Returns a BufferIndex ID which is to be used in subsequent grab/release
         * calls
         */
        size_t add_writer();

        /**
         * Add a reader
         *
         * Returns a BufferIndex ID which is to be used in subsequent grab/release
         * calls
         */
        size_t add_reader();

        /**
         * Grab a portion of the buffer for writing
         *
         * elem_ptr pointer in buffer which you can then edit
         * elems_this_write number of elements you are responsible for writing
         * id BufferIndex ID that must be provided to subsequent release call
         *
         * Returns 0 if successful.
         * Returns ENOBUFS if buffer full
         */
        int grab_write(
            char*& elem_ptr,
            const size_t elems_this_write,
            const size_t id
        );

        /**
         * Release a portion of the buffer for writing
         *
         * id BufferIndex ID corresponding to prior grab call
         *
         * Returns 0 if successful.
         * Returns ENXIO if BufferIndex provided isn't valid
         */
        int release_write(
            const size_t id
        );

        /**
         * Grab a portion of the buffer for reading
         *
         * elem_ptr pointer in buffer which you can then read
         * elems_this_read number of elements you are responsible for reading
         * id BufferIndex ID that must be provided to subsequent release call
         * timeout number of microseconds to wait for data
         *
         * Returns 0 if successful.
         * Returns ENOBUFS if buffer full
         */
        int grab_read(
            char*& elem_ptr,
            const size_t elems_this_write,
            const size_t id,
            const std::chrono::microseconds& timeout
        );

        /**
         * Release a portion of the buffer for reading
         *
         * id BufferIndex ID corresponding to prior grab call
         *
         * Returns 0 if successful.
         * Returns ENXIO if BufferIndex provided isn't valid
         */
        int release_read(
            const size_t id
        );

    private:

        // thread safety
        std::condition_variable buf_cv;
        // To support concurrent writers/readers that use grab/release
        // to directly access the buffer, we provide a list of writers/readers
        // define what portions of the buffer are currently in use.
        BufferIndices indices;
        size_t next_id;
        
        // To prevent the readers and writers from conflicting, we need to
        // track the min and max indices of the readers and writers
        size_t min_write_index;
        size_t max_write_index;
        size_t min_read_index;
        size_t max_read_index;
        
};
