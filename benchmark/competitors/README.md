# Installing Boost for Benchmark Comparison

## What is Boost?
Boost is a collection of peer-reviewed, production-quality C++ libraries.
`boost::lockfree::spsc_queue` is their lock-free SPSC queue implementation 
used in real production systems. Comparing your queue against it proves 
your implementation is competitive with industry-standard code.

## Install on WSL (Ubuntu)
```bash
sudo apt update
sudo apt install libboost-all-dev
```

## Verify Installation
```bash
dpkg -l | grep libboost
```
You should see multiple `libboost-*` packages listed.

## Usage in Code
```cpp
#include <boost/lockfree/spsc_queue.hpp>

// Create a queue with fixed capacity (compile-time, like yours)
boost::lockfree::spsc_queue<int, boost::lockfree::capacity<1024>> queue;

// Push and pop — same API pattern as your queue
queue.push(42);
int val;
queue.pop(val);  // val == 42
```

## Compile With Boost
```bash
g++ bench_throughput.cpp -O3 -pthread -o bench
```
No extra link flags needed for the header-only lockfree library.
