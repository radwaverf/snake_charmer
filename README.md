# Snake Charmer: C++ Ring Buffers

The purpose of this repo is just to provide generic ring buffers that can be
used to handle streaming data. It doesn't do any streaming instance. You get
to determine how you want to schedule events.

## Internals

### `RingBuffer`

This is a simple ring buffer that manages the buffer internals. This introduces
the concept of "slack", which is the amount of extra buffer available so that
host-level interrupts have a chance of being gracefully handled. It's the
base class for all other ring buffers.

### `CopyRingBuffer`

This ring buffer does read/write operations with `memcpy`'s.

### `DirectRingBuffer`

This ring buffer does read/write operations with more granular grab/release
calls, enabling the external code to directly access the buffer.

## Dependencies

doctest-dev
libspdlog-dev

