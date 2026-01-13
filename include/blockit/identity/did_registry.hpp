#pragma once

#include "did_document.hpp"
#include <datapod/datapod.hpp>
#include <shared_mutex>
#include <unordered_map>

namespace blockit {

    /// DID operation types
    enum class DIDOperationType : dp::u8 {
        Create = 0,
        Update = 1,
        Deactivate = 2,
    };

    /// Get string name for operation type
    inline std::string didOperationTypeToString(DIDOperationType type) {
        switch (type) {
        case DIDOperationType::Create:
            return "create";
        case DIDOperationType::Update:
            return "update";
        case DIDOperationType::Deactivate:
            return "deactivate";
        default:
            return "unknown";
        }
    }

    /// DID operation record (stored in blockchain transactions)
    struct DIDOperation {
        dp::u8 operation_type{0};    // DIDOperationType
        dp::String did;              // The DID being operated on
        dp::Vector<dp::u8> document; // Serialized DIDDocument
        dp::i64 timestamp{0};        // Operation timestamp
        dp::String signer_did;       // DID that signed this operation

        DIDOperation() = default;

        DIDOperation(DIDOperationType type, const std::string &did_str, const std::vector<uint8_t> &doc_data,
                     const std::string &signer = "")
            : operation_type(static_cast<dp::u8>(type)), did(dp::String(did_str.c_str())),
              document(dp::Vector<dp::u8>(doc_data.begin(), doc_data.end())),
              timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count()),
              signer_did(dp::String(signer.c_str())) {}

        /// Get operation type as enum
        inline DIDOperationType getOperationType() const { return static_cast<DIDOperationType>(operation_type); }

        /// Get DID string
        inline std::string getDID() const { return std::string(did.c_str()); }

        /// Get signer DID string
        inline std::string getSignerDID() const { return std::string(signer_did.c_str()); }

        /// Get document bytes
        inline std::vector<uint8_t> getDocumentBytes() const {
            return std::vector<uint8_t>(document.begin(), document.end());
        }

        /// Deserialize the DID document from this operation
        inline dp::Result<DIDDocument, dp::Error> getDocument() const {
            if (document.empty()) {
                return dp::Result<DIDDocument, dp::Error>::err(dp::Error::invalid_argument("No document data"));
            }
            std::vector<uint8_t> doc_vec(document.begin(), document.end());
            dp::ByteBuf buf(doc_vec.begin(), doc_vec.end());
            return DIDDocument::deserialize(buf);
        }

        /// For use as blockchain transaction data
        inline std::vector<uint8_t> toBytes() const {
            auto &self = const_cast<DIDOperation &>(*this);
            auto buf = dp::serialize<dp::Mode::WITH_VERSION>(self);
            return std::vector<uint8_t>(buf.begin(), buf.end());
        }

        /// Deserialize from bytes
        inline static dp::Result<DIDOperation, dp::Error> fromBytes(const std::vector<uint8_t> &data) {
            try {
                dp::ByteBuf buf(data.begin(), data.end());
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, DIDOperation>(buf);
                return dp::Result<DIDOperation, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<DIDOperation, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        /// Serialization
        auto members() { return std::tie(operation_type, did, document, timestamp, signer_did); }
        auto members() const { return std::tie(operation_type, did, document, timestamp, signer_did); }
    };

    /// DID Registry manages DID lifecycle
    /// This is a standalone registry that can be used with any blockchain implementation
    class DIDRegistry {
      public:
        DIDRegistry() = default;

        // === DID Operations ===

        /// Create a new DID and document
        /// Returns the created DIDOperation that should be stored on the blockchain
        inline dp::Result<std::pair<DIDDocument, DIDOperation>, dp::Error> create(const Key &key) {
            // Create DID document
            auto doc = DIDDocument::create(key);
            auto did = doc.getId();
            auto did_str = did.toString();

            // Check if DID already exists
            {
                std::shared_lock lock(mutex_);
                if (did_cache_.find(did_str) != did_cache_.end()) {
                    return dp::Result<std::pair<DIDDocument, DIDOperation>, dp::Error>::err(
                        dp::Error::already_exists("DID already exists"));
                }
            }

            // Serialize document
            auto serialized = doc.serialize();
            std::vector<uint8_t> doc_bytes(serialized.begin(), serialized.end());

            // Create operation
            DIDOperation op(DIDOperationType::Create, did_str, doc_bytes, did_str);

            // Add to cache and history
            {
                std::unique_lock lock(mutex_);
                did_cache_[did_str] = doc;
                did_history_[did_str].push_back(doc);
            }

            return dp::Result<std::pair<DIDDocument, DIDOperation>, dp::Error>::ok({doc, op});
        }

        /// Apply an operation from the blockchain (used when loading/syncing)
        inline dp::Result<void, dp::Error> applyOperation(const DIDOperation &op) {
            auto doc_result = op.getDocument();
            if (!doc_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(doc_result.error());
            }

            auto doc = doc_result.value();
            auto did_str = op.getDID();

            std::unique_lock lock(mutex_);

            switch (op.getOperationType()) {
            case DIDOperationType::Create:
                if (did_cache_.find(did_str) != did_cache_.end()) {
                    return dp::Result<void, dp::Error>::err(dp::Error::already_exists("DID already exists"));
                }
                did_cache_[did_str] = doc;
                break;

            case DIDOperationType::Update:
                if (did_cache_.find(did_str) == did_cache_.end()) {
                    return dp::Result<void, dp::Error>::err(dp::Error::not_found("DID not found"));
                }
                did_cache_[did_str] = doc;
                break;

            case DIDOperationType::Deactivate:
                if (did_cache_.find(did_str) == did_cache_.end()) {
                    return dp::Result<void, dp::Error>::err(dp::Error::not_found("DID not found"));
                }
                did_cache_[did_str] = doc;
                break;
            }

            // Track history
            did_history_[did_str].push_back(doc);

            return dp::Result<void, dp::Error>::ok();
        }

        /// Resolve a DID to its document
        inline dp::Result<DIDDocument, dp::Error> resolve(const DID &did) const { return resolve(did.toString()); }

        /// Resolve a DID string to its document
        inline dp::Result<DIDDocument, dp::Error> resolve(const std::string &did_string) const {
            std::shared_lock lock(mutex_);
            auto it = did_cache_.find(did_string);
            if (it == did_cache_.end()) {
                return dp::Result<DIDDocument, dp::Error>::err(dp::Error::not_found("DID not found"));
            }
            return dp::Result<DIDDocument, dp::Error>::ok(it->second);
        }

        /// Update a DID document
        /// The controller_key must be able to sign as one of the document's controllers
        inline dp::Result<DIDOperation, dp::Error> update(const DID &did, const DIDDocument &updated_doc,
                                                          const Key &controller_key) {
            auto did_str = did.toString();

            // Verify the DID exists and get current document
            DIDDocument current_doc;
            {
                std::shared_lock lock(mutex_);
                auto it = did_cache_.find(did_str);
                if (it == did_cache_.end()) {
                    return dp::Result<DIDOperation, dp::Error>::err(dp::Error::not_found("DID not found"));
                }
                current_doc = it->second;
            }

            // Check if document is active
            if (!current_doc.isActive()) {
                return dp::Result<DIDOperation, dp::Error>::err(
                    dp::Error::invalid_argument("Cannot update deactivated DID"));
            }

            // Verify controller authorization
            auto controller_did = DID::fromKey(controller_key);
            if (!current_doc.isController(controller_did)) {
                return dp::Result<DIDOperation, dp::Error>::err(
                    dp::Error::permission_denied("Not a controller of this DID"));
            }

            // Serialize updated document
            auto serialized = updated_doc.serialize();
            std::vector<uint8_t> doc_bytes(serialized.begin(), serialized.end());

            // Create operation
            DIDOperation op(DIDOperationType::Update, did_str, doc_bytes, controller_did.toString());

            // Update cache
            {
                std::unique_lock lock(mutex_);
                did_cache_[did_str] = updated_doc;
                did_history_[did_str].push_back(updated_doc);
            }

            return dp::Result<DIDOperation, dp::Error>::ok(op);
        }

        /// Deactivate a DID (permanent)
        inline dp::Result<DIDOperation, dp::Error> deactivate(const DID &did, const Key &controller_key) {
            auto did_str = did.toString();

            // Get current document
            DIDDocument current_doc;
            {
                std::shared_lock lock(mutex_);
                auto it = did_cache_.find(did_str);
                if (it == did_cache_.end()) {
                    return dp::Result<DIDOperation, dp::Error>::err(dp::Error::not_found("DID not found"));
                }
                current_doc = it->second;
            }

            // Check if already deactivated
            if (!current_doc.isActive()) {
                return dp::Result<DIDOperation, dp::Error>::err(dp::Error::invalid_argument("DID already deactivated"));
            }

            // Verify controller authorization
            auto controller_did = DID::fromKey(controller_key);
            if (!current_doc.isController(controller_did)) {
                return dp::Result<DIDOperation, dp::Error>::err(
                    dp::Error::permission_denied("Not a controller of this DID"));
            }

            // Deactivate document
            current_doc.deactivate();

            // Serialize
            auto serialized = current_doc.serialize();
            std::vector<uint8_t> doc_bytes(serialized.begin(), serialized.end());

            // Create operation
            DIDOperation op(DIDOperationType::Deactivate, did_str, doc_bytes, controller_did.toString());

            // Update cache
            {
                std::unique_lock lock(mutex_);
                did_cache_[did_str] = current_doc;
                did_history_[did_str].push_back(current_doc);
            }

            return dp::Result<DIDOperation, dp::Error>::ok(op);
        }

        // === Query Operations ===

        /// Check if a DID exists
        inline bool exists(const DID &did) const { return exists(did.toString()); }

        /// Check if a DID string exists
        inline bool exists(const std::string &did_string) const {
            std::shared_lock lock(mutex_);
            return did_cache_.find(did_string) != did_cache_.end();
        }

        /// Get all DIDs in the registry
        inline std::vector<DID> getAllDIDs() const {
            std::shared_lock lock(mutex_);
            std::vector<DID> result;
            for (const auto &[did_str, doc] : did_cache_) {
                auto parsed = DID::parse(did_str);
                if (parsed.is_ok()) {
                    result.push_back(parsed.value());
                }
            }
            return result;
        }

        /// Get all active DIDs
        inline std::vector<DID> getActiveDIDs() const {
            std::shared_lock lock(mutex_);
            std::vector<DID> result;
            for (const auto &[did_str, doc] : did_cache_) {
                if (doc.isActive()) {
                    auto parsed = DID::parse(did_str);
                    if (parsed.is_ok()) {
                        result.push_back(parsed.value());
                    }
                }
            }
            return result;
        }

        /// Get all DIDs controlled by a specific DID
        inline std::vector<DID> getControlled(const DID &controller) const {
            std::shared_lock lock(mutex_);
            std::vector<DID> result;
            for (const auto &[did_str, doc] : did_cache_) {
                if (doc.isController(controller)) {
                    auto parsed = DID::parse(did_str);
                    if (parsed.is_ok()) {
                        result.push_back(parsed.value());
                    }
                }
            }
            return result;
        }

        /// Get DID document history (all versions)
        inline std::vector<DIDDocument> getHistory(const DID &did) const { return getHistory(did.toString()); }

        /// Get DID document history by string
        inline std::vector<DIDDocument> getHistory(const std::string &did_string) const {
            std::shared_lock lock(mutex_);
            auto it = did_history_.find(did_string);
            if (it == did_history_.end()) {
                return {};
            }
            return it->second;
        }

        /// Get the number of DIDs in the registry
        inline size_t size() const {
            std::shared_lock lock(mutex_);
            return did_cache_.size();
        }

        /// Clear the registry (for testing)
        inline void clear() {
            std::unique_lock lock(mutex_);
            did_cache_.clear();
            did_history_.clear();
        }

        // === Verification ===

        /// Verify a signature was made by a key in the DID document
        inline dp::Result<bool, dp::Error> verifySignature(const DID &did, const std::string &key_id,
                                                           const std::vector<uint8_t> &data,
                                                           const std::vector<uint8_t> &signature) const {
            // Resolve the DID
            auto doc_result = resolve(did);
            if (!doc_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(doc_result.error());
            }

            auto doc = doc_result.value();

            // Get the verification method
            auto vm_result = doc.getVerificationMethod(key_id);
            if (!vm_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(vm_result.error());
            }

            auto vm = vm_result.value();

            // Convert to Key and verify
            auto key_result = vm.toKey();
            if (!key_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(key_result.error());
            }

            return key_result.value().verify(data, signature);
        }

        /// Verify the signer has authentication relationship
        inline dp::Result<bool, dp::Error> verifyAuthentication(const DID &did, const std::vector<uint8_t> &challenge,
                                                                const std::vector<uint8_t> &signature,
                                                                const std::string &key_id) const {
            // Resolve the DID
            auto doc_result = resolve(did);
            if (!doc_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(doc_result.error());
            }

            auto doc = doc_result.value();

            // Check if the key is used for authentication
            if (!doc.canAuthenticate(key_id)) {
                return dp::Result<bool, dp::Error>::err(
                    dp::Error::permission_denied("Key is not authorized for authentication"));
            }

            // Verify the signature
            return verifySignature(did, key_id, challenge, signature);
        }

      private:
        // In-memory cache: DID string -> latest document
        mutable std::unordered_map<std::string, DIDDocument> did_cache_;

        // History: DID string -> all versions
        mutable std::unordered_map<std::string, std::vector<DIDDocument>> did_history_;

        mutable std::shared_mutex mutex_;
    };

} // namespace blockit
