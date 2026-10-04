// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/core/backend_registry.h>
#include <kcenon/database/core/result.h>

#include <algorithm>

namespace kcenon::database
{
namespace core
{

backend_registry& backend_registry::instance()
{
	static backend_registry instance;
	return instance;
}

kcenon::common::VoidResult backend_registry::register_backend(const std::string& name,
                                                              backend_factory_fn factory)
{
	if (name.empty())
	{
		return kcenon::common::error_info{static_cast<int>(error_code::invalid_argument), "Backend name cannot be empty", "backend_registry"};
	}

	if (!factory)
	{
		return kcenon::common::error_info{static_cast<int>(error_code::invalid_argument), "Factory function cannot be null", "backend_registry"};
	}

	std::lock_guard<std::mutex> lock(mutex_);

	// Check if backend already registered
	if (factories_.find(name) != factories_.end())
	{
		return kcenon::common::error_info{static_cast<int>(error_code::invalid_state), "Backend '" + name + "' is already registered", "backend_registry"};
	}

	factories_[name] = factory;
	return kcenon::common::VoidResult::ok(std::monostate{});
}

kcenon::common::VoidResult backend_registry::unregister_backend(const std::string& name)
{
	std::lock_guard<std::mutex> lock(mutex_);

	auto it = factories_.find(name);
	if (it == factories_.end())
	{
		return kcenon::common::error_info{static_cast<int>(error_code::invalid_state), "Backend '" + name + "' not found", "backend_registry"};
	}

	factories_.erase(it);
	return kcenon::common::VoidResult::ok(std::monostate{});
}

std::unique_ptr<database_backend> backend_registry::create(const std::string& name) const
{
	std::lock_guard<std::mutex> lock(mutex_);

	auto it = factories_.find(name);
	if (it == factories_.end())
	{
		return nullptr;
	}

	try
	{
		return it->second();
	}
	catch (...)
	{
		// Factory function threw an exception
		return nullptr;
	}
}

bool backend_registry::has_backend(const std::string& name) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return factories_.find(name) != factories_.end();
}

std::vector<std::string> backend_registry::available_backends() const
{
	std::lock_guard<std::mutex> lock(mutex_);

	std::vector<std::string> names;
	names.reserve(factories_.size());

	for (const auto& pair : factories_)
	{
		names.push_back(pair.first);
	}

	// Sort for consistent output
	std::sort(names.begin(), names.end());

	return names;
}

size_t backend_registry::backend_count() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return factories_.size();
}

void backend_registry::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	factories_.clear();
}

} // namespace core
} // namespace kcenon::database
