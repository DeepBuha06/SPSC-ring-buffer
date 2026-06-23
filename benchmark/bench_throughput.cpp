// COMPILE: cmake --build build --target bench_throughput

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <pthread.h>
#include "../include/spsc_queue.h"
#include "competitors/mutex_queue.h"
#include "competitors/readerwriterqueue.h"
#include <boost/lockfree/spsc_queue.hpp>

// Adapter for moodycamel::ReaderWriterQueue to unify push/pop interface
template <typename T>
struct MoodycamelAdapter {
    moodycamel::ReaderWriterQueue<T> q;
    explicit MoodycamelAdapter(size_t size) : q(size) {}
    bool push(const T& item) { return q.try_enqueue(item); }
    bool pop(T& item) { return q.try_dequeue(item); }
};

const int NUM_ITEMS  = 10'000'000;
const int WARMUP_ITEMS = 1'000'000;
const int NUM_ROUNDS = 10;
const int PRODUCER_CORE = 0;
const int CONSUMER_CORE = 1;

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

template<typename T>
void producer(T& q, int core, int count) {
    pin_thread_to_core(core);
    for (int i = 0; i < count; i++) {
        while (!q.push(i)) { _mm_pause(); }
    }
}

template<typename T>
void consumer(T& q, int core, int count) {
    pin_thread_to_core(core);
    int val;
    for (int i = 0; i < count; i++) {
        while (!q.pop(val)) { _mm_pause(); }
    }
}

template<typename T>
double measure_once(T& q) {
    auto start = std::chrono::high_resolution_clock::now();
    std::thread prod(producer<T>, std::ref(q), PRODUCER_CORE, NUM_ITEMS);
    std::thread cons(consumer<T>, std::ref(q), CONSUMER_CORE, NUM_ITEMS);
    prod.join();
    cons.join();
    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    return NUM_ITEMS / seconds;
}

template<typename T>
std::vector<double> run_benchmark(T& q) {
    // Warmup
    {
        std::thread wp(producer<T>, std::ref(q), PRODUCER_CORE, WARMUP_ITEMS);
        std::thread wc(consumer<T>, std::ref(q), CONSUMER_CORE, WARMUP_ITEMS);
        wp.join();
        wc.join();
    }
    // Measured rounds
    std::vector<double> results;
    for (int r = 0; r < NUM_ROUNDS; r++) {
        results.push_back(measure_once(q));
    }
    std::sort(results.begin(), results.end());
    return results;
}

void print_row(const char* name, const std::vector<double>& results) {
    double median = results[results.size() / 2];
    double min_v  = results.front();
    double max_v  = results.back();
    printf("  %-28s %10.2f    %10.2f    %10.2f\n",
           name, median / 1e6, min_v / 1e6, max_v / 1e6);
}

int main() {
    SPSCQueue<int, 1024> spsc_q;
    MutexQueue<int, 1024> mutex_q;
    boost::lockfree::spsc_queue<int, boost::lockfree::capacity<1024>> boost_q;
    MoodycamelAdapter<int> moodycamel_q(1024);

    auto spsc_res      = run_benchmark(spsc_q);
    auto mutex_res     = run_benchmark(mutex_q);
    auto boost_res     = run_benchmark(boost_q);
    auto moodycamel_res = run_benchmark(moodycamel_q);

    printf("\n  Throughput (%d rounds, %dM items each)\n", NUM_ROUNDS, NUM_ITEMS / 1'000'000);
    printf("  %-28s %10s    %10s    %10s\n", "Implementation", "Median", "Min", "Max");
    printf("  %-28s %10s    %10s    %10s\n", "---", "---", "---", "---");
    print_row("Custom SPSC Queue", spsc_res);
    print_row("boost::lockfree::spsc_queue", boost_res);
    print_row("moodycamel::ReaderWriterQueue", moodycamel_res);
    print_row("std::mutex Queue", mutex_res);
    printf("  (M ops/sec)\n\n");

    return 0;
}
