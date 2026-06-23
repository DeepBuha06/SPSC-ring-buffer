// COMPILE: cmake --build build --target bench_latency

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <immintrin.h>   // for _mm_pause()
#include <pthread.h>     // for pthread_setaffinity_np (CPU pinning)
#include "../include/spsc_queue.h"
#include "competitors/mutex_queue.h"
#include "competitors/readerwriterqueue.h"
#include <boost/lockfree/spsc_queue.hpp>

template <typename T>
struct MoodycamelAdapter {
    moodycamel::ReaderWriterQueue<T> q;
    explicit MoodycamelAdapter(size_t size) : q(size) {}
    bool push(const T& item) { return q.try_enqueue(item); }
    bool pop(T& item) { return q.try_dequeue(item); }
};

const int NUM_ITEMS    = 1'000'000;
const int WARMUP_ITEMS = 100'000;
const int NUM_ROUNDS   = 10;
const int PRODUCER_CORE = 0;
const int CONSUMER_CORE = 1;

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

struct LatencyResult {
    long long p50;
    long long p99;
    long long p999;
};

template <typename T>
void producer(T& q, int core, int count) {
    pin_thread_to_core(core);
    for (int i = 0; i < count; i++) {
        TimestampedItem item{i, std::chrono::high_resolution_clock::now()};
        while (!q.push(item)) { _mm_pause(); }
    }
}

template <typename T>
void warmup_consumer(T& q, int core, int count) {
    pin_thread_to_core(core);
    for (int i = 0; i < count; i++) {
        TimestampedItem item;
        while (!q.pop(item)) { _mm_pause(); }
    }
}

template <typename T>
LatencyResult measure_once(T& q) {
    pin_thread_to_core(CONSUMER_CORE);
    std::vector<long long> latencies(NUM_ITEMS);

    std::thread prod(producer<T>, std::ref(q), PRODUCER_CORE, NUM_ITEMS);

    for (int i = 0; i < NUM_ITEMS; i++) {
        TimestampedItem item;
        while (!q.pop(item)) { _mm_pause(); }
        auto now = std::chrono::high_resolution_clock::now();
        latencies[i] = std::chrono::duration_cast<
            std::chrono::nanoseconds>(now - item.send_time).count();
    }
    prod.join();

    std::sort(latencies.begin(), latencies.end());
    return {
        latencies[NUM_ITEMS * 50 / 100],
        latencies[NUM_ITEMS * 99 / 100],
        latencies[NUM_ITEMS * 999 / 1000]
    };
}

template <typename T>
std::vector<LatencyResult> run_benchmark(T& q) {
    // Warmup
    {
        std::thread wp(producer<T>, std::ref(q), PRODUCER_CORE, WARMUP_ITEMS);
        std::thread wc(warmup_consumer<T>, std::ref(q), CONSUMER_CORE, WARMUP_ITEMS);
        wp.join();
        wc.join();
    }
    // Measured rounds
    std::vector<LatencyResult> results;
    for (int r = 0; r < NUM_ROUNDS; r++) {
        results.push_back(measure_once(q));
    }
    return results;
}

long long median_of(std::vector<long long>& v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

void print_row(const char* name, const std::vector<LatencyResult>& results) {
    std::vector<long long> p50s, p99s, p999s;
    for (auto& r : results) {
        p50s.push_back(r.p50);
        p99s.push_back(r.p99);
        p999s.push_back(r.p999);
    }
    printf("  %-28s %10lld    %10lld    %10lld\n",
           name, median_of(p50s), median_of(p99s), median_of(p999s));
}

int main() {
    SPSCQueue<TimestampedItem, 1024> spsc_q;
    MutexQueue<TimestampedItem, 1024> mutex_q;
    boost::lockfree::spsc_queue<TimestampedItem, boost::lockfree::capacity<1024>> boost_q;
    MoodycamelAdapter<TimestampedItem> moodycamel_q(1024);

    auto spsc_res       = run_benchmark(spsc_q);
    auto mutex_res      = run_benchmark(mutex_q);
    auto boost_res      = run_benchmark(boost_q);
    auto moodycamel_res = run_benchmark(moodycamel_q);

    printf("\n  Latency Percentiles (%d rounds, %dM items each, median of rounds)\n",
           NUM_ROUNDS, NUM_ITEMS / 1'000'000);
    printf("  %-28s %10s    %10s    %10s\n", "Implementation", "p50 (ns)", "p99 (ns)", "p99.9 (ns)");
    printf("  %-28s %10s    %10s    %10s\n", "---", "---", "---", "---");
    print_row("Custom SPSC Queue", spsc_res);
    print_row("boost::lockfree::spsc_queue", boost_res);
    print_row("moodycamel::ReaderWriterQueue", moodycamel_res);
    print_row("std::mutex Queue", mutex_res);
    printf("\n");

    return 0;
}
