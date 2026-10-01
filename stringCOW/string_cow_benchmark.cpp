#include <benchmark/benchmark.h>
#include "basic_string_cow.hpp"
#include <string>
#include <vector>

// =============================================================================
//  1. Copy construction — the core COW win
// =============================================================================

static void BM_StdString_CopyCtor(benchmark::State& state) {
	std::string s(state.range(0), 'x');
	for (auto _ : state) {
		std::string copy = s;
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_StdString_CopyCtor)->Range(8, 1 << 16);

static void BM_MooString_CopyCtor(benchmark::State& state) {
	moo::string s(state.range(0), 'x');
	for (auto _ : state) {
		moo::string copy = s;
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_MooString_CopyCtor)->Range(8, 1 << 16);

// =============================================================================
//  2. Copy-then-read: copy a string, then read from it (no mutation)
//     Real-world pattern: passing strings to functions that only observe.
// =============================================================================

static void BM_StdString_CopyThenRead(benchmark::State& state) {
	std::string s(state.range(0), 'x');
	for (auto _ : state) {
		std::string copy = s;
		benchmark::DoNotOptimize(copy.data());
		benchmark::DoNotOptimize(copy.size());
	}
}
BENCHMARK(BM_StdString_CopyThenRead)->Range(64, 1 << 16);

static void BM_MooString_CopyThenRead(benchmark::State& state) {
	moo::string s(state.range(0), 'x');
	for (auto _ : state) {
		moo::string copy = s;
		benchmark::DoNotOptimize(copy.data());
		benchmark::DoNotOptimize(copy.size());
	}
}
BENCHMARK(BM_MooString_CopyThenRead)->Range(64, 1 << 16);

// =============================================================================
//  3. Multiple copies: simulate N readers sharing one string (e.g. broadcast)
// =============================================================================

static void BM_StdString_FanOut(benchmark::State& state) {
	const int N = 10;
	std::string s(state.range(0), 'x');
	for (auto _ : state) {
		std::vector<std::string> copies(N, s);
		benchmark::DoNotOptimize(copies.data());
	}
}
BENCHMARK(BM_StdString_FanOut)->Range(64, 1 << 14);

static void BM_MooString_FanOut(benchmark::State& state) {
	const int N = 10;
	moo::string s(state.range(0), 'x');
	for (auto _ : state) {
		std::vector<moo::string> copies(N, s);
		benchmark::DoNotOptimize(copies.data());
	}
}
BENCHMARK(BM_MooString_FanOut)->Range(64, 1 << 14);

// =============================================================================
//  4. Copy-then-mutate: copy and then write (forces COW detach)
//     This is where COW pays its cost — should be comparable or slightly
//     slower than std::string.
// =============================================================================

static void BM_StdString_CopyThenMutate(benchmark::State& state) {
	std::string s(state.range(0), 'x');
	for (auto _ : state) {
		std::string copy = s;
		copy[0] = 'y';
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_StdString_CopyThenMutate)->Range(64, 1 << 16);

static void BM_MooString_CopyThenMutate(benchmark::State& state) {
	moo::string s(state.range(0), 'x');
	for (auto _ : state) {
		moo::string copy = s;
		copy[0] = 'y';  // triggers detach
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_MooString_CopyThenMutate)->Range(64, 1 << 16);

// =============================================================================
//  5. Move construction — should be ~equal for both
// =============================================================================

static void BM_StdString_MoveCtor(benchmark::State& state) {
	for (auto _ : state) {
		std::string s(state.range(0), 'x');
		std::string moved = std::move(s);
		benchmark::DoNotOptimize(moved);
	}
}
BENCHMARK(BM_StdString_MoveCtor)->Range(64, 1 << 16);

static void BM_MooString_MoveCtor(benchmark::State& state) {
	for (auto _ : state) {
		moo::string s(state.range(0), 'x');
		moo::string moved = std::move(s);
		benchmark::DoNotOptimize(moved);
	}
}
BENCHMARK(BM_MooString_MoveCtor)->Range(64, 1 << 16);

// =============================================================================
//  6. Append (push_back) — build a string char-by-char
// =============================================================================

static void BM_StdString_PushBack(benchmark::State& state) {
	for (auto _ : state) {
		std::string s;
		s.reserve(state.range(0));
		for (int64_t i = 0; i < state.range(0); ++i)
			s.push_back('a');
		benchmark::DoNotOptimize(s);
	}
}
BENCHMARK(BM_StdString_PushBack)->Range(64, 1 << 14);

static void BM_MooString_PushBack(benchmark::State& state) {
	for (auto _ : state) {
		moo::string s;
		s.reserve(state.range(0));
		for (int64_t i = 0; i < state.range(0); ++i)
			s.push_back('a');
		benchmark::DoNotOptimize(s);
	}
}
BENCHMARK(BM_MooString_PushBack)->Range(64, 1 << 14);

// =============================================================================
//  7. Append (bulk) — append a string_view in one shot
// =============================================================================

static void BM_StdString_Append(benchmark::State& state) {
	std::string payload(state.range(0), 'z');
	for (auto _ : state) {
		std::string s;
		s += payload;
		benchmark::DoNotOptimize(s);
	}
}
BENCHMARK(BM_StdString_Append)->Range(64, 1 << 14);

static void BM_MooString_Append(benchmark::State& state) {
	moo::string payload(state.range(0), 'z');
	for (auto _ : state) {
		moo::string s;
		s += payload.view();
		benchmark::DoNotOptimize(s);
	}
}
BENCHMARK(BM_MooString_Append)->Range(64, 1 << 14);

// =============================================================================
//  8. Equality comparison — COW can short-circuit on pointer identity
// =============================================================================

static void BM_StdString_EqualitySameContent(benchmark::State& state) {
	std::string a(state.range(0), 'x');
	std::string b = a;
	for (auto _ : state) {
		bool eq = (a == b);
		benchmark::DoNotOptimize(eq);
	}
}
BENCHMARK(BM_StdString_EqualitySameContent)->Range(64, 1 << 16);

static void BM_MooString_EqualitySameContent(benchmark::State& state) {
	moo::string a(state.range(0), 'x');
	moo::string b = a;  // shares control block
	for (auto _ : state) {
		bool eq = (a == b);
		benchmark::DoNotOptimize(eq);
	}
}
BENCHMARK(BM_MooString_EqualitySameContent)->Range(64, 1 << 16);

// =============================================================================
//  9. Construction from C string literal
// =============================================================================

static void BM_StdString_FromCStr(benchmark::State& state) {
	// build a C-string of the right length on the heap so we can vary size
	std::vector<char> buf(state.range(0) + 1, 'a');
	buf.back() = '\0';
	const char* cstr = buf.data();
	for (auto _ : state) {
		std::string s(cstr);
		benchmark::DoNotOptimize(s);
	}
}
BENCHMARK(BM_StdString_FromCStr)->Range(8, 1 << 14);

static void BM_MooString_FromCStr(benchmark::State& state) {
	std::vector<char> buf(state.range(0) + 1, 'a');
	buf.back() = '\0';
	const char* cstr = buf.data();
	for (auto _ : state) {
		moo::string s(cstr);
		benchmark::DoNotOptimize(s);
	}
}
BENCHMARK(BM_MooString_FromCStr)->Range(8, 1 << 14);

// =============================================================================
// 10. SSO boundary: exercise strings right around the SSO threshold (30 bytes)
// =============================================================================

static void BM_StdString_CopyNearSSO(benchmark::State& state) {
	std::string s(state.range(0), 'x');
	for (auto _ : state) {
		std::string copy = s;
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_StdString_CopyNearSSO)->DenseRange(24, 40, 2);

static void BM_MooString_CopyNearSSO(benchmark::State& state) {
	moo::string s(state.range(0), 'x');
	for (auto _ : state) {
		moo::string copy = s;
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_MooString_CopyNearSSO)->DenseRange(24, 40, 2);

// =============================================================================
// 11. Multithreaded concurrent sharing:
//     N reader threads copying and reading from a single shared 1KB string.
//     std::string: each thread invokes malloc + memcpy + free.
//     moo::ts_string: each thread performs lock-free atomic inc/dec.
// =============================================================================

static void BM_StdString_MultithreadedRead(benchmark::State& state) {
	static const std::string shared_std(state.range(0), 'x');
	for (auto _ : state) {
		std::string copy = shared_std;
		benchmark::DoNotOptimize(copy.data());
	}
}
BENCHMARK(BM_StdString_MultithreadedRead)->Arg(1024)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

static void BM_MooTsString_MultithreadedRead(benchmark::State& state) {
	static const moo::ts_string shared_moo(state.range(0), 'x');
	for (auto _ : state) {
		moo::ts_string copy = shared_moo;
		benchmark::DoNotOptimize(copy.data());
	}
}
BENCHMARK(BM_MooTsString_MultithreadedRead)->Arg(1024)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// =============================================================================
// 12. Atomic Overhead:
//     Compare non-atomic moo::string vs atomic moo::ts_string in single-threaded
//     to measure the exact nanosecond cost of std::atomic refcounting.
// =============================================================================

static void BM_AtomicCost_NonAtomic(benchmark::State& state) {
	moo::string s(state.range(0), 'x');
	for (auto _ : state) {
		moo::string copy = s;
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_AtomicCost_NonAtomic)->Range(64, 4096);

static void BM_AtomicCost_Atomic(benchmark::State& state) {
	moo::ts_string s(state.range(0), 'x');
	for (auto _ : state) {
		moo::ts_string copy = s;
		benchmark::DoNotOptimize(copy);
	}
}
BENCHMARK(BM_AtomicCost_Atomic)->Range(64, 4096);

BENCHMARK_MAIN();
