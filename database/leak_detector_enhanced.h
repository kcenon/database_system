#pragma once
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace database {

class connection_leak_detector {
public:
    struct connection_info {
        std::chrono::steady_clock::time_point acquired_time;
        std::string stack_trace;
        std::thread::id thread_id;
    };

    static connection_leak_detector& instance() {
        static connection_leak_detector detector;
        return detector;
    }

    void track_acquisition(void* conn_ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        tracked_[conn_ptr] = {
            std::chrono::steady_clock::now(),
            capture_stack_trace(),
            std::this_thread::get_id()
        };
        total_acquired_++;
    }

    void track_release(void* conn_ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        tracked_.erase(conn_ptr);
        total_released_++;
    }

    size_t get_leak_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tracked_.size();
    }

private:
    std::string capture_stack_trace() const {
        return "[stack trace placeholder]";
    }

    mutable std::mutex mutex_;
    std::unordered_map<void*, connection_info> tracked_;
    std::atomic<size_t> total_acquired_{0};
    std::atomic<size_t> total_released_{0};
};

} // namespace
