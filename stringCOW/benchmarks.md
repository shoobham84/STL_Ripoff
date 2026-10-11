# Benchmarks

Benchmarks were run using Google Benchmark on an AMD Ryzen 5 5600H with `-O2 -DNDEBUG -march=native`.

## Copy Construction

Copying `moo::string` is O(1) refcount increment once outside SSO, while `std::string` requires heap allocation and memcpy for strings larger than 15 bytes.

| Size (bytes) | std::string | moo::string |
|---|---|---|
| 8 | 3.49 ns | 0.80 ns |
| 64 | 10.6 ns | 1.37 ns |
| 512 | 12.4 ns | 1.36 ns |
| 4096 | 45.3 ns | 1.37 ns |
| 32768 | 532 ns | 1.37 ns |
| 65536 | 1056 ns | 1.37 ns |

## Fan-out (Multiple Readers)

Simulates 10 readers copying the same source string (e.g. broadcasting a message payload).

| Size (bytes) | std::string | moo::string |
|---|---|---|
| 64 | 114 ns | 32.2 ns |
| 512 | 136 ns | 32.2 ns |
| 4096 | 617 ns | 32.2 ns |
| 16384 | 20422 ns | 32.2 ns |

## Copy Then Read

Passing strings by value to read-only observers.

| Size (bytes) | std::string | moo::string |
|---|---|---|
| 64 | 10.4 ns | 2.06 ns |
| 512 | 12.2 ns | 2.06 ns |
| 4096 | 45.4 ns | 2.06 ns |
| 65536 | 1056 ns | 2.06 ns |

## Equality Comparison

Checks two identical strings. Because COW copies share a control block, `moo::string` short-circuits on pointer comparison instead of scanning bytes.

| Size (bytes) | std::string | moo::string |
|---|---|---|
| 64 | 2.72 ns | 0.99 ns |
| 512 | 5.67 ns | 0.99 ns |
| 4096 | 43.0 ns | 0.99 ns |
| 65536 | 1023 ns | 0.99 ns |

## Copy Then Mutate (Detach Cost)

Copying a string and immediately mutating index 0 forces `moo::string` to detach and allocate. This measures the worst-case COW penalty.

| Size (bytes) | std::string | moo::string |
|---|---|---|
| 64 | 10.9 ns | 13.2 ns |
| 512 | 12.7 ns | 17.8 ns |
| 4096 | 45.6 ns | 46.2 ns |
| 65536 | 1057 ns | 1059 ns |

## SSO Threshold

`moo::string` holds up to 30 characters in its 32-byte layout, transitioning to heap COW at 32 bytes. `std::string` transitions to heap allocation beyond 15 bytes.

| Size (bytes) | std::string | moo::string |
|---|---|---|
| 24 | 10.7 ns | 0.81 ns |
| 30 | 10.7 ns | 0.81 ns |
| 32 | 10.4 ns | 1.37 ns |
| 40 | 10.4 ns | 1.37 ns |

## Atomic Refcount Overhead

Compares non-atomic `moo::string` against thread-safe `moo::ts_string` in single-threaded copies to isolate the atomic instruction overhead.

| Size (bytes) | moo::string (non-atomic) | moo::ts_string (atomic) |
|---|---|---|
| 64 | 1.38 ns | 3.19 ns |
| 512 | 1.38 ns | 3.19 ns |
| 4096 | 1.38 ns | 3.20 ns |

## Multithreaded Sharing (1KB Payload)

N reader threads copying and reading from a shared 1KB string. In `std::string`, each thread performs independent heap allocations. In `moo::ts_string`, all threads bump the same atomic refcount, leading to cache-line bouncing across cores.

| Threads | std::string | moo::ts_string |
|---|---|---|
| 1 | 14.8 ns | 3.41 ns |
| 2 | 27.4 ns | 51.1 ns |
| 4 | 32.3 ns | 117 ns |
| 8 | 40.8 ns | 227 ns |
