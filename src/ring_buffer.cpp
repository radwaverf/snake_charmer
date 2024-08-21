#include <algorithm>
#include <assert.h>
#include <cstring>
#include <stdio.h>
#ifdef _WIN32
  #include <windows.h>
  #undef max
#elif __unix__
  #include <sys/mman.h>
  #include <unistd.h>
#else
  #error "Only windows and linux supported"
#endif

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
        slack(slack),
        buf_ptr(nullptr)
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
#ifdef _WIN32
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    logger->debug("dwPageSize: {} vs dwAllocationGranularity: {}",
        sys_info.dwPageSize, sys_info.dwAllocationGranularity);
    const size_t pagesize_bytes = std::max(sys_info.dwPageSize, sys_info.dwAllocationGranularity);
#elif __unix__
    const size_t pagesize_bytes = getpagesize();
#endif
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

#ifdef _WIN32
    // following https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2
    // and https://stackoverflow.com/q/39456956
    
    // // get virtual address space of (size = buf_size + buf_overlap) for our buffer
    
    void* placeholder1 = nullptr;
    placeholder1 = (PCHAR)VirtualAlloc2(
        nullptr,
        nullptr,
        2*buf_size,
        MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
        PAGE_NOACCESS,
        nullptr, 0
    );

    if (placeholder1 == nullptr) {
        throw std::runtime_error(fmt::format("VirtualAlloc2 failed, error {}", GetLastError()));
    }
    
    // Split the placeholder region into two regions of equal size.
    bool result = VirtualFree(
        placeholder1,
        buf_size,
        MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER
    );

    if (result == FALSE) {
        throw std::runtime_error(fmt::format("VirtualFreeEx failed, error {}", GetLastError()));
    }
    void* placeholder2 = nullptr;
    placeholder2 = (void*)((ULONG_PTR)placeholder1 + buf_size);
    // Create a pagefile-backed section for the buffer.
    HANDLE section = nullptr;
    section = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        buf_size, nullptr
    );

    if (section == nullptr) {
        throw std::runtime_error(fmt::format("CreateFileMapping failed, error {}", GetLastError()));
    }
    // Map the section into the first placeholder region.
    void* view1 = nullptr;
    view1 = MapViewOfFile3(
        section,
        nullptr,
        placeholder1,
        0,
        buf_size,
        MEM_REPLACE_PLACEHOLDER,
        PAGE_READWRITE,
        nullptr, 0
    );

    if (view1 == nullptr) {
        throw std::runtime_error(fmt::format("MapViewOfFile3 failed, error {}", GetLastError()));
    }

    // Ownership transferred, don't free this now.
    placeholder1 = nullptr;

    // Map the section into the second placeholder region.
    void* view2 = nullptr;
    view2 = MapViewOfFile3(
        section,
        nullptr,
        placeholder2,
        0,
        buf_size,
        MEM_REPLACE_PLACEHOLDER,
        PAGE_READWRITE,
        nullptr, 0
    );

    if (view2 == nullptr) {
        throw std::runtime_error(fmt::format("MapViewOfFile3 failed, error {}", GetLastError()));
    }

    // Success, return both mapped views to the caller.
    buf_ptr = static_cast<char*>(view1);
    secondary_view = view2;

    placeholder2 = nullptr;
    view1 = nullptr;
    view2 = nullptr;
    if (section != nullptr) {
        CloseHandle(section);
    }
    if (placeholder1 != nullptr) {
        VirtualFree(placeholder1, 0, MEM_RELEASE);
    }
    if (placeholder2 != nullptr) {
        VirtualFree(placeholder2, 0, MEM_RELEASE);
    }
    if (view1 != nullptr) {
        UnmapViewOfFileEx(view1, 0);
    }
    if (view2 != nullptr) {
        UnmapViewOfFileEx(view2, 0);
    }
    
#elif __unix__
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
#endif
}

RingBuffer::~RingBuffer() {
#ifdef _WIN32
    UnmapViewOfFile(buf_ptr);
    UnmapViewOfFile(secondary_view);
#elif __unix__
    munmap(buf_ptr, buf_size + buf_overlap);
#endif
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
