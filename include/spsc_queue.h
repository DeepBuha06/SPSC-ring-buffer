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

    // Non-atomic local caches of the remote index
    // push() caches tail (owned by consumer) — avoids cross-core fetch when queue is NOT full
    // pop() caches head (owned by producer) — avoids cross-core fetch when queue is NOT empty
    // Staleness is safe: both indices only ever increase, so stale values
    // overestimate fullness (push) or underestimate availability (pop)
    size_t cached_tail_ = 0;  // used by producer in push()
    size_t cached_head_ = 0;  // used by consumer in pop()
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
        // size_t current_tail = tail.load(std::memory_order_acquire);
        // but this is doing cross cache fetch everytime head changes, so we have to reduce that overhead
        // what we can do is (use the old cached tail and if we see queue is full, which it may or may not be)
        // due to head moving forward only
        if (current_head - cached_tail_ == capacity){
            cached_tail_ = tail.load(std::memory_order_acquire);// cross core fetch only when required
            
            // now check if full or not correctly
            // if the current head - current tail is equal to the capacity, it means that the queue is full
            if(current_head - cached_tail_ == capacity){
                return false;
            } 
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
        // for loading a variable we own, we'll use relaxed
        size_t current_tail = tail.load(std::memory_order_relaxed);
        // for loading a variable owned by another core, we'll use acquire
        // but same as push(), we cache head locally to avoid cross-core fetch every time
        // cached_head_ can only be stale in the safe direction:
        //   stale cached_head_ <= real head → we think queue has FEWER items than reality
        //   → we might think it's empty when it's not → just re-read, safe!
        //   → we'll NEVER read past valid data
        if (cached_head_ - current_tail == 0){
            cached_head_ = head.load(std::memory_order_acquire); // cross core fetch only when needed

            // now check if truly empty
            if(cached_head_ - current_tail == 0){
                return false;
            }
        }
        
        item = buffer_[current_tail & (capacity - 1)];

        // updating the tail with release semantic
        // release ensure that all the write before this store are visible to the other core when it loads this variable
        tail.store(current_tail + 1, std::memory_order_release);

        return true;
    }
};
