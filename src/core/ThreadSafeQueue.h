#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace railshot {

template<typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        {
            std::lock_guard lock(mutex_);
            if (shutdown_) {
                return;
            }
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    std::optional<T> pop(int timeoutMs = 100) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
                return !queue_.empty() || shutdown_;
            })) {
            return std::nullopt;
        }
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    void reset() {
        std::lock_guard lock(mutex_);
        shutdown_ = false;
        std::queue<T> empty;
        std::swap(queue_, empty);
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] bool isShutdown() const {
        std::lock_guard lock(mutex_);
        return shutdown_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool shutdown_ = false;
};

} // namespace railshot
