#include <iostream>
#include <cassert>
#include "../include/spsc_queue.h"

int main() {
    SPSCQueue<int, 8> queue;

    // Test basic push/pop: push 5 items, pop them, check order
    for (int i = 0; i < 5; i++) assert(queue.push(i));
    for (int i = 0; i < 5; i++) {
        int val;
        assert(queue.pop(val));
        assert(val == i);  // fifo order!
    }

    // test full queue: push 8 items, verify 9th push returns false
    for (int i = 0; i < 8; i++) assert(queue.push(i));
    assert(queue.push(8) == false);

    // empty the queue by popping all 8 items
    for (int i = 0; i < 8; i++) {
        int val;
        assert(queue.pop(val));
    }

    // test empty queue: pop from empty queue, verify it returns false
    int dummy;
    assert(queue.pop(dummy) == false);


    // test wraparound: push 8, pop 8, push 5 more, pop 5 more
    for (int i = 0; i < 5; i++) assert(queue.push(i));
    for (int i = 0; i < 5; i++) {
        int val;
        assert(queue.pop(val));
        assert(val == i);
    }

    std::cout << "ALL TESTS PASSED" << std::endl;

    return 0;
}
