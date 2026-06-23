cmake_minimum_required(VERSION 3.16)
project(spsc_ring_buffer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags
add_compile_options(-O3 -Wall -Wextra -Wno-interference-size)

# Header-only library target
add_library(spsc_queue INTERFACE)
target_include_directories(spsc_queue INTERFACE ${CMAKE_SOURCE_DIR}/include)

# Boost (header-only lockfree component)
find_package(Boost REQUIRED)

# --- Tests ---

add_executable(test_single tests/test_single_thread.cpp)
target_link_libraries(test_single PRIVATE spsc_queue)

add_executable(test_multi tests/test_multi_thread.cpp)
target_link_libraries(test_multi PRIVATE spsc_queue Threads::Threads)

# --- Benchmarks ---

find_package(Threads REQUIRED)

add_executable(bench_throughput benchmark/bench_throughput.cpp)
target_link_libraries(bench_throughput PRIVATE spsc_queue Boost::boost Threads::Threads)
target_include_directories(bench_throughput PRIVATE benchmark)

add_executable(bench_latency benchmark/bench_latency.cpp)
target_link_libraries(bench_latency PRIVATE spsc_queue Boost::boost Threads::Threads)
target_include_directories(bench_latency PRIVATE benchmark)
