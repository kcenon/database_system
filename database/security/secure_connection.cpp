// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "secure_connection.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace database::security
{

	// ─────────────────────────────────────────────
	// credential_manager
	// ─────────────────────────────────────────────

	bool credential_manager::store_credentials(const std::string& connection_id,
	                                           const security_credentials& credentials)
	{
		std::lock_guard<std::mutex> lock(credentials_mutex_);

		// Serialize credentials to a simple format and encrypt
		std::ostringstream oss;
		oss << credentials.username << "\n"
		    << credentials.password_hash << "\n"
		    << credentials.certificate_path << "\n"
		    << credentials.private_key_path << "\n"
		    << credentials.ca_cert_path << "\n"
		    << static_cast<int>(credentials.auth_method) << "\n"
		    << static_cast<int>(credentials.encryption);

		std::string serialized = oss.str();
		std::string encrypted = encrypt_data(serialized);
		encrypted_credentials_[connection_id] = encrypted;
		return true;
	}

	std::optional<security_credentials> credential_manager::get_credentials(
		const std::string& connection_id) const
	{
		std::lock_guard<std::mutex> lock(credentials_mutex_);

		auto it = encrypted_credentials_.find(connection_id);
		if (it == encrypted_credentials_.end())
		{
			return std::nullopt;
		}

		std::string decrypted = decrypt_data(it->second);
		if (decrypted.empty())
		{
			return std::nullopt;
		}

		// Deserialize credentials
		std::istringstream iss(decrypted);
		security_credentials creds;
		std::getline(iss, creds.username);
		std::getline(iss, creds.password_hash);
		std::getline(iss, creds.certificate_path);
		std::getline(iss, creds.private_key_path);
		std::getline(iss, creds.ca_cert_path);

		int auth_method_int = 0;
		int encryption_int = 0;
		iss >> auth_method_int >> encryption_int;
		creds.auth_method = static_cast<authentication_method>(auth_method_int);
		creds.encryption = static_cast<encryption_type>(encryption_int);

		return creds;
	}

	bool credential_manager::remove_credentials(const std::string& connection_id)
	{
		std::lock_guard<std::mutex> lock(credentials_mutex_);
		return encrypted_credentials_.erase(connection_id) > 0;
	}

	void credential_manager::set_master_key(const std::string& key)
	{
		std::lock_guard<std::mutex> lock(credentials_mutex_);
		master_key_ = key;
	}

	bool credential_manager::rotate_encryption_keys()
	{
		std::lock_guard<std::mutex> lock(credentials_mutex_);

		if (master_key_.empty())
		{
			return false;
		}

		// Decrypt all credentials with old key, then re-encrypt with new key
		// For now, this is a placeholder — actual rotation requires a new key input
		return true;
	}

	std::string credential_manager::hash_password(const std::string& password) const
	{
		// XOR-based obfuscation as a placeholder.
		// Production systems should use bcrypt/Argon2 via a cryptographic library.
		if (password.empty())
		{
			return {};
		}

		// Simple hash: SHA-256-like mixing (deterministic, NOT cryptographically secure)
		uint64_t hash = 0xcbf29ce484222325ULL; // FNV offset basis
		for (char c : password)
		{
			hash ^= static_cast<uint64_t>(c);
			hash *= 0x100000001b3ULL; // FNV prime
		}

		std::ostringstream oss;
		oss << std::hex << std::setfill('0') << std::setw(16) << hash;
		return "fnv1a:" + oss.str();
	}

	bool credential_manager::verify_password(const std::string& password,
	                                          const std::string& hash) const
	{
		return hash_password(password) == hash;
	}

	std::string credential_manager::encrypt_data(const std::string& data) const
	{
		if (data.empty())
		{
			return {};
		}

		// XOR-based obfuscation with master key.
		// This is NOT cryptographically secure — it prevents plaintext storage
		// but should be replaced with AES-256-GCM when a crypto backend is integrated.
		std::string key = master_key_.empty() ? "default_key" : master_key_;
		std::string result = data;

		for (size_t i = 0; i < result.size(); ++i)
		{
			result[i] ^= key[i % key.size()];
		}

		// Encode as hex string for safe storage
		std::ostringstream oss;
		for (unsigned char c : result)
		{
			oss << std::hex << std::setfill('0') << std::setw(2)
			    << static_cast<int>(c);
		}
		return oss.str();
	}

	std::string credential_manager::decrypt_data(const std::string& encrypted_data) const
	{
		if (encrypted_data.empty())
		{
			return {};
		}

		// Decode hex string
		std::string decoded;
		decoded.reserve(encrypted_data.size() / 2);
		for (size_t i = 0; i + 1 < encrypted_data.size(); i += 2)
		{
			unsigned int byte = 0;
			std::istringstream iss(encrypted_data.substr(i, 2));
			iss >> std::hex >> byte;
			decoded.push_back(static_cast<char>(byte));
		}

		// XOR decrypt with master key
		std::string key = master_key_.empty() ? "default_key" : master_key_;
		for (size_t i = 0; i < decoded.size(); ++i)
		{
			decoded[i] ^= key[i % key.size()];
		}

		return decoded;
	}

	// ─────────────────────────────────────────────
	// encryption_manager
	// ─────────────────────────────────────────────

	std::string encryption_manager::encrypt_field_data(const std::string& data,
	                                                    const std::string& field_name) const
	{
		std::lock_guard<std::mutex> lock(encryption_mutex_);

		if (data.empty())
		{
			return {};
		}

		std::string key = derive_key(field_name);
		if (key.empty())
		{
			return {};
		}

		// XOR-based field encryption (placeholder for AES-256)
		std::string result = data;
		for (size_t i = 0; i < result.size(); ++i)
		{
			result[i] ^= key[i % key.size()];
		}

		// Hex-encode
		std::ostringstream oss;
		for (unsigned char c : result)
		{
			oss << std::hex << std::setfill('0') << std::setw(2)
			    << static_cast<int>(c);
		}
		return oss.str();
	}

	std::string encryption_manager::decrypt_field_data(const std::string& encrypted_data,
	                                                    const std::string& field_name) const
	{
		std::lock_guard<std::mutex> lock(encryption_mutex_);

		if (encrypted_data.empty())
		{
			return {};
		}

		std::string key = derive_key(field_name);
		if (key.empty())
		{
			return {};
		}

		// Hex-decode
		std::string decoded;
		decoded.reserve(encrypted_data.size() / 2);
		for (size_t i = 0; i + 1 < encrypted_data.size(); i += 2)
		{
			unsigned int byte = 0;
			std::istringstream iss(encrypted_data.substr(i, 2));
			iss >> std::hex >> byte;
			decoded.push_back(static_cast<char>(byte));
		}

		// XOR decrypt
		for (size_t i = 0; i < decoded.size(); ++i)
		{
			decoded[i] ^= key[i % key.size()];
		}

		return decoded;
	}

	bool encryption_manager::generate_field_key(const std::string& field_name)
	{
		std::lock_guard<std::mutex> lock(encryption_mutex_);

		// Generate a random key for the field
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);

		std::string key(32, '\0');
		for (auto& c : key)
		{
			c = static_cast<char>(dis(gen));
		}

		// Hex-encode the key for storage
		std::ostringstream oss;
		for (unsigned char c : key)
		{
			oss << std::hex << std::setfill('0') << std::setw(2)
			    << static_cast<int>(c);
		}
		field_keys_[field_name] = oss.str();
		return true;
	}

	bool encryption_manager::rotate_field_key(const std::string& field_name)
	{
		std::lock_guard<std::mutex> lock(encryption_mutex_);

		auto it = field_keys_.find(field_name);
		if (it == field_keys_.end())
		{
			return false;
		}

		// Generate new key (in production, would re-encrypt existing data)
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);

		std::string key(32, '\0');
		for (auto& c : key)
		{
			c = static_cast<char>(dis(gen));
		}

		std::ostringstream oss;
		for (unsigned char c : key)
		{
			oss << std::hex << std::setfill('0') << std::setw(2)
			    << static_cast<int>(c);
		}
		it->second = oss.str();
		return true;
	}

	void encryption_manager::set_master_encryption_key(const std::string& key)
	{
		std::lock_guard<std::mutex> lock(encryption_mutex_);
		master_key_ = key;
	}

	bool encryption_manager::configure_encrypted_column(const std::string& table,
	                                                     const std::string& column,
	                                                     encryption_type type)
	{
		std::lock_guard<std::mutex> lock(encryption_mutex_);
		std::string key = table + "." + column;
		encrypted_columns_[key] = type;

		// Auto-generate field key if not present
		if (field_keys_.find(key) == field_keys_.end())
		{
			// Temporarily release lock to call generate_field_key
			// Instead, inline the key generation
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(0, 255);

			std::string field_key(32, '\0');
			for (auto& c : field_key)
			{
				c = static_cast<char>(dis(gen));
			}

			std::ostringstream oss;
			for (unsigned char ch : field_key)
			{
				oss << std::hex << std::setfill('0') << std::setw(2)
				    << static_cast<int>(ch);
			}
			field_keys_[key] = oss.str();
		}

		return true;
	}

	bool encryption_manager::is_column_encrypted(const std::string& table,
	                                              const std::string& column) const
	{
		std::lock_guard<std::mutex> lock(encryption_mutex_);
		std::string key = table + "." + column;
		return encrypted_columns_.find(key) != encrypted_columns_.end();
	}

	std::string encryption_manager::derive_key(const std::string& field_name) const
	{
		// Check for field-specific key first
		auto it = field_keys_.find(field_name);
		if (it != field_keys_.end())
		{
			return it->second;
		}

		// Fall back to master key
		if (!master_key_.empty())
		{
			return master_key_;
		}

		return {};
	}

	// ─────────────────────────────────────────────
	// audit_logger
	// ─────────────────────────────────────────────

	audit_logger::audit_logger(const std::string& log_file_path)
		: log_file_path_(log_file_path)
	{
	}

	void audit_logger::persist_entry(const audit_log_entry& entry)
	{
		if (log_file_path_.empty())
		{
			return;
		}

		std::ofstream file(log_file_path_, std::ios::app);
		if (!file.is_open())
		{
			return;
		}

		auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
		file << time_t << ","
		     << entry.user_id << ","
		     << entry.session_id << ","
		     << entry.operation << ","
		     << entry.table_name << ","
		     << entry.query_hash << ","
		     << (entry.success ? "true" : "false") << ","
		     << entry.error_message << ","
		     << entry.client_ip << ","
		     << entry.user_agent << "\n";
		file.flush();
	}

	void audit_logger::log_database_access(const std::string& user_id,
	                                        const std::string& session_id,
	                                        const std::string& operation,
	                                        const std::string& table,
	                                        const std::string& query_hash,
	                                        bool success,
	                                        const std::string& error_message)
	{
		audit_log_entry entry;
		entry.timestamp = std::chrono::system_clock::now();
		entry.user_id = user_id;
		entry.session_id = session_id;
		entry.operation = operation;
		entry.table_name = table;
		entry.query_hash = query_hash;
		entry.success = success;
		entry.error_message = error_message;

		std::lock_guard<std::mutex> lock(audit_mutex_);
		audit_logs_.push_back(entry);
		persist_entry(entry);
	}

	void audit_logger::log_authentication_event(const std::string& user_id,
	                                             const std::string& client_ip,
	                                             bool success,
	                                             const std::string& method)
	{
		audit_log_entry entry;
		entry.timestamp = std::chrono::system_clock::now();
		entry.user_id = user_id;
		entry.client_ip = client_ip;
		entry.operation = "authentication";
		entry.success = success;
		entry.error_message = success ? "" : "Authentication failed via " + method;

		std::lock_guard<std::mutex> lock(audit_mutex_);
		audit_logs_.push_back(entry);
		persist_entry(entry);
	}

	void audit_logger::log_authorization_failure(const std::string& user_id,
	                                              const std::string& operation,
	                                              const std::string& table,
	                                              const std::string& reason)
	{
		audit_log_entry entry;
		entry.timestamp = std::chrono::system_clock::now();
		entry.user_id = user_id;
		entry.operation = "authorization_failure:" + operation;
		entry.table_name = table;
		entry.success = false;
		entry.error_message = reason;

		std::lock_guard<std::mutex> lock(audit_mutex_);
		audit_logs_.push_back(entry);
		persist_entry(entry);
	}

	std::vector<audit_log_entry> audit_logger::get_audit_logs(
		std::chrono::hours window) const
	{
		std::lock_guard<std::mutex> lock(audit_mutex_);

		auto cutoff = std::chrono::system_clock::now() - window;
		std::vector<audit_log_entry> result;

		for (const auto& entry : audit_logs_)
		{
			if (entry.timestamp >= cutoff)
			{
				result.push_back(entry);
			}
		}

		return result;
	}

	std::vector<audit_log_entry> audit_logger::get_user_audit_logs(
		const std::string& user_id, std::chrono::hours window) const
	{
		std::lock_guard<std::mutex> lock(audit_mutex_);

		auto cutoff = std::chrono::system_clock::now() - window;
		std::vector<audit_log_entry> result;

		for (const auto& entry : audit_logs_)
		{
			if (entry.timestamp >= cutoff && entry.user_id == user_id)
			{
				result.push_back(entry);
			}
		}

		return result;
	}

	std::string audit_logger::generate_security_report(
		std::chrono::hours window) const
	{
		auto logs = get_audit_logs(window);

		size_t total = logs.size();
		size_t failures = 0;
		size_t auth_events = 0;

		for (const auto& entry : logs)
		{
			if (!entry.success)
			{
				++failures;
			}
			if (entry.operation == "authentication")
			{
				++auth_events;
			}
		}

		std::ostringstream oss;
		oss << "Security Report (last " << window.count() << " hours)\n"
		    << "  Total events: " << total << "\n"
		    << "  Failures: " << failures << "\n"
		    << "  Auth events: " << auth_events << "\n"
		    << "  Failure rate: "
		    << (total > 0 ? (100.0 * failures / total) : 0.0) << "%\n";

		return oss.str();
	}

	std::vector<std::string> audit_logger::detect_suspicious_activity(
		std::chrono::hours window) const
	{
		auto logs = get_audit_logs(window);
		std::vector<std::string> alerts;

		// Detect users with high failure rates
		std::unordered_map<std::string, size_t> user_failures;
		for (const auto& entry : logs)
		{
			if (!entry.success)
			{
				++user_failures[entry.user_id];
			}
		}

		for (const auto& [user_id, count] : user_failures)
		{
			if (count >= 5)
			{
				alerts.push_back("User '" + user_id + "' has " +
				                 std::to_string(count) +
				                 " failed operations in the last " +
				                 std::to_string(window.count()) + " hours");
			}
		}

		return alerts;
	}

	void audit_logger::set_log_retention_period(std::chrono::hours retention)
	{
		std::lock_guard<std::mutex> lock(audit_mutex_);
		retention_period_ = retention;
	}

	void audit_logger::cleanup_old_logs()
	{
		std::lock_guard<std::mutex> lock(audit_mutex_);

		auto cutoff = std::chrono::system_clock::now() - retention_period_;
		audit_logs_.erase(
			std::remove_if(audit_logs_.begin(), audit_logs_.end(),
			               [&cutoff](const audit_log_entry& entry) {
				               return entry.timestamp < cutoff;
			               }),
			audit_logs_.end());
	}

	bool audit_logger::export_logs_to_file(const std::string& filename) const
	{
		std::lock_guard<std::mutex> lock(audit_mutex_);

		std::ofstream file(filename);
		if (!file.is_open())
		{
			return false;
		}

		// Write CSV header
		file << "timestamp,user_id,session_id,operation,table_name,"
		        "query_hash,success,error_message,client_ip,user_agent\n";

		for (const auto& entry : audit_logs_)
		{
			auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
			file << time_t << ","
			     << entry.user_id << ","
			     << entry.session_id << ","
			     << entry.operation << ","
			     << entry.table_name << ","
			     << entry.query_hash << ","
			     << (entry.success ? "true" : "false") << ","
			     << entry.error_message << ","
			     << entry.client_ip << ","
			     << entry.user_agent << "\n";
		}

		return true;
	}

} // namespace database::security
