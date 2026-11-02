#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace rtp {

template <typename T>
class ThreadSafeRingBuffer {
public:
    explicit ThreadSafeRingBuffer(size_t capacity)
        : capacity_(capacity), buffer_(capacity) {}

    ~ThreadSafeRingBuffer() {
        shutdown();
    }

    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (closed_) return false;

        // If buffer full, drop or reject
        if (size_ == capacity_) {
            overflow_count_++;
            return false;
        }

        buffer_[tail_] = std::move(item);
        tail_ = (tail_ + 1) % capacity_;
        size_++;

        not_empty_cv_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (size_ == 0 && !closed_) {
            not_empty_cv_.wait(lock);
        }

        if (size_ == 0) return std::nullopt;

        T item = std::move(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        size_--;

        return item;
    }

    std::optional<T> pop_timeout(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size_ == 0 && !closed_) {
            not_empty_cv_.wait_for(lock, timeout, [this] {
                return size_ > 0 || closed_;
            });
        }

        if (size_ == 0) return std::nullopt;

        T item = std::move(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        size_--;

        return item;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_empty_cv_.notify_all();
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == 0;
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] uint64_t overflow_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return overflow_count_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;
    size_t head_{0};
    size_t tail_{0};
    size_t size_{0};
    uint64_t overflow_count_{0};
    bool closed_{false};

    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
};

} // namespace rtp
