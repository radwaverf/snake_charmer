

class RingBuffer {
    public:
        RingBuffer(const size_t elem_size, const size_t num_elems);
        ~RingBuffer();

    private:
        const size_t elem_size;
        size_t num_elems;
        void* buf_ptr;
        size_t buf_size;

};
