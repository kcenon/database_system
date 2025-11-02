/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "connection_pool_v2.h"

#ifdef USE_THREAD_SYSTEM
    #include <kcenon/thread/core/thread_worker.h>
    #include <kcenon/thread/interfaces/thread_context.h>
#endif

namespace database::pooling {

connection_pool_v2::connection_pool_v2(
    database_types db_type,
    const connection_pool_config& config,
    std::function<std::unique_ptr<database_base>()> factory,
    size_t thread_count)
    : underlying_pool_(std::make_shared<connection_pool>(db_type, config, std::move(factory)))
#ifdef USE_THREAD_SYSTEM
    , scheduler_pool_(nullptr)
    , thread_count_(thread_count)
    , metrics_(std::make_shared<monitoring::priority_metrics<connection_priority>>())
#else
    , metrics_(std::make_shared<monitoring::pool_metrics>())
#endif
    , shutdown_requested_(false)
{
#ifdef USE_THREAD_SYSTEM
    // Create typed thread pool for priority-based scheduling
    async::thread_context_type context;
    scheduler_pool_ = std::make_shared<kcenon::thread::typed_thread_pool_t<connection_priority>>(
        "connection_pool_v2_scheduler",
        context
    );

    // Add workers to the scheduler pool
    // Typed workers need to specify which priority levels they handle
    std::vector<connection_priority> all_priorities = {
        connection_priority::HEALTH_CHECK,
        connection_priority::NORMAL_QUERY,
        connection_priority::TRANSACTION,
        connection_priority::CRITICAL
    };

    // Add workers before starting the pool
    for (size_t i = 0; i < thread_count_; ++i) {
        // Each worker handles all priority levels
        // Constructor: (types, use_time_tag, context)
        auto worker = std::make_unique<kcenon::thread::typed_thread_worker_t<connection_priority>>(
            all_priorities,
            true,  // use_time_tag
            context
        );

        auto add_result = scheduler_pool_->enqueue(std::move(worker));
        if (add_result.has_error()) {
            throw std::runtime_error("Failed to add scheduler worker: " +
                                   add_result.get_error().message());
        }
    }

    // Start scheduler pool after adding workers
    auto result = scheduler_pool_->start();
    if (result.has_error()) {
        throw std::runtime_error("Failed to start connection_pool_v2 scheduler: " +
                               result.get_error().message());
    }
#endif
}

connection_pool_v2::~connection_pool_v2() {
    shutdown();
}

bool connection_pool_v2::initialize() {
    if (!underlying_pool_) {
        return false;
    }

    return underlying_pool_->initialize();
}

std::future<Result<std::shared_ptr<connection_wrapper>>>
connection_pool_v2::acquire_connection(connection_priority priority) {
#ifdef USE_THREAD_SYSTEM
    // Use priority-based scheduling with thread_system
    auto promise = std::make_shared<std::promise<Result<std::shared_ptr<connection_wrapper>>>>();
    auto future = promise->get_future();

    // Record start time for metrics
    auto start_time = std::chrono::steady_clock::now();

    // Update queued count
    if (metrics_) {
        metrics_->update_queued(1);
    }

    // Capture metrics as shared_ptr to avoid accessing `this` from worker thread
    auto metrics_copy = metrics_;

    // Create completion callback with metrics tracking
    auto callback = [promise, start_time, priority, metrics_copy](Result<std::shared_ptr<connection_wrapper>> result) {
        // Calculate wait time
        auto end_time = std::chrono::steady_clock::now();
        auto wait_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time
        ).count();

        // Record metrics
        if (metrics_copy) {
            metrics_copy->update_queued(-1);

            if (result.is_ok()) {
                metrics_copy->record_acquisition_with_priority(priority, wait_time_us, true);
                metrics_copy->update_active(1);
            } else {
                metrics_copy->record_acquisition_with_priority(priority, wait_time_us, false);
            }
        }

        promise->set_value(std::move(result));
    };

    // Create priority-based job
    auto job = std::make_unique<connection_request_job<connection_priority>>(
        priority,
        underlying_pool_,
        std::move(callback)
    );

    // Submit to scheduler
    auto enqueue_result = scheduler_pool_->enqueue(std::move(job));
    if (enqueue_result.has_error()) {
        // Failed to enqueue - return immediate error
        if (metrics_) {
            metrics_->update_queued(-1);
            metrics_->record_acquisition_with_priority(priority, 0, false);
        }

        promise->set_value(error_info{
            -598,
            "Failed to enqueue connection request: " + enqueue_result.get_error().message(),
            "connection_pool_v2"
        });
    }

    return future;
#else
    // Fallback: Direct synchronous acquisition
    auto promise = std::make_shared<std::promise<Result<std::shared_ptr<connection_wrapper>>>>();
    auto future = promise->get_future();

    // Record start time for metrics
    auto start_time = std::chrono::steady_clock::now();

    if (metrics_) {
        metrics_->update_queued(1);
    }

    try {
        auto result = underlying_pool_->acquire_connection();

        // Calculate wait time
        auto end_time = std::chrono::steady_clock::now();
        auto wait_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time
        ).count();

        // Record metrics
        if (metrics_) {
            metrics_->update_queued(-1);

            if (result.is_ok()) {
                metrics_->record_acquisition(wait_time_us, true);
                metrics_->update_active(1);
            } else {
                metrics_->record_acquisition(wait_time_us, false);
            }
        }

        promise->set_value(std::move(result));
    } catch (const std::exception& e) {
        if (metrics_) {
            metrics_->update_queued(-1);
            metrics_->record_acquisition(0, false);
        }

        promise->set_value(error_info{
            -597,
            std::string("Exception in acquire_connection: ") + e.what(),
            "connection_pool_v2"
        });
    }

    return future;
#endif
}

void connection_pool_v2::release_connection(std::shared_ptr<connection_wrapper> connection) {
    if (underlying_pool_) {
        underlying_pool_->release_connection(std::move(connection));

        // Update active connection count
        if (metrics_) {
            metrics_->update_active(-1);
        }
    }
}

void connection_pool_v2::schedule_health_check() {
#ifdef USE_THREAD_SYSTEM
    if (!scheduler_pool_ || shutdown_requested_) {
        return;
    }

    // Create low-priority health check job
    auto job = std::make_unique<health_check_job<connection_priority>>(underlying_pool_);

    // Submit to scheduler (will be processed with HEALTH_CHECK priority)
    auto result = scheduler_pool_->enqueue(std::move(job));
    if (result.has_error()) {
        // Log error but don't throw - health checks are non-critical
        // In production, this would go to a logger
        // std::cerr << "Failed to schedule health check: " << result.get_error().message() << "\n";
    }
#else
    // Fallback: Direct synchronous health check
    if (underlying_pool_) {
        underlying_pool_->health_check();
    }
#endif
}

size_t connection_pool_v2::active_connections() const {
    if (underlying_pool_) {
        return underlying_pool_->active_connections();
    }
    return 0;
}

size_t connection_pool_v2::available_connections() const {
    if (underlying_pool_) {
        return underlying_pool_->available_connections();
    }
    return 0;
}

connection_stats connection_pool_v2::get_stats() const {
    if (underlying_pool_) {
        return underlying_pool_->get_stats();
    }
    return connection_stats{};
}

#ifdef USE_THREAD_SYSTEM
std::shared_ptr<monitoring::priority_metrics<connection_priority>>
connection_pool_v2::get_metrics() const {
    return metrics_;
}
#else
std::shared_ptr<monitoring::pool_metrics>
connection_pool_v2::get_metrics() const {
    return metrics_;
}
#endif

void connection_pool_v2::shutdown() {
    if (shutdown_requested_.exchange(true)) {
        return; // Already shutting down
    }

#ifdef USE_THREAD_SYSTEM
    // Stop scheduler pool first (stops accepting new jobs)
    if (scheduler_pool_) {
        scheduler_pool_->stop(false); // Graceful shutdown
    }
#endif

    // Shutdown underlying pool
    if (underlying_pool_) {
        underlying_pool_->shutdown();
    }
}

} // namespace database::pooling
