// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "backend_registry.h"

#include <algorithm>

namespace database
{
namespace core
{

backend_registry& backend_registry::instance()
{
	static backend_registry instance;
	return instance;
}

database::VoidResult backend_registry::register_backend(const std::string& name,
                                                        backend_factory_fn factory)
{
	if (name.empty())
	{
		return database::VoidResult(database::error_info{1, "Backend name cannot be empty", "backend_registry"});
	}

	if (!factory)
	{
		return database::VoidResult(database::error_info{2, "Factory function cannot be null", "backend_registry"});
	}

	std::lock_guard<std::mutex> lock(mutex_);

	// Check if backend already registered
	if (factories_.find(name) != factories_.end())
	{
		return database::VoidResult(database::error_info{3, "Backend '" + name + "' is already registered", "backend_registry"});
	}

	factories_[name] = factory;
	return database::VoidResult(std::monostate{});
}

database::VoidResult backend_registry::unregister_backend(const std::string& name)
{
	std::lock_guard<std::mutex> lock(mutex_);

	auto it = factories_.find(name);
	if (it == factories_.end())
	{
		return database::VoidResult(database::error_info{4, "Backend '" + name + "' not found", "backend_registry"});
	}

	factories_.erase(it);
	return database::VoidResult(std::monostate{});
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
} // namespace database
