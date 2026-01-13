#pragma once

#include <blockit/ledger/key.hpp>
#include <datapod/datapod.hpp>
#include <regex>
#include <string>

namespace blockit {

    /// DID (Decentralized Identifier) following W3C DID Core v1.0
    /// Format: did:blockit:<method-specific-id>
    /// The method-specific-id is the SHA-256 hash of the initial public key (hex encoded)
    class DID {
      public:
        static constexpr const char *METHOD = "blockit";
        static constexpr const char *SCHEME = "did";

        DID() = default;

        /// Parse a DID string (did:blockit:xxx)
        inline static dp::Result<DID, dp::Error> parse(const std::string &did_string) {
            // Basic validation: must start with "did:"
            if (did_string.size() < 4 || did_string.substr(0, 4) != "did:") {
                return dp::Result<DID, dp::Error>::err(
                    dp::Error::invalid_argument("Invalid DID: must start with 'did:'"));
            }

            // Find the method separator
            size_t first_colon = did_string.find(':', 0);
            size_t second_colon = did_string.find(':', first_colon + 1);

            if (second_colon == std::string::npos) {
                return dp::Result<DID, dp::Error>::err(
                    dp::Error::invalid_argument("Invalid DID: missing method-specific-id"));
            }

            std::string method = did_string.substr(first_colon + 1, second_colon - first_colon - 1);

            // Validate method is "blockit"
            if (method != METHOD) {
                return dp::Result<DID, dp::Error>::err(dp::Error::invalid_argument(
                    dp::String(("Invalid DID method: expected 'blockit', got '" + method + "'").c_str())));
            }

            // Extract method-specific-id (may include fragment or path)
            std::string method_specific_id = did_string.substr(second_colon + 1);

            // Remove fragment if present (for base DID)
            size_t fragment_pos = method_specific_id.find('#');
            if (fragment_pos != std::string::npos) {
                method_specific_id = method_specific_id.substr(0, fragment_pos);
            }

            // Remove path if present (for base DID)
            size_t path_pos = method_specific_id.find('/');
            if (path_pos != std::string::npos) {
                method_specific_id = method_specific_id.substr(0, path_pos);
            }

            // Validate method-specific-id is hex (SHA-256 hash = 64 hex chars)
            if (method_specific_id.size() != 64) {
                return dp::Result<DID, dp::Error>::err(dp::Error::invalid_argument(
                    "Invalid DID: method-specific-id must be 64 hex characters (SHA-256 hash)"));
            }

            for (char c : method_specific_id) {
                if (!std::isxdigit(static_cast<unsigned char>(c))) {
                    return dp::Result<DID, dp::Error>::err(
                        dp::Error::invalid_argument("Invalid DID: method-specific-id must be hexadecimal"));
                }
            }

            DID did;
            did.method_specific_id_ = dp::String(method_specific_id.c_str());
            return dp::Result<DID, dp::Error>::ok(did);
        }

        /// Create DID from a Key (uses Key::getId() as method-specific-id)
        inline static DID fromKey(const Key &key) {
            DID did;
            did.method_specific_id_ = dp::String(key.getId().c_str());
            return did;
        }

        /// Create DID from method-specific identifier
        inline static DID fromMethodSpecificId(const std::string &method_specific_id) {
            DID did;
            did.method_specific_id_ = dp::String(method_specific_id.c_str());
            return did;
        }

        /// Get the full DID string (did:blockit:xxx)
        inline std::string toString() const {
            return std::string(SCHEME) + ":" + METHOD + ":" + std::string(method_specific_id_.c_str());
        }

        /// Get method name ("blockit")
        inline std::string getMethod() const { return METHOD; }

        /// Get method-specific identifier
        inline std::string getMethodSpecificId() const { return std::string(method_specific_id_.c_str()); }

        /// Create a DID URL with fragment (did:blockit:xxx#fragment)
        inline std::string withFragment(const std::string &fragment) const { return toString() + "#" + fragment; }

        /// Create a DID URL with path (did:blockit:xxx/path)
        inline std::string withPath(const std::string &path) const { return toString() + "/" + path; }

        /// Create a DID URL with query (did:blockit:xxx?query)
        inline std::string withQuery(const std::string &query) const { return toString() + "?" + query; }

        /// Check if DID is empty/uninitialized
        inline bool isEmpty() const { return method_specific_id_.empty(); }

        /// Equality comparison
        inline bool operator==(const DID &other) const { return method_specific_id_ == other.method_specific_id_; }

        inline bool operator!=(const DID &other) const { return !(*this == other); }

        /// Less-than comparison (for use in maps/sets)
        inline bool operator<(const DID &other) const {
            return std::string(method_specific_id_.c_str()) < std::string(other.method_specific_id_.c_str());
        }

        /// Hash function for use in unordered containers
        inline size_t hash() const { return std::hash<std::string>{}(std::string(method_specific_id_.c_str())); }

        /// Serialization
        auto members() { return std::tie(method_specific_id_); }
        auto members() const { return std::tie(method_specific_id_); }

      private:
        dp::String method_specific_id_;
    };

} // namespace blockit

// Hash specialization for std::unordered_map
namespace std {
    template <> struct hash<blockit::DID> {
        size_t operator()(const blockit::DID &did) const { return did.hash(); }
    };
} // namespace std
