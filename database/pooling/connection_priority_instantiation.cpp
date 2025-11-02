/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file connection_priority_instantiation.cpp
 * @brief Explicit template instantiations for connection_priority enum
 *
 * This file forces template instantiation by including the .cpp implementation
 * files from thread_system, then explicitly instantiating for connection_priority.
 *
 * This is required because typed_thread_pool is a template library and custom
 * enum types need explicit instantiation. Thread_system only instantiates for
 * job_types by default.
 */

#ifdef USE_THREAD_SYSTEM

// First, include the header with our custom enum
#include "connection_pool_v2.h"

// Now include the .cpp implementation files (not .h headers)
// This provides all the template function definitions
#include "impl/typed_pool/typed_thread_pool.cpp"
#include "impl/typed_pool/typed_thread_worker.cpp"
#include "impl/typed_pool/typed_job_queue.cpp"
#include "impl/typed_pool/typed_job.cpp"

// Explicitly instantiate only the templates we actually use
// Note: We only use typed_job_queue_t (not adaptive or lockfree variants)
namespace kcenon::thread {
    template class typed_thread_pool_t<database::pooling::connection_priority>;
    template class typed_thread_worker_t<database::pooling::connection_priority>;
    template class typed_job_queue_t<database::pooling::connection_priority>;
    template class typed_job_t<database::pooling::connection_priority>;
}

// Force instantiation of all member functions by calling them in a dead function
// This ensures vtables and all virtual function implementations are generated
namespace {
    void force_instantiation_of_all_members() {
        using priority_type = database::pooling::connection_priority;
        using pool_type = kcenon::thread::typed_thread_pool_t<priority_type>;
        using worker_type = kcenon::thread::typed_thread_worker_t<priority_type>;
        using job_queue_type = kcenon::thread::typed_job_queue_t<priority_type>;
        using typed_job_type = kcenon::thread::typed_job_t<priority_type>;

        // These function calls will never execute, but force the compiler
        // to instantiate all member functions including virtual ones
        if (false) {
            database::async::thread_context_type ctx;

            // Force pool instantiation
            pool_type pool("test", ctx);
            (void)pool.start();
            (void)pool.stop(false);
            (void)pool.get_job_queue();

            // Force job queue instantiation (including virtual functions)
            job_queue_type queue;
            std::unique_ptr<typed_job_type> job_ptr;
            (void)queue.enqueue(std::move(job_ptr));
            (void)queue.dequeue();
            (void)queue.dequeue_batch();
            (void)queue.stop();
            (void)queue.clear();
        }
    }
}

#endif // USE_THREAD_SYSTEM
