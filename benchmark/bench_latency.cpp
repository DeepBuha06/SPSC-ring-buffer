// COMPILE: g++ bench_latency.cpp -O3 -pthread -o bench_lat && ./bench_lat

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <immintrin.h>   // for _mm_pause()
#include <pthread.h>     // for pthread_setaffinity_np (CPU pinning)
#include "../include/spsc_queue.h"
#include "competitors/mutex_queue.h"
#include <boost/lockfree/spsc_queue.hpp>

const int NUM_ITEMS = 1'000'000; // 1M (we store each latency, so memory matters)

// Pin the CALLING thread to a specific CPU core.
// This prevents the OS scheduler from migrating threads between cores,
// which would cause ALL cached data to be invalidated (multi-ms spike).
void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

struct TimestampedItem {
    int value;
    std::chrono::high_resolution_clock::time_point send_time;
};

template <typename T>
void producer(T& q, int core, int count) {
    pin_thread_to_core(core);
    for (int i = 0; i < count; i++) {
        TimestampedItem item{i, std::chrono::high_resolution_clock::now()};
        while (!q.push(item)) {
            _mm_pause();
        }
    }
}

// warmup consumer: just drains the queue, doesn't record anything
template <typename T>
void warmup_consumer(T& q, int core, int count) {
    pin_thread_to_core(core);
    for (int i = 0; i < count; i++) {
        TimestampedItem item;
        while (!q.pop(item)) {
            _mm_pause();
        }
    }
}

// real consumer: records latencies and prints percentiles
template <typename T>
void consumer(T& q, int core, int count) {
    pin_thread_to_core(core);
    std::vector<long long> latencies(count);
    for (int i = 0; i < count; i++) {
        TimestampedItem item;
        while (!q.pop(item)) {
            _mm_pause();
        }
        auto now = std::chrono::high_resolution_clock::now();
        latencies[i] = std::chrono::duration_cast<
            std::chrono::nanoseconds>(now - item.send_time).count();
    }
    std::sort(latencies.begin(), latencies.end());
    long long p50  = latencies[count * 50 / 100];
    long long p99  = latencies[count * 99 / 100];
    long long p999 = latencies[count * 999 / 1000];
    printf("p50: %lld ns  |  p99: %lld ns  |  p99.9: %lld ns\n",
           p50, p99, p999);
}

const int WARMUP_ITEMS = 100'000;  // 100K items to warm up caches, TLB, branch predictor
const int PRODUCER_CORE = 0;
const int CONSUMER_CORE = 1;

// Run a fair benchmark: warmup pass first (results discarded), then real measurement.
// This ensures every queue starts from equally warm state - no ordering bias.
template <typename T>
void run_benchmark(const char* name, T& q) {
    // --- WARMUP: exercise same code path, discard results ---
    {
        std::thread wp(producer<T>, std::ref(q), PRODUCER_CORE, WARMUP_ITEMS);
        std::thread wc(warmup_consumer<T>, std::ref(q), CONSUMER_CORE, WARMUP_ITEMS);
        wp.join();
        wc.join();
    }
    // --- REAL MEASUREMENT ---
    std::cout << name << std::endl;
    std::thread p(producer<T>, std::ref(q), PRODUCER_CORE, NUM_ITEMS);
    std::thread c(consumer<T>, std::ref(q), CONSUMER_CORE, NUM_ITEMS);
    p.join();
    c.join();
}

int main() {
    SPSCQueue<TimestampedItem, 1024> spsc_q;
    MutexQueue<TimestampedItem, 1024> mutex_q;
    boost::lockfree::spsc_queue<TimestampedItem> boost_q(1024);

    // Order doesn't matter anymore - each gets its own warmup
    run_benchmark("Custom SPSC Queue:", spsc_q);
    run_benchmark("Mutex Queue:", mutex_q);
    run_benchmark("Boost Queue:", boost_q);
    
    return 0;
}
