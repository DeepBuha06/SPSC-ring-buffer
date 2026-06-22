#include <atomic>
#include <cstddef>

#pragma once

template <typename T, size_t capacity>
// defining the capapcity in template brackets, i am forcing it to be a compile time constant
class SPSCQueue{
private:
    T buffer_[capacity];

    // if we do like this than according to cache lines of 64 byts, for some SPSCqueue with 8 byts data
    // we can have both this numbers in same cache line, making the cached data again being fetched even if it was correct for another pointer
    // that is what is called false sharing
    // std::atomic<size_t> head{0};
    // std::atomic<size_t> tail{0};
    // so we have to fix false sharing
    
    // alignas(64) std::atomic<size_t> head{0};
    // alignas(64) std::atomic<size_t> tail{0};

    // but hardcoding is a bad practise to do cause we have different cache line size
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> head{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> tail{0};
public:
    SPSCQueue(){
        // If this math is NOT zero, stop the build and print the error message!
        // cause we have to make the queue faster by removing overhead of the modulo operator 
        // and replacing it with a & operation
        static_assert((capacity & (capacity - 1)) == 0, "Capacity MUST be a power of 2!");
    }

    bool push(const T& item){
        // for loading a variable we own, we'll use relaxed
        size_t current_head = head.load(std::memory_order_relaxed);
        // for loading a variable owned by another core, we'll use acquire
        size_t current_tail = tail.load(std::memory_order_acquire);

        // if the current head - current tail is equal to the capacity, it means that the queue is full
        if(current_head - current_tail == capacity){
            return false;
        } 

        buffer_[current_head & (capacity - 1)] = item;

        // updating the head with release semantic
        // release ensure that all the write before this store are visible to the other core when it loads this variable
        head.store(current_head + 1, std::memory_order_release);

        return true;
    }

    // now for poping the data from the queue
    // this method will be called by the consumer core
    bool pop(T& item){
        // for loading a variable owned by another core, we'll use acquire
        size_t current_head = head.load(std::memory_order_acquire);
        // for loading a variable we own, we'll use relaxed
        size_t current_tail = tail.load(std::memory_order_relaxed);

        // if the current head - current tail is equal to the capacity, it means that the queue is empty
        if(current_head - current_tail == 0){
            return false;
        }
        
        item = buffer_[current_tail & (capacity - 1)];

        // updating the tail with release semantic
        // release ensure that all the write before this store are visible to the other core when it loads this variable
        tail.store(current_tail + 1, std::memory_order_release);

        return true;
    }
};
