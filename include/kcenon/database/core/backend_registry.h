// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file backend_registry.h
 * @brief Registry for database backend plugins
 *
 * Provides a centralized registry for registering and creating database backends
 * at runtime. This eliminates the need for conditional compilation and enables
 * dynamic backend selection.
 *
 * Key Features:
 * - Runtime backend registration
 * - Thread-safe factory pattern
 * - Automatic backend discovery via static initialization
 * - Support for both built-in and external backends
 *
 * Usage Pattern:
 * 1. Register backends (typically via static initialization):
 *    @code
 *    backend_registry::register_backend("postgresql", &postgresql_backend::create);
 *    @endcode
 *
 * 2. Create backend instances:
 *    @code
 *    auto backend = backend_registry::create("postgresql");
 *    if (!backend) {
 *        // Handle error: backend not found
 *    }
 *    @endcode
 *
 * 3. Query available backends:
 *    @code
 *    auto backends = backend_registry::available_backends();
 *    for (const auto& name : backends) {
 *        std::cout << "Available: " << name << std::endl;
 *    }
 *    @endcode
 */

#pragma once

#include <kcenon/database/core/database_backend.h>

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

namespace kcenon::database
{
namespace core
{

/**
 * @class backend_registry
 * @brief Singleton registry for database backend plugins
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex for synchronization
 * - Safe for concurrent registration and creation
 *
 * Singleton Pattern:
 * - Uses Meyer's singleton (thread-safe in C++11+)
 * - No manual memory management required
 * - Automatic cleanup on program exit
 */
class backend_registry
{
public:
	/**
	 * @brief Get the singleton instance
	 * @return Reference to the backend_registry instance
	 */
	static backend_registry& instance();

	// Prevent copying and moving
	backend_registry(const backend_registry&) = delete;
	backend_registry& operator=(const backend_registry&) = delete;
	backend_registry(backend_registry&&) = delete;
	backend_registry& operator=(backend_registry&&) = delete;

	/**
	 * @brief Register a backend factory function
	 * @param name Backend name (e.g., "postgresql", "sqlite")
	 * @param factory Factory function that creates backend instances
	 * @return VoidResult::ok() on success, error if name already registered
	 *
	 * Thread Safety: This method is thread-safe
	 *
	 * Example:
	 * @code
	 *   backend_registry::instance().register_backend(
	 *       "postgresql",
	 *       &postgresql_backend::create
	 *   );
	 * @endcode
	 */
	kcenon::common::VoidResult register_backend(const std::string& name, backend_factory_fn factory);

	/**
	 * @brief Unregister a backend (for testing or dynamic unloading)
	 * @param name Backend name to unregister
	 * @return VoidResult::ok() on success, error if not found
	 *
	 * Thread Safety: This method is thread-safe
	 */
	kcenon::common::VoidResult unregister_backend(const std::string& name);

	/**
	 * @brief Create a backend instance by name
	 * @param name Backend name (e.g., "postgresql", "sqlite")
	 * @return Unique pointer to backend, or nullptr if not found
	 *
	 * Thread Safety: This method is thread-safe
	 *
	 * Example:
	 * @code
	 *   auto backend = backend_registry::instance().create("postgresql");
	 *   if (!backend) {
	 *       std::cerr << "PostgreSQL backend not available" << std::endl;
	 *   }
	 * @endcode
	 */
	std::unique_ptr<database_backend> create(const std::string& name) const;

	/**
	 * @brief Check if a backend is registered
	 * @param name Backend name to check
	 * @return true if backend is registered
	 *
	 * Thread Safety: This method is thread-safe
	 */
	bool has_backend(const std::string& name) const;

	/**
	 * @brief Get list of all registered backend names
	 * @return Vector of backend names
	 *
	 * Thread Safety: This method is thread-safe
	 *
	 * Example:
	 * @code
	 *   auto backends = backend_registry::instance().available_backends();
	 *   for (const auto& name : backends) {
	 *       std::cout << "Available: " << name << std::endl;
	 *   }
	 * @endcode
	 */
	std::vector<std::string> available_backends() const;

	/**
	 * @brief Get number of registered backends
	 * @return Count of registered backends
	 *
	 * Thread Safety: This method is thread-safe
	 */
	size_t backend_count() const;

	/**
	 * @brief Clear all registered backends (for testing)
	 *
	 * Thread Safety: This method is thread-safe
	 *
	 * Warning: This should only be used in test code. It will remove
	 * all registered backends including built-in ones.
	 */
	void clear();

private:
	backend_registry() = default;
	~backend_registry() = default;

	mutable std::mutex mutex_;
	std::map<std::string, backend_factory_fn> factories_;
};

/**
 * @class backend_registrar
 * @brief Helper class for automatic backend registration
 *
 * This class uses static initialization to automatically register backends
 * at program startup. Each backend implementation should create a static
 * instance of this class.
 *
 * Example Usage in backend implementation:
 * @code
 *   // postgresql_backend.cpp
 *   namespace {
 *       backend_registrar<postgresql_backend> registrar("postgresql");
 *   }
 * @endcode
 *
 * This pattern ensures that all compiled-in backends are automatically
 * registered without explicit initialization code.
 */
template<typename BackendType>
class backend_registrar
{
public:
	/**
	 * @brief Constructor that registers the backend
	 * @param name Backend name for registration
	 */
	explicit backend_registrar(const std::string& name)
	{
		backend_registry::instance().register_backend(name, &BackendType::create);
	}
};

/**
 * @brief Convenience function for creating backends (static method style)
 * @param name Backend name
 * @return Unique pointer to backend, or nullptr if not found
 *
 * This is a convenience wrapper around backend_registry::instance().create()
 * for more concise code.
 *
 * Example:
 * @code
 *   auto backend = create_backend("postgresql");
 * @endcode
 */
inline std::unique_ptr<database_backend> create_backend(const std::string& name)
{
	return backend_registry::instance().create(name);
}

/**
 * @brief Convenience function for checking backend availability
 * @param name Backend name to check
 * @return true if backend is registered
 */
inline bool has_backend(const std::string& name)
{
	return backend_registry::instance().has_backend(name);
}

/**
 * @brief Convenience function for getting available backends
 * @return Vector of registered backend names
 */
inline std::vector<std::string> available_backends()
{
	return backend_registry::instance().available_backends();
}

} // namespace core
} // namespace kcenon::database
