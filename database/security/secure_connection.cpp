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

#if __has_include(<openssl/evp.h>)
#define DATABASE_HAS_OPENSSL 1
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#else
#define DATABASE_HAS_OPENSSL 0
#endif

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

		// Generate new key
		std::string old_key = master_key_;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dist(33, 126);  // printable ASCII
		std::string new_key;
		new_key.reserve(32);
		for (int i = 0; i < 32; ++i)
		{
			new_key.push_back(static_cast<char>(dist(gen)));
		}

		// Re-encrypt all stored credentials with new key
		std::unordered_map<std::string, std::string> rotated;
		for (const auto& [id, encrypted] : encrypted_credentials_)
		{
			// Decrypt with old key
			std::string decrypted = decrypt_data(encrypted);
			if (decrypted.empty())
			{
				return false;  // Rotation failed — abort to preserve data
			}

			// Re-encrypt with new key (temporarily set new key)
			master_key_ = new_key;
			std::string re_encrypted = encrypt_data(decrypted);
			master_key_ = old_key;  // Restore old key in case of further failures

			if (re_encrypted.empty())
			{
				return false;
			}
			rotated[id] = re_encrypted;
		}

		// Commit: swap all credentials and update key
		encrypted_credentials_ = std::move(rotated);
		master_key_ = new_key;
		return true;
	}

	namespace
	{
		std::string bytes_to_hex(const std::vector<uint8_t>& bytes)
		{
			std::ostringstream oss;
			for (uint8_t b : bytes)
			{
				oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
			}
			return oss.str();
		}

		std::vector<uint8_t> hex_to_bytes(const std::string& hex)
		{
			std::vector<uint8_t> bytes;
			bytes.reserve(hex.size() / 2);
			for (size_t i = 0; i + 1 < hex.size(); i += 2)
			{
				unsigned int val = 0;
				std::istringstream iss(hex.substr(i, 2));
				iss >> std::hex >> val;
				bytes.push_back(static_cast<uint8_t>(val));
			}
			return bytes;
		}
	} // anonymous namespace

	std::string credential_manager::hash_password(const std::string& password) const
	{
		if (password.empty())
		{
			return {};
		}

#if DATABASE_HAS_OPENSSL
		// PBKDF2-HMAC-SHA256: cryptographically secure password hashing
		constexpr int iterations = 100000;
		constexpr int salt_len = 16;
		constexpr int hash_len = 32;

		std::vector<uint8_t> salt(salt_len);
		RAND_bytes(salt.data(), salt_len);

		std::vector<uint8_t> hash(hash_len);
		PKCS5_PBKDF2_HMAC(
			password.c_str(), static_cast<int>(password.size()),
			salt.data(), salt_len,
			iterations,
			EVP_sha256(),
			hash_len, hash.data()
		);

		// Format: "pbkdf2:<iterations>:<salt_hex>:<hash_hex>"
		std::ostringstream oss;
		oss << "pbkdf2:" << iterations << ":" << bytes_to_hex(salt) << ":" << bytes_to_hex(hash);
		return oss.str();
#else
		// WARNING: FNV1a is NOT cryptographically secure.
		// Build with OpenSSL to enable PBKDF2-HMAC-SHA256 password hashing.
		static bool warned = false;
		if (!warned)
		{
			std::cerr << "[database_system] WARNING: Using FNV1a placeholder for password hashing. "
			          << "Build with OpenSSL for PBKDF2-HMAC-SHA256 support.\n";
			warned = true;
		}

		uint64_t hash = 0xcbf29ce484222325ULL;
		for (char c : password)
		{
			hash ^= static_cast<uint64_t>(c);
			hash *= 0x100000001b3ULL;
		}

		std::ostringstream oss;
		oss << std::hex << std::setfill('0') << std::setw(16) << hash;
		return "fnv1a:" + oss.str();
#endif
	}

	bool credential_manager::verify_password(const std::string& password,
	                                          const std::string& hash) const
	{
		if (password.empty() || hash.empty())
		{
			return false;
		}

#if DATABASE_HAS_OPENSSL
		// Parse format: "pbkdf2:<iterations>:<salt_hex>:<hash_hex>"
		if (hash.substr(0, 7) == "pbkdf2:")
		{
			auto first_colon = hash.find(':', 7);
			auto second_colon = hash.find(':', first_colon + 1);
			if (first_colon == std::string::npos || second_colon == std::string::npos)
			{
				return false;
			}

			int iterations = std::stoi(hash.substr(7, first_colon - 7));
			auto salt = hex_to_bytes(hash.substr(first_colon + 1, second_colon - first_colon - 1));
			auto expected_hash = hex_to_bytes(hash.substr(second_colon + 1));

			std::vector<uint8_t> computed(expected_hash.size());
			PKCS5_PBKDF2_HMAC(
				password.c_str(), static_cast<int>(password.size()),
				salt.data(), static_cast<int>(salt.size()),
				iterations,
				EVP_sha256(),
				static_cast<int>(computed.size()), computed.data()
			);

			return computed == expected_hash;
		}

		// Legacy FNV1a format migration: verify with old algorithm
		if (hash.substr(0, 6) == "fnv1a:")
		{
			uint64_t h = 0xcbf29ce484222325ULL;
			for (char c : password)
			{
				h ^= static_cast<uint64_t>(c);
				h *= 0x100000001b3ULL;
			}
			std::ostringstream oss;
			oss << std::hex << std::setfill('0') << std::setw(16) << h;
			return hash == ("fnv1a:" + oss.str());
		}
#endif

		// Fallback: direct comparison (covers fnv1a format without OpenSSL)
		return hash_password(password) == hash;
	}

	std::string credential_manager::encrypt_data(const std::string& data) const
	{
		if (data.empty())
		{
			return {};
		}

#if DATABASE_HAS_OPENSSL
		// AES-256-GCM encryption
		std::string key = master_key_.empty() ? "default_key_placeholder!!" : master_key_;
		// Pad or truncate key to 32 bytes for AES-256
		key.resize(32, '\0');

		constexpr int iv_len = 12;
		constexpr int tag_len = 16;

		std::vector<uint8_t> iv(iv_len);
		RAND_bytes(iv.data(), iv_len);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx) return {};

		std::vector<uint8_t> ciphertext(data.size() + EVP_MAX_BLOCK_LENGTH);
		std::vector<uint8_t> tag(tag_len);
		int len = 0, ciphertext_len = 0;

		EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, nullptr);
		EVP_EncryptInit_ex(ctx, nullptr, nullptr,
		                   reinterpret_cast<const unsigned char*>(key.data()), iv.data());
		EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
		                  reinterpret_cast<const unsigned char*>(data.data()),
		                  static_cast<int>(data.size()));
		ciphertext_len = len;
		EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
		ciphertext_len += len;
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag_len, tag.data());
		EVP_CIPHER_CTX_free(ctx);

		ciphertext.resize(ciphertext_len);

		// Format: "aes:<iv_hex>:<ciphertext_hex>:<tag_hex>"
		return "aes:" + bytes_to_hex(iv) + ":" + bytes_to_hex(ciphertext) + ":" + bytes_to_hex(tag);
#else
		// WARNING: XOR obfuscation is NOT cryptographically secure.
		static bool warned = false;
		if (!warned)
		{
			std::cerr << "[database_system] WARNING: Using XOR placeholder for data encryption. "
			          << "Build with OpenSSL for AES-256-GCM support.\n";
			warned = true;
		}

		std::string key = master_key_.empty() ? "default_key" : master_key_;
		std::string result = data;
		for (size_t i = 0; i < result.size(); ++i)
		{
			result[i] ^= key[i % key.size()];
		}

		std::ostringstream oss;
		for (unsigned char c : result)
		{
			oss << std::hex << std::setfill('0') << std::setw(2)
			    << static_cast<int>(c);
		}
		return "xor:" + oss.str();
#endif
	}

	std::string credential_manager::decrypt_data(const std::string& encrypted_data) const
	{
		if (encrypted_data.empty())
		{
			return {};
		}

#if DATABASE_HAS_OPENSSL
		// AES-256-GCM decryption: parse "aes:<iv_hex>:<ciphertext_hex>:<tag_hex>"
		if (encrypted_data.substr(0, 4) == "aes:")
		{
			auto first = encrypted_data.find(':', 4);
			auto second = encrypted_data.find(':', first + 1);
			if (first == std::string::npos || second == std::string::npos)
			{
				return {};
			}

			auto iv = hex_to_bytes(encrypted_data.substr(4, first - 4));
			auto ciphertext = hex_to_bytes(encrypted_data.substr(first + 1, second - first - 1));
			auto tag = hex_to_bytes(encrypted_data.substr(second + 1));

			std::string key = master_key_.empty() ? "default_key_placeholder!!" : master_key_;
			key.resize(32, '\0');

			EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
			if (!ctx) return {};

			std::vector<uint8_t> plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH);
			int len = 0, plaintext_len = 0;

			EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
			EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr);
			EVP_DecryptInit_ex(ctx, nullptr, nullptr,
			                   reinterpret_cast<const unsigned char*>(key.data()), iv.data());
			EVP_DecryptUpdate(ctx, plaintext.data(), &len,
			                  ciphertext.data(), static_cast<int>(ciphertext.size()));
			plaintext_len = len;
			EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()),
			                    const_cast<uint8_t*>(tag.data()));

			int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
			EVP_CIPHER_CTX_free(ctx);

			if (ret <= 0) return {};  // Authentication failed
			plaintext_len += len;

			return std::string(plaintext.begin(), plaintext.begin() + plaintext_len);
		}
#endif

		// Legacy XOR format (with or without "xor:" prefix)
		std::string hex_data = encrypted_data;
		if (hex_data.substr(0, 4) == "xor:")
		{
			hex_data = hex_data.substr(4);
		}

		std::string decoded;
		decoded.reserve(hex_data.size() / 2);
		for (size_t i = 0; i + 1 < hex_data.size(); i += 2)
		{
			unsigned int byte = 0;
			std::istringstream iss(hex_data.substr(i, 2));
			iss >> std::hex >> byte;
			decoded.push_back(static_cast<char>(byte));
		}

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

#if DATABASE_HAS_OPENSSL
		// AES-256-GCM field encryption with derived key
		key.resize(32, '\0');  // Ensure 256-bit key

		constexpr int iv_len = 12;
		constexpr int tag_len = 16;

		std::vector<uint8_t> iv(iv_len);
		RAND_bytes(iv.data(), iv_len);

		EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
		if (!ctx) return {};

		std::vector<uint8_t> ciphertext(data.size() + EVP_MAX_BLOCK_LENGTH);
		std::vector<uint8_t> tag(tag_len);
		int len = 0, ct_len = 0;

		EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, nullptr);
		EVP_EncryptInit_ex(ctx, nullptr, nullptr,
		                   reinterpret_cast<const unsigned char*>(key.data()), iv.data());
		EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
		                  reinterpret_cast<const unsigned char*>(data.data()),
		                  static_cast<int>(data.size()));
		ct_len = len;
		EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
		ct_len += len;
		EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag_len, tag.data());
		EVP_CIPHER_CTX_free(ctx);

		ciphertext.resize(ct_len);
		return "aes:" + bytes_to_hex(iv) + ":" + bytes_to_hex(ciphertext) + ":" + bytes_to_hex(tag);
#else
		// XOR-based field encryption (placeholder)
		std::string result = data;
		for (size_t i = 0; i < result.size(); ++i)
		{
			result[i] ^= key[i % key.size()];
		}

		std::ostringstream oss;
		for (unsigned char c : result)
		{
			oss << std::hex << std::setfill('0') << std::setw(2)
			    << static_cast<int>(c);
		}
		return "xor:" + oss.str();
#endif
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

#if DATABASE_HAS_OPENSSL
		// AES-256-GCM decryption
		if (encrypted_data.substr(0, 4) == "aes:")
		{
			key.resize(32, '\0');

			auto first = encrypted_data.find(':', 4);
			auto second = encrypted_data.find(':', first + 1);
			if (first == std::string::npos || second == std::string::npos) return {};

			auto iv = hex_to_bytes(encrypted_data.substr(4, first - 4));
			auto ciphertext = hex_to_bytes(encrypted_data.substr(first + 1, second - first - 1));
			auto tag = hex_to_bytes(encrypted_data.substr(second + 1));

			EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
			if (!ctx) return {};

			std::vector<uint8_t> plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH);
			int len = 0, pt_len = 0;

			EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
			EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr);
			EVP_DecryptInit_ex(ctx, nullptr, nullptr,
			                   reinterpret_cast<const unsigned char*>(key.data()), iv.data());
			EVP_DecryptUpdate(ctx, plaintext.data(), &len,
			                  ciphertext.data(), static_cast<int>(ciphertext.size()));
			pt_len = len;
			EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()),
			                    const_cast<uint8_t*>(tag.data()));

			int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
			EVP_CIPHER_CTX_free(ctx);

			if (ret <= 0) return {};
			pt_len += len;
			return std::string(plaintext.begin(), plaintext.begin() + pt_len);
		}
#endif

		// Legacy XOR format
		std::string hex_data = encrypted_data;
		if (hex_data.substr(0, 4) == "xor:")
		{
			hex_data = hex_data.substr(4);
		}

		std::string decoded;
		decoded.reserve(hex_data.size() / 2);
		for (size_t i = 0; i + 1 < hex_data.size(); i += 2)
		{
			unsigned int byte = 0;
			std::istringstream iss(hex_data.substr(i, 2));
			iss >> std::hex >> byte;
			decoded.push_back(static_cast<char>(byte));
		}

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
