// This is a simple thread-safe queue using std::mutex.
// It exists ONLY to be benchmarked against lock-free queue.

#pragma once

#include <queue>
#include <mutex>

template <typename T, size_t Capacity>
class MutexQueue {
private:
    std::queue<T> queue_;
    std::mutex mtx_;

public:
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.size() >= Capacity) return false;
        queue_.push(item);
        return true;
    }

    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return false;
        item = queue_.front();
        queue_.pop();
        return true;
    }
};
