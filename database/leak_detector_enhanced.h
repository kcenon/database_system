#pragma once
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace database {

/**
 * @class connection_leak_detector
 * @brief Enhanced connection leak detector with stack trace capture
 *
 * @note Sprint 3 (Task 3.2): Converted from singleton to dependency injection pattern
 */
class connection_leak_detector {
public:
    struct connection_info {
        std::chrono::steady_clock::time_point acquired_time;
        std::string stack_trace;
        std::thread::id thread_id;
    };

    /**
     * @brief Default constructor (now public for dependency injection)
     * @since Sprint 3 (Task 3.2)
     */
    connection_leak_detector() = default;

    /**
     * @brief Singleton instance accessor (DEPRECATED)
     * @deprecated Use dependency injection via database_context instead.
     *
     * Migration guide:
     * @code
     * // Old (deprecated):
     * auto& detector = connection_leak_detector::instance();
     *
     * // New (recommended):
     * auto context = std::make_shared<database_context>();
     * auto detector = context->get_leak_detector();
     * @endcode
     */
    [[deprecated("Use database_context::get_leak_detector() instead")]]
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
