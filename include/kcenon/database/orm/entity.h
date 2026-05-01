// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/database_types.h>
#include <kcenon/database/core/database_backend.h>
#include <string>
#include <vector>
#include <memory>
#include <type_traits>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <optional>

namespace database::orm
{
	// Forward declarations
	class entity_base;
	class field_metadata;
	class entity_metadata;

	// Type traits for type safety (C++17 compatible)
	template<typename T, typename = void>
	struct is_entity : std::false_type {};

	template<typename T>
	struct is_entity<T, std::void_t<
		typename T::primary_key_type,
		decltype(std::declval<T>().table_name()),
		decltype(std::declval<T>().get_metadata())
	>> : std::is_convertible<decltype(std::declval<T>().table_name()), std::string> {};

	template<typename T>
	inline constexpr bool is_entity_v = is_entity<T>::value;

	template<typename T>
	inline constexpr bool is_field_type_v =
		std::is_same_v<T, int32_t> ||
		std::is_same_v<T, int64_t> ||
		std::is_same_v<T, double> ||
		std::is_same_v<T, std::string> ||
		std::is_same_v<T, bool> ||
		std::is_same_v<T, std::chrono::system_clock::time_point>;

	// Forward declaration with SFINAE constraint
	template<typename EntityType, typename = std::enable_if_t<is_entity_v<EntityType>>>
	class query_builder;

	// Field constraint types
	enum class field_constraint {
		none = 0,
		primary_key = 1,
		not_null = 2,
		unique = 4,
		auto_increment = 8,
		index = 16,
		foreign_key = 32,
		default_now = 64
	};

	inline field_constraint operator|(field_constraint a, field_constraint b) {
		return static_cast<field_constraint>(static_cast<int>(a) | static_cast<int>(b));
	}

	inline bool has_constraint(field_constraint constraints, field_constraint check) {
		return (static_cast<int>(constraints) & static_cast<int>(check)) != 0;
	}

	/**
	 * @class field_metadata
	 * @brief Metadata for entity fields including constraints and relationships.
	 */
	class field_metadata
	{
	public:
		field_metadata(const std::string& name,
		               const std::string& type_name,
		               field_constraint constraints = field_constraint::none,
		               const std::string& index_name = "",
		               const std::string& foreign_table = "",
		               const std::string& foreign_field = "");

		const std::string& name() const { return name_; }
		const std::string& type_name() const { return type_name_; }
		field_constraint constraints() const { return constraints_; }
		const std::string& index_name() const { return index_name_; }
		const std::string& foreign_table() const { return foreign_table_; }
		const std::string& foreign_field() const { return foreign_field_; }

		bool is_primary_key() const { return has_constraint(constraints_, field_constraint::primary_key); }
		bool is_not_null() const { return has_constraint(constraints_, field_constraint::not_null); }
		bool is_unique() const { return has_constraint(constraints_, field_constraint::unique); }
		bool is_auto_increment() const { return has_constraint(constraints_, field_constraint::auto_increment); }
		bool has_index() const { return has_constraint(constraints_, field_constraint::index); }
		bool is_foreign_key() const { return has_constraint(constraints_, field_constraint::foreign_key); }
		bool has_default_now() const { return has_constraint(constraints_, field_constraint::default_now); }

		std::string to_sql_definition() const;

	private:
		std::string name_;
		std::string type_name_;
		field_constraint constraints_;
		std::string index_name_;
		std::string foreign_table_;
		std::string foreign_field_;
	};

	/**
	 * @class entity_metadata
	 * @brief Metadata for entire entities including table mapping and relationships.
	 */
	class entity_metadata
	{
	public:
		entity_metadata(const std::string& table_name);

		void add_field(const field_metadata& field);
		const std::vector<field_metadata>& fields() const { return fields_; }
		const std::string& table_name() const { return table_name_; }

		const field_metadata* get_primary_key() const;
		std::vector<const field_metadata*> get_indexes() const;
		std::vector<const field_metadata*> get_foreign_keys() const;

		std::string create_table_sql() const;
		std::string create_indexes_sql() const;

	private:
		std::string table_name_;
		std::vector<field_metadata> fields_;
	};

	/**
	 * @class entity_base
	 * @brief Base class for all ORM entities.
	 */
	class entity_base
	{
	public:
		virtual ~entity_base() = default;
		virtual std::string table_name() const = 0;
		virtual const entity_metadata& get_metadata() const = 0;

		// CRUD operations
		virtual bool save() = 0;
		virtual bool load() = 0;
		virtual bool update() = 0;
		virtual bool remove() = 0;

	protected:
		entity_base() = default;
	};

	/**
	 * @class field_accessor
	 * @brief Template class for type-safe field access.
	 */
	template<typename T, typename = std::enable_if_t<is_field_type_v<T>>>
	class field_accessor
	{
	public:
		field_accessor(T& value, const field_metadata& metadata)
			: value_(value), metadata_(metadata) {}

		T& get() { return value_; }
		const T& get() const { return value_; }
		const field_metadata& metadata() const { return metadata_; }

		field_accessor& operator=(const T& value) {
			value_ = value;
			return *this;
		}

		operator T&() { return value_; }
		operator const T&() const { return value_; }

	private:
		T& value_;
		const field_metadata& metadata_;
	};

	/**
	 * @class query_builder
	 * @brief Template query builder for type-safe ORM queries.
	 */
	template<typename EntityType, typename>
	class query_builder
	{
	public:
		query_builder(std::shared_ptr<core::database_backend> db);

		// Query building methods
		query_builder& where(const std::string& condition);
		query_builder& order_by(const std::string& field, bool ascending = true);
		query_builder& limit(size_t count);
		query_builder& offset(size_t count);

		// Join operations
		template<typename OtherEntity>
		std::enable_if_t<is_entity_v<OtherEntity>, query_builder&> join(const std::string& condition);

		template<typename OtherEntity>
		std::enable_if_t<is_entity_v<OtherEntity>, query_builder&> left_join(const std::string& condition);

		// Execution methods
		std::vector<EntityType> execute();
		std::optional<EntityType> first();
		size_t count();

		// Aggregation methods
		double sum(const std::string& field);
		double avg(const std::string& field);
		core::database_value min(const std::string& field);
		core::database_value max(const std::string& field);

	private:
		std::shared_ptr<core::database_backend> db_;
		std::string where_clause_;
		std::string order_clause_;
		std::string join_clause_;
		size_t limit_count_ = 0;
		size_t offset_count_ = 0;

		std::string build_query() const;
		EntityType map_result_to_entity(const core::database_result& result, size_t row) const;
	};

	/**
	 * @class entity_manager
	 * @brief Manages entity metadata and provides factory methods.
	 *
	 * @note This class uses dependency injection pattern.
	 * Access via database_context::get_entity_manager() (Sprint 3, Task 3.1).
	 *
	 * @example
	 * @code
	 * auto context = std::make_shared<database_context>();
	 * auto entity_mgr = context->get_entity_manager();
	 * @endcode
	 */
	class entity_manager
	{
	public:
		/**
		 * @brief Default constructor - used by database_context
		 */
		entity_manager() = default;


		template<typename EntityType>
		std::enable_if_t<is_entity_v<EntityType>> register_entity();

		template<typename EntityType>
		std::enable_if_t<is_entity_v<EntityType>, const entity_metadata&> get_metadata();

		template<typename EntityType>
		std::enable_if_t<is_entity_v<EntityType>, query_builder<EntityType>> query(std::shared_ptr<core::database_backend> db);

		// Schema operations
		bool create_tables(std::shared_ptr<core::database_backend> db);
		bool drop_tables(std::shared_ptr<core::database_backend> db);
		bool sync_schema(std::shared_ptr<core::database_backend> db);

	private:
		std::unordered_map<std::string, std::unique_ptr<entity_metadata>> metadata_cache_;
	};

	// Helper macros for entity definition
	#define ENTITY_FIELD(type, name, ...) \
		private: \
			type name##_; \
			static inline field_metadata name##_metadata_{#name, #type, __VA_ARGS__}; \
		public: \
			field_accessor<type> name{name##_, name##_metadata_}; \
			static const field_metadata& name##_field() { return name##_metadata_; }

	#define ENTITY_TABLE(table_name) \
		public: \
			std::string table_name() const override { return table_name; } \
			using primary_key_type = decltype(id_); \
		private: \
			static inline entity_metadata metadata_{table_name};

	#define ENTITY_METADATA() \
		public: \
			const entity_metadata& get_metadata() const override { \
				static std::once_flag init_flag; \
				std::call_once(init_flag, [this]() { initialize_metadata(); }); \
				return metadata_; \
			} \
		private: \
			static void initialize_metadata();

	// Constraint helper functions
	inline field_constraint primary_key() { return field_constraint::primary_key; }
	inline field_constraint not_null() { return field_constraint::not_null; }
	inline field_constraint unique() { return field_constraint::unique; }
	inline field_constraint auto_increment() { return field_constraint::auto_increment; }
	inline field_constraint default_now() { return field_constraint::default_now; }

	inline field_constraint index(const std::string& name = "") {
		return field_constraint::index;
	}

	inline field_constraint foreign_key(const std::string& table, const std::string& field) {
		return field_constraint::foreign_key;
	}

} // namespace database::orm