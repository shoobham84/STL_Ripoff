cmake_minimum_required(VERSION 3.23)

project(stringCOW LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(benchmark CONFIG REQUIRED)

add_executable(test_cow test_string_cow.cpp)
target_compile_options(test_cow PRIVATE -Wall -Wextra)

add_executable(bench_cow string_cow_benchmark.cpp)
target_compile_options(bench_cow PRIVATE -Wall -Wextra -O2 -DNDEBUG -march=native)
target_link_libraries(bench_cow PRIVATE benchmark::benchmark)
