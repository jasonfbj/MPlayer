#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

template<typename T>
class BlockingQueue {
public:
    BlockingQueue() : max_size_(0), finished_(false) {}

    void setMaxSize(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_size_ = size;
    }

    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            return false;
        }
        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    bool pop(T& item, int timeoutMs = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeoutMs < 0) {
            not_empty_.wait(lock, [this] { return !queue_.empty() || finished_; });
        } else {
            if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                [this] { return !queue_.empty() || finished_; })) {
                return false;
            }
        }
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        queue_.swap(empty);
        finished_ = false;  // Reset so the queue can be reused after close()
    }

    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    bool isFull() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return max_size_ > 0 && queue_.size() >= max_size_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void finish() {
        std::lock_guard<std::mutex> lock(mutex_);
        finished_ = true;
        not_empty_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::queue<T> queue_;
    size_t max_size_;
    bool finished_;
};
