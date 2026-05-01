// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file backend_base.h
 * @brief CRTP template base class for database backends
 *
 * Provides a template base class that eliminates duplicated lifecycle patterns
 * across all database backend implementations. Uses CRTP (Curiously Recurring
 * Template Pattern) for static polymorphism.
 *
 * Issue #328: Create template base class for backend lifecycle
 *
 * Design Goals:
 * - Eliminate ~150 lines of boilerplate per backend
 * - Centralize common lifecycle logic (constructor, destructor, create, type)
 * - Standardize initialization guard checks
 * - Allow backends to focus on database-specific implementation
 *
 * Usage Pattern:
 * @code
 *   class postgresql_backend
 *       : public backend_base<postgresql_backend, database_types::postgres> {
 *   public:
 *       static constexpr const char* backend_name() { return "postgresql_backend"; }
 *
 *   protected:
 *       friend class backend_base<postgresql_backend, database_types::postgres>;
 *       VoidResult do_initialize(const connection_config& config);
 *       VoidResult do_shutdown();
 *   };
 * @endcode
 */

#pragma once

#include <kcenon/database/core/database_backend.h>
#include <kcenon/database/core/result.h>
#include <kcenon/database/database_types.h>

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <atomic>

namespace database
{
namespace core
{

/**
 * @class backend_base
 * @brief CRTP template base class for database backends
 *
 * This template class implements the common lifecycle pattern shared by all
 * database backends, reducing code duplication and ensuring consistent behavior.
 *
 * @tparam Derived The derived backend class (CRTP pattern)
 * @tparam Type The database_types enum value for this backend
 *
 * Template Parameters:
 * - Derived: Must implement:
 *   - static constexpr const char* backend_name()
 *   - VoidResult do_initialize(const connection_config&)
 *   - VoidResult do_shutdown()
 *
 * Thread Safety:
 * - initialized_ is atomic for thread-safe state queries
 * - Derived classes must handle their own synchronization for connection access
 */
template<typename Derived, database_types Type>
class backend_base : public database_backend
{
public:
	/**
	 * @brief Default constructor
	 *
	 * Initializes the backend in an uninitialized state.
	 */
	backend_base() = default;

	/**
	 * @brief Virtual destructor
	 *
	 * Calls shutdown() to ensure proper cleanup of derived class resources.
	 */
	~backend_base() override
	{
		shutdown();
	}

	// Prevent copying (backends own unique resources)
	backend_base(const backend_base&) = delete;
	backend_base& operator=(const backend_base&) = delete;

	// Prevent moving (std::atomic members are not moveable)
	backend_base(backend_base&&) noexcept = delete;
	backend_base& operator=(backend_base&&) noexcept = delete;

	/**
	 * @brief Factory method for backend_registry
	 * @return Unique pointer to new backend instance
	 */
	static std::unique_ptr<database_backend> create()
	{
		return std::make_unique<Derived>();
	}

	/**
	 * @brief Get the database type of this backend
	 * @return Database type identifier from template parameter
	 */
	database_types type() const override
	{
		return Type;
	}

	/**
	 * @brief Initialize the database backend
	 * @param config Connection configuration
	 * @return VoidResult::ok() on success, error on failure
	 *
	 * Performs initialization guard check, then delegates to derived class.
	 * Derived class must implement do_initialize() for database-specific logic.
	 */
	kcenon::common::VoidResult initialize(const connection_config& config) override
	{
		if (initialized_) {
			return kcenon::common::error_info{
				static_cast<int>(database::error_code::invalid_state),
				"Backend already initialized",
				Derived::backend_name()
			};
		}

		auto result = static_cast<Derived*>(this)->do_initialize(config);
		if (result.is_ok()) {
			initialized_ = true;
		}
		return result;
	}

	/**
	 * @brief Shutdown the database backend gracefully
	 * @return VoidResult::ok() on success, error on failure
	 *
	 * Performs shutdown guard check, then delegates to derived class.
	 * Derived class must implement do_shutdown() for database-specific cleanup.
	 */
	kcenon::common::VoidResult shutdown() override
	{
		if (!initialized_) {
			return kcenon::common::ok();
		}

		auto result = static_cast<Derived*>(this)->do_shutdown();
		initialized_ = false;
		return result;
	}

	/**
	 * @brief Check if backend is initialized and ready
	 * @return true if initialized and can accept queries
	 */
	bool is_initialized() const override
	{
		return initialized_;
	}

protected:
	std::atomic<bool> initialized_{false}; ///< Initialization state
};

} // namespace core
} // namespace database
