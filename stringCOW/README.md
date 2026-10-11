# stringCOW

A C++20 Copy-On-Write (COW) string with Small String Optimization (SSO).

The string has a 32-byte layout. Strings up to 30 characters fit in the inline buffer without heap allocations. Beyond 30 characters, the string allocates a single contiguous block containing the refcounted control block and the payload.

Copying a heap string increments the reference count rather than cloning the buffer. Non-const access via operator[] or data() detaches the buffer into an unshared copy. Equality comparisons short-circuit in O(1) if two strings share the same control block pointer.

Thread safety is configurable at compile time: `moo::string` uses standard uint32_t counters with no atomic overhead, while `moo::ts_string` uses atomic operations with acquire-release ordering.

## Build

Requires CMake 3.23+ and a C++20 compiler.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run tests:
```bash
./build/test_cow
```

Run benchmarks:
```bash
./build/bench_cow
```

Detailed benchmarks and comparisons against std::string are in [benchmarks.md](benchmarks.md).
