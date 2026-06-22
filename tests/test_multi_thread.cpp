#include <iostream>
#include <thread>
#include <cassert>
#include "../include/spsc_queue.h"

void producer(SPSCQueue<int, 1024>& q) {
    for (int i = 0; i < 1000000; i++) {
        while (!q.push(i)) { } // retry until success
    }
}

void consumer(SPSCQueue<int, 1024>& q) {
    for (int i = 0; i < 1000000; i++) {
        int val;
        while (!q.pop(val)) { } // retry until success
        assert(val == i);      // must arrive in FIFO order
    }
}

int main() {
    SPSCQueue<int, 1024> queue;
    
    // using ref to give the same queue object instead of copy of the queue object
    std::thread t1(producer, std::ref(queue));
    std::thread t2(consumer, std::ref(queue));
    t1.join();
    t2.join();

    std::cout << "ALL TESTS PASSED" << std::endl;

    return 0;
}
