#pragma once

#include <chrono>
#include <datapod/datapod.hpp>
#include <keylock/keylock.hpp>
#include <optional>
#include <vector>

namespace blockit {

    /// Ed25519 keypair identity for PoA validators
    /// Header-only implementation using keylock for crypto operations
    class Key {
      public:
        /// Generate new Ed25519 keypair (permanent, no expiration)
        inline static dp::Result<Key, dp::Error> generate() {
            keylock::keylock crypto(keylock::Algorithm::Ed25519);
            auto keypair = crypto.generate_keypair();

            if (keypair.private_key.empty()) {
                return dp::Result<Key, dp::Error>::err(dp::Error::io_error("Failed to generate keypair"));
            }

            return dp::Result<Key, dp::Error>::ok(Key(keypair, std::nullopt));
        }

        /// Generate new Ed25519 keypair with expiration
        inline static dp::Result<Key, dp::Error>
        generateWithExpiration(std::chrono::system_clock::time_point valid_until) {
            keylock::keylock crypto(keylock::Algorithm::Ed25519);
            auto keypair = crypto.generate_keypair();

            if (keypair.private_key.empty()) {
                return dp::Result<Key, dp::Error>::err(dp::Error::io_error("Failed to generate keypair"));
            }

            return dp::Result<Key, dp::Error>::ok(Key(keypair, valid_until));
        }

        /// Create from keylock::KeyPair directly
        inline explicit Key(const keylock::KeyPair &keypair) : keypair_(keypair), valid_until_(std::nullopt) {}

        /// Create from keylock::KeyPair with expiration
        inline Key(const keylock::KeyPair &keypair, std::chrono::system_clock::time_point valid_until)
            : keypair_(keypair), valid_until_(valid_until) {}

        /// Load from keypair bytes (private key can be 32 or 64 bytes for Ed25519)
        inline static dp::Result<Key, dp::Error> fromKeypair(const std::vector<uint8_t> &public_key,
                                                             const std::vector<uint8_t> &private_key) {
            if (public_key.size() != 32) {
                return dp::Result<Key, dp::Error>::err(
                    dp::Error::invalid_argument("Ed25519 public key must be 32 bytes"));
            }
            // Ed25519 private key can be 32 bytes (seed) or 64 bytes (seed + public key)
            if (private_key.size() != 32 && private_key.size() != 64) {
                return dp::Result<Key, dp::Error>::err(
                    dp::Error::invalid_argument("Ed25519 private key must be 32 or 64 bytes"));
            }

            keylock::KeyPair keypair;
            keypair.public_key = public_key;
            keypair.private_key = private_key;
            return dp::Result<Key, dp::Error>::ok(Key(keypair, std::nullopt));
        }

        /// Load from public key only (for verification)
        inline static dp::Result<Key, dp::Error> fromPublicKey(const std::vector<uint8_t> &public_key) {
            if (public_key.size() != 32) {
                return dp::Result<Key, dp::Error>::err(
                    dp::Error::invalid_argument("Ed25519 public key must be 32 bytes"));
            }

            keylock::KeyPair keypair;
            keypair.public_key = public_key;
            // private_key left empty - can only verify, not sign
            return dp::Result<Key, dp::Error>::ok(Key(keypair, std::nullopt));
        }

        /// Sign data
        inline dp::Result<std::vector<uint8_t>, dp::Error> sign(const std::vector<uint8_t> &data) const {
            if (keypair_.private_key.empty()) {
                return dp::Result<std::vector<uint8_t>, dp::Error>::err(
                    dp::Error::io_error("No private key available"));
            }

            keylock::keylock crypto(keylock::Algorithm::Ed25519);
            auto result = crypto.sign(data, keypair_.private_key);

            if (!result.success) {
                return dp::Result<std::vector<uint8_t>, dp::Error>::err(
                    dp::Error::io_error(dp::String(result.error_message.c_str())));
            }

            return dp::Result<std::vector<uint8_t>, dp::Error>::ok(result.data);
        }

        /// Verify signature
        inline dp::Result<bool, dp::Error> verify(const std::vector<uint8_t> &data,
                                                  const std::vector<uint8_t> &signature) const {
            if (keypair_.public_key.empty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("No public key available"));
            }

            keylock::keylock crypto(keylock::Algorithm::Ed25519);
            auto result = crypto.verify(data, signature, keypair_.public_key);

            if (!result.success) {
                return dp::Result<bool, dp::Error>::err(
                    dp::Error::invalid_argument(dp::String(result.error_message.c_str())));
            }

            return dp::Result<bool, dp::Error>::ok(result.success);
        }

        /// Get public key
        inline const std::vector<uint8_t> &getPublicKey() const { return keypair_.public_key; }

        /// Get private key (for signing)
        inline const std::vector<uint8_t> &getPrivateKey() const { return keypair_.private_key; }

        /// Check if has private key (can sign)
        inline bool hasPrivateKey() const { return !keypair_.private_key.empty(); }

        /// Get unique ID (SHA-256 hash of public key, hex encoded)
        inline std::string getId() const {
            if (keypair_.public_key.empty()) {
                return "unknown";
            }

            keylock::keylock crypto(keylock::Algorithm::XChaCha20_Poly1305, keylock::HashAlgorithm::SHA256);
            auto hash_result = crypto.hash(keypair_.public_key);

            if (!hash_result.success) {
                return "unknown";
            }

            return keylock::keylock::to_hex(hash_result.data);
        }

        /// Check if expired
        inline bool isExpired() const {
            if (!valid_until_) {
                return false;
            }

            auto now = std::chrono::system_clock::now();
            return now >= *valid_until_;
        }

        /// Check validity (not expired + has keys)
        inline bool isValid() const {
            return !keypair_.private_key.empty() && !keypair_.public_key.empty() && !isExpired();
        }

        /// Get expiration time
        inline std::optional<std::chrono::system_clock::time_point> getExpiration() const { return valid_until_; }

        /// Set expiration time
        inline void setExpiration(std::chrono::system_clock::time_point valid_until) { valid_until_ = valid_until; }

        /// Clear expiration (make permanent)
        inline void clearExpiration() { valid_until_ = std::nullopt; }

        /// Serialize (public key + private key length + private key + optional expiration)
        inline std::vector<uint8_t> serialize() const {
            std::vector<dp::u8> result;
            dp::u32 priv_len = static_cast<dp::u32>(keypair_.private_key.size());
            result.reserve(32 + 4 + priv_len + 1 + 8);

            // Public key (32 bytes)
            result.insert(result.end(), keypair_.public_key.begin(), keypair_.public_key.end());

            // Private key length (4 bytes)
            const dp::u8 *priv_len_bytes = reinterpret_cast<const dp::u8 *>(&priv_len);
            result.insert(result.end(), priv_len_bytes, priv_len_bytes + 4);

            // Private key (variable length)
            result.insert(result.end(), keypair_.private_key.begin(), keypair_.private_key.end());

            // Has expiration flag (1 byte)
            dp::u8 has_expiration = valid_until_.has_value() ? 1 : 0;
            result.push_back(has_expiration);

            // Expiration timestamp (8 bytes, if present)
            if (valid_until_) {
                dp::i64 ts =
                    std::chrono::duration_cast<std::chrono::milliseconds>(valid_until_->time_since_epoch()).count();
                const dp::u8 *ts_bytes = reinterpret_cast<const dp::u8 *>(&ts);
                result.insert(result.end(), ts_bytes, ts_bytes + 8);
            }

            return result;
        }

        /// Deserialize
        inline static dp::Result<Key, dp::Error> deserialize(const std::vector<uint8_t> &data) {
            if (data.size() < 37) { // 32 + 4 + 1 (minimum, with 0-byte private key)
                return dp::Result<Key, dp::Error>::err(dp::Error::invalid_argument("Invalid serialized key data"));
            }

            size_t offset = 0;

            // Public key (32 bytes)
            std::vector<uint8_t> public_key(data.begin() + offset, data.begin() + offset + 32);
            offset += 32;

            // Private key length (4 bytes)
            if (offset + 4 > data.size()) {
                return dp::Result<Key, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            dp::u32 priv_len = *reinterpret_cast<const dp::u32 *>(data.data() + offset);
            offset += 4;

            // Private key (variable length)
            if (offset + priv_len > data.size()) {
                return dp::Result<Key, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            std::vector<uint8_t> private_key(data.begin() + offset, data.begin() + offset + priv_len);
            offset += priv_len;

            // Has expiration flag
            if (offset >= data.size()) {
                return dp::Result<Key, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            uint8_t has_expiration = data[offset];
            offset += 1;

            // Optional expiration (8 bytes)
            std::optional<std::chrono::system_clock::time_point> valid_until;
            if (has_expiration && offset + 8 <= data.size()) {
                dp::i64 ts = *reinterpret_cast<const dp::i64 *>(data.data() + offset);
                valid_until = std::chrono::system_clock::time_point() + std::chrono::milliseconds(ts);
            }

            keylock::KeyPair keypair;
            keypair.public_key = public_key;
            keypair.private_key = private_key;

            return dp::Result<Key, dp::Error>::ok(Key(keypair, valid_until));
        }

        /// Equality operator (compares public keys)
        inline bool operator==(const Key &other) const { return keypair_.public_key == other.keypair_.public_key; }

        inline bool operator!=(const Key &other) const { return !(*this == other); }

      private:
        inline Key(const keylock::KeyPair &keypair, std::optional<std::chrono::system_clock::time_point> valid_until)
            : keypair_(keypair), valid_until_(valid_until) {}

        keylock::KeyPair keypair_;
        std::optional<std::chrono::system_clock::time_point> valid_until_;
    };

} // namespace blockit
