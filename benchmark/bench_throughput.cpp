// COMPILE: g++ bench_throughput.cpp -O3 -pthread -o bench_tp && ./bench_tp

#include <iostream>
#include <thread>
#include <chrono>
#include "../include/spsc_queue.h"
#include "competitors/mutex_queue.h"

#include <boost/lockfree/spsc_queue.hpp>

const int NUM_ITEMS = 10'000'000;

template<typename T>
void producer(T& q) {
    for (int i = 0; i < NUM_ITEMS; i++){
        while (!q.push(i)) {}
    }
}

template<typename T>
void consumer(T& q) {
    int val;
    for (int i = 0; i < NUM_ITEMS; i++){
        while (!q.pop(val)) {}
    }
}

template<typename T>
double bench_throughput(T& q){
    auto start = std::chrono::high_resolution_clock::now();
    
    std::thread prod(producer<T>, std::ref(q));
    std::thread cons(consumer<T>, std::ref(q));
    
    prod.join(); 
    cons.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    return NUM_ITEMS / seconds; // ops per second
}


int main() {
    SPSCQueue<int, 1024> my_queue;
    MutexQueue<int, 1024> slow_queue;
    boost::lockfree::spsc_queue<int> boost_queue(1024);

    double my_ops = bench_throughput(my_queue);
    double slow_ops = bench_throughput(slow_queue);
    double boost_ops = bench_throughput(boost_queue);

    std::cout << "My Lock-Free Queue: " << my_ops / 1000000.0 << " Million ops/sec\n";
    std::cout << "Standard Mutex Queue: " << slow_ops / 1000000.0 << " Million ops/sec\n";
    std::cout << "Boost Lock-Free Queue: " << boost_ops / 1000000.0 << " Million ops/sec\n";
    
    return 0;
}
