#include <spdlog/spdlog.h>
#include <snake_charmer/direct_ring_buffer.h>


namespace snake_charmer {

using BufferIndexIter = std::map<size_t, BufferIndexPtr>::iterator;

DirectRingBuffer::DirectRingBuffer(
        const size_t elem_size,
        const size_t max_elems_per_write,
        const size_t max_elems_per_read,
        const size_t slack
) :
        RingBuffer(elem_size, max_elems_per_write, max_elems_per_read, slack),
        next_id(0),
        min_write_index(0),
        max_write_index(0),
        min_read_index(0),
        max_read_index(0)
{
    indices.clear();
}


size_t DirectRingBuffer::add_writer() {
    std::lock_guard<std::mutex> lock(buf_mutex);
    BufferIndexPtr index = std::make_shared<BufferIndex>(
        next_id++, 0, 0, IndexFunction::Write, false
    );
    indices[index->id] = index;
    spdlog::info("Added writer {}. There are now {} indices. Next ID: {}", index->id, indices.size(), next_id);
    return index->id;
}

size_t DirectRingBuffer::add_reader() {
    std::lock_guard<std::mutex> lock(buf_mutex);
    BufferIndexPtr index = std::make_shared<BufferIndex>(
        next_id++, 0, 0, IndexFunction::Read, false
    );
    indices[index->id] = index;
    spdlog::info("Added reader {}. There are now {} indices. Next ID: {}", index->id, indices.size(), next_id);
    return index->id;
}

int DirectRingBuffer::grab_write(
        char*& elem_ptr,
        const size_t elems_this_write,
        const size_t id
        )
{
    if(elems_this_write > max_elems_per_write) {
        spdlog::error("requested too many elems this read: {} vs {}",
                elems_this_write, max_elems_per_write);
        return EMSGSIZE;
    }
    BufferIndexIter itr;
    {
        std::lock_guard<std::mutex> lock(buf_mutex);
        // verify that the requested writer exists
        itr = indices.find(id);
        if(itr == indices.end()) {
            spdlog::info("Unable to find writer {}. Existing IDs are...", id);
            for(itr = indices.begin(); itr != indices.end(); itr++) {
                spdlog::info("\t{}: {}", id, itr->second->function == IndexFunction::Read ? "Reader" : "Writer");
            }
            return ENXIO; // invalid ID
        }
    }
    BufferIndexPtr index = itr->second;
    if(index->function != IndexFunction::Write) {
        return EINVAL; // invalid function
    }
    if(index->in_use) {
        return EBUSY; // already in use, must be released before its grabbed again
    }

    {
        std::lock_guard<std::mutex> lock(buf_mutex);
        // verify that there are sufficient space in buffer for this write
        size_t buffer_space = this->get_buffer_size_elems() - (
            max_write_index - min_read_index
        );
        if(elems_this_write > buffer_space) {
            return ENOBUFS; // insufficient space
        }
        index->in_use = true;
        index->start = max_write_index;
        max_write_index += elems_this_write;
        index->end = max_write_index;
        spdlog::debug("Write grab elems {} to {} == byte offsets {} to {} == indices {} to {}",
                index->start, index->end,
                (index->start * elem_size) % buf_size, (index->end * elem_size) % buf_size,
                index->start * elem_size, index->end * elem_size
        );
        elem_ptr = buf_ptr + (index->start * elem_size) % buf_size;
        spdlog::debug("elem_ptr = {}, buf_ptr={}", (void*)(elem_ptr), (void*)(buf_ptr));
    }

    return 0;
}

int DirectRingBuffer::release_write(const size_t id) {
    BufferIndexIter itr;
    {
        std::lock_guard<std::mutex> lock(buf_mutex);
        // verify that the requested writer exists
        itr = indices.find(id);
        if(itr == indices.end()) {
            return ENXIO; // invalid ID
        }
    }
    BufferIndexPtr index = itr->second;
    if(index->function != IndexFunction::Write) {
        return EINVAL; // invalid function
    }
    if(not index->in_use) {
        return EBUSY; // not in use, must be grabbed before it's released
    }
    
    {
        std::lock_guard<std::mutex> lock(buf_mutex);
        // the end of this index is as upper bound of the min_write_index
        min_write_index = index->end; 
        index->in_use = false;
        for(itr = indices.begin(); itr != indices.end(); itr++) {
            if(itr->second->function == IndexFunction::Read) {
                continue;
            }
            if(itr->second->in_use) {
                min_write_index = std::min(min_write_index, itr->second->start);
            } else {
                min_write_index = std::min(min_write_index, itr->second->end);
            }
        }
        buf_cv.notify_all();
    }
    return 0;
}

int DirectRingBuffer::grab_read(
        char*& elem_ptr,
        const size_t elems_this_read,
        const size_t id,
        const std::chrono::microseconds& timeout
        )
{
    if(elems_this_read > max_elems_per_read) {
        spdlog::error("requested too many elems this read: {} vs {}",
                elems_this_read, max_elems_per_read);
        return EMSGSIZE;
    }
    BufferIndexIter itr;
    {
        std::lock_guard<std::mutex> lock(buf_mutex);
        // verify that the requested writer exists
        itr = indices.find(id);
        if(itr == indices.end()) {
            return ENXIO; // invalid ID
        }
    }
    BufferIndexPtr index = itr->second;
    if(index->function != IndexFunction::Read) {
        return EINVAL; // invalid function
    }
    if(index->in_use) {
        return EBUSY; // already in use, must be released before its grabbed again
    }

    {
        std::unique_lock<std::mutex> lock(buf_mutex);
        // verify that there are sufficient data in buffer for this read
        while(elems_this_read > min_write_index - max_read_index) {
            std::cv_status status = buf_cv.wait_for(lock, timeout);
            if (status == std::cv_status::timeout) {
                spdlog::debug("timeout");
                return ENOMSG;
            }
        }
        index->in_use = true;
        index->start = max_read_index;
        max_read_index += elems_this_read;
        index->end = max_read_index;
        spdlog::debug("Read grab elems {} to {} == byte offsets {} to {} == indices {} to {}",
                index->start, index->end,
                (index->start * elem_size) % buf_size, (index->end * elem_size) % buf_size,
                index->start * elem_size, index->end * elem_size
        );
        elem_ptr = buf_ptr + (index->start * elem_size) % buf_size;
        spdlog::debug("elem_ptr = {}, buf_ptr={}", (void*)(elem_ptr), (void*)(buf_ptr));
    }

    return 0;
}

int DirectRingBuffer::release_read(const size_t id) {
    BufferIndexIter itr;
    {
        std::lock_guard<std::mutex> lock(buf_mutex);
        // verify that the requested writer exists
        itr = indices.find(id);
        if(itr == indices.end()) {
            return ENXIO; // invalid ID
        }
    }
    BufferIndexPtr index = itr->second;
    if(index->function != IndexFunction::Read) {
        return EINVAL; // invalid function
    }
    if(not index->in_use) {
        return EBUSY; // not in use, must be grabbed before it's released
    }
    
    {
        std::lock_guard<std::mutex> lock(buf_mutex);
        // the end of this index is as upper bound of the min_read_index
        min_read_index = index->end; 
        index->in_use = false;
        for(itr = indices.begin(); itr != indices.end(); itr++) {
            if(itr->second->function == IndexFunction::Write) {
                continue;
            }
            if(itr->second->in_use) {
                min_read_index = std::min(min_read_index, itr->second->start);
            } else {
                min_read_index = std::min(min_read_index, itr->second->end);
            }
        }
    }
    return 0;
}

}
