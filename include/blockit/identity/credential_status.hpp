#pragma once

#include "verifiable_credential.hpp"
#include <datapod/datapod.hpp>
#include <shared_mutex>
#include <unordered_map>

namespace blockit {

    /// Credential operation types
    enum class CredentialOperationType : dp::u8 {
        Issue = 0,
        Revoke = 1,
        Suspend = 2,
        Unsuspend = 3,
    };

    /// Get string name for credential operation type
    inline std::string credentialOperationTypeToString(CredentialOperationType type) {
        switch (type) {
        case CredentialOperationType::Issue:
            return "issue";
        case CredentialOperationType::Revoke:
            return "revoke";
        case CredentialOperationType::Suspend:
            return "suspend";
        case CredentialOperationType::Unsuspend:
            return "unsuspend";
        default:
            return "unknown";
        }
    }

    /// Credential operation record (stored in blockchain transactions)
    struct CredentialOperation {
        dp::u8 operation_type{0}; // CredentialOperationType
        dp::String credential_id;
        dp::String issuer_did;
        dp::i64 timestamp{0};
        dp::Vector<dp::u8> credential_data; // Serialized credential (for Issue operation)
        dp::String reason;                  // Optional reason for revocation/suspension

        CredentialOperation() = default;

        CredentialOperation(CredentialOperationType type, const std::string &cred_id, const std::string &issuer,
                            const std::string &reason_str = "")
            : operation_type(static_cast<dp::u8>(type)), credential_id(dp::String(cred_id.c_str())),
              issuer_did(dp::String(issuer.c_str())), timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                    std::chrono::system_clock::now().time_since_epoch())
                                                                    .count()),
              reason(dp::String(reason_str.c_str())) {}

        /// Create an Issue operation
        inline static CredentialOperation createIssue(const VerifiableCredential &credential) {
            CredentialOperation op;
            op.operation_type = static_cast<dp::u8>(CredentialOperationType::Issue);
            op.credential_id = dp::String(credential.getId().c_str());
            op.issuer_did = dp::String(credential.getIssuerString().c_str());
            op.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();

            auto serialized = credential.serialize();
            op.credential_data = dp::Vector<dp::u8>(serialized.begin(), serialized.end());

            return op;
        }

        /// Create a Revoke operation
        inline static CredentialOperation createRevoke(const std::string &cred_id, const std::string &issuer,
                                                       const std::string &reason = "") {
            return CredentialOperation(CredentialOperationType::Revoke, cred_id, issuer, reason);
        }

        /// Create a Suspend operation
        inline static CredentialOperation createSuspend(const std::string &cred_id, const std::string &issuer,
                                                        const std::string &reason = "") {
            return CredentialOperation(CredentialOperationType::Suspend, cred_id, issuer, reason);
        }

        /// Create an Unsuspend operation
        inline static CredentialOperation createUnsuspend(const std::string &cred_id, const std::string &issuer) {
            return CredentialOperation(CredentialOperationType::Unsuspend, cred_id, issuer);
        }

        /// Get operation type as enum
        inline CredentialOperationType getOperationType() const {
            return static_cast<CredentialOperationType>(operation_type);
        }

        /// Get credential ID
        inline std::string getCredentialId() const { return std::string(credential_id.c_str()); }

        /// Get issuer DID
        inline std::string getIssuerDID() const { return std::string(issuer_did.c_str()); }

        /// Get reason
        inline std::string getReason() const { return std::string(reason.c_str()); }

        /// Get timestamp
        inline dp::i64 getTimestamp() const { return timestamp; }

        /// Get credential data (for Issue operations)
        inline std::vector<uint8_t> getCredentialData() const {
            return std::vector<uint8_t>(credential_data.begin(), credential_data.end());
        }

        /// Deserialize the credential (for Issue operations)
        inline dp::Result<VerifiableCredential, dp::Error> getCredential() const {
            if (credential_data.empty()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(
                    dp::Error::invalid_argument("No credential data"));
            }
            dp::ByteBuf buf(credential_data.begin(), credential_data.end());
            return VerifiableCredential::deserialize(buf);
        }

        /// Serialize to bytes
        inline std::vector<uint8_t> toBytes() const {
            auto &self = const_cast<CredentialOperation &>(*this);
            auto buf = dp::serialize<dp::Mode::WITH_VERSION>(self);
            return std::vector<uint8_t>(buf.begin(), buf.end());
        }

        /// Deserialize from bytes
        inline static dp::Result<CredentialOperation, dp::Error> fromBytes(const std::vector<uint8_t> &data) {
            try {
                dp::ByteBuf buf(data.begin(), data.end());
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, CredentialOperation>(buf);
                return dp::Result<CredentialOperation, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<CredentialOperation, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        /// Serialization
        auto members() {
            return std::tie(operation_type, credential_id, issuer_did, timestamp, credential_data, reason);
        }
        auto members() const {
            return std::tie(operation_type, credential_id, issuer_did, timestamp, credential_data, reason);
        }
    };

    /// Status entry for a credential
    struct CredentialStatusEntry {
        CredentialStatus status{CredentialStatus::Active};
        dp::i64 updated_at{0};
        std::string reason;
        std::string issuer_did;

        CredentialStatusEntry() = default;
        CredentialStatusEntry(CredentialStatus s, dp::i64 ts, const std::string &r = "", const std::string &issuer = "")
            : status(s), updated_at(ts), reason(r), issuer_did(issuer) {}
    };

    /// Credential Status List - tracks credential status (active, revoked, suspended)
    /// Standalone class that can be used with or without blockchain
    class CredentialStatusList {
      public:
        CredentialStatusList() = default;

        // === Status Operations ===

        /// Record a new credential issuance
        inline void recordIssue(const std::string &credential_id, const std::string &issuer_did) {
            std::unique_lock lock(mutex_);
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            status_index_[credential_id] = CredentialStatusEntry(CredentialStatus::Active, now, "", issuer_did);
        }

        /// Record a credential revocation
        inline dp::Result<void, dp::Error> recordRevoke(const std::string &credential_id, const std::string &issuer_did,
                                                        const std::string &reason = "") {
            std::unique_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return dp::Result<void, dp::Error>::err(dp::Error::not_found("Credential not found"));
            }

            // Verify issuer
            if (it->second.issuer_did != issuer_did) {
                return dp::Result<void, dp::Error>::err(
                    dp::Error::permission_denied("Only the issuer can revoke this credential"));
            }

            // Cannot revoke if already revoked
            if (it->second.status == CredentialStatus::Revoked) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("Credential already revoked"));
            }

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            it->second.status = CredentialStatus::Revoked;
            it->second.updated_at = now;
            it->second.reason = reason;

            return dp::Result<void, dp::Error>::ok();
        }

        /// Record a credential suspension
        inline dp::Result<void, dp::Error>
        recordSuspend(const std::string &credential_id, const std::string &issuer_did, const std::string &reason = "") {
            std::unique_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return dp::Result<void, dp::Error>::err(dp::Error::not_found("Credential not found"));
            }

            // Verify issuer
            if (it->second.issuer_did != issuer_did) {
                return dp::Result<void, dp::Error>::err(
                    dp::Error::permission_denied("Only the issuer can suspend this credential"));
            }

            // Cannot suspend if revoked
            if (it->second.status == CredentialStatus::Revoked) {
                return dp::Result<void, dp::Error>::err(
                    dp::Error::invalid_argument("Cannot suspend a revoked credential"));
            }

            // Cannot suspend if already suspended
            if (it->second.status == CredentialStatus::Suspended) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("Credential already suspended"));
            }

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            it->second.status = CredentialStatus::Suspended;
            it->second.updated_at = now;
            it->second.reason = reason;

            return dp::Result<void, dp::Error>::ok();
        }

        /// Record a credential unsuspension
        inline dp::Result<void, dp::Error> recordUnsuspend(const std::string &credential_id,
                                                           const std::string &issuer_did) {
            std::unique_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return dp::Result<void, dp::Error>::err(dp::Error::not_found("Credential not found"));
            }

            // Verify issuer
            if (it->second.issuer_did != issuer_did) {
                return dp::Result<void, dp::Error>::err(
                    dp::Error::permission_denied("Only the issuer can unsuspend this credential"));
            }

            // Can only unsuspend if suspended
            if (it->second.status != CredentialStatus::Suspended) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("Credential is not suspended"));
            }

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            it->second.status = CredentialStatus::Active;
            it->second.updated_at = now;
            it->second.reason = "";

            return dp::Result<void, dp::Error>::ok();
        }

        /// Apply an operation (used when loading/syncing from blockchain)
        inline dp::Result<void, dp::Error> applyOperation(const CredentialOperation &op) {
            switch (op.getOperationType()) {
            case CredentialOperationType::Issue:
                recordIssue(op.getCredentialId(), op.getIssuerDID());
                return dp::Result<void, dp::Error>::ok();

            case CredentialOperationType::Revoke:
                return recordRevoke(op.getCredentialId(), op.getIssuerDID(), op.getReason());

            case CredentialOperationType::Suspend:
                return recordSuspend(op.getCredentialId(), op.getIssuerDID(), op.getReason());

            case CredentialOperationType::Unsuspend:
                return recordUnsuspend(op.getCredentialId(), op.getIssuerDID());

            default:
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("Unknown operation type"));
            }
        }

        // === Query Operations ===

        /// Check if a credential exists
        inline bool exists(const std::string &credential_id) const {
            std::shared_lock lock(mutex_);
            return status_index_.find(credential_id) != status_index_.end();
        }

        /// Check if a credential is revoked
        inline bool isRevoked(const std::string &credential_id) const {
            std::shared_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return false;
            }
            return it->second.status == CredentialStatus::Revoked;
        }

        /// Check if a credential is suspended
        inline bool isSuspended(const std::string &credential_id) const {
            std::shared_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return false;
            }
            return it->second.status == CredentialStatus::Suspended;
        }

        /// Check if a credential is active
        inline bool isActive(const std::string &credential_id) const {
            std::shared_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return false;
            }
            return it->second.status == CredentialStatus::Active;
        }

        /// Get credential status
        inline dp::Result<CredentialStatus, dp::Error> getStatus(const std::string &credential_id) const {
            std::shared_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return dp::Result<CredentialStatus, dp::Error>::err(dp::Error::not_found("Credential not found"));
            }
            return dp::Result<CredentialStatus, dp::Error>::ok(it->second.status);
        }

        /// Get full status entry
        inline dp::Result<CredentialStatusEntry, dp::Error> getStatusEntry(const std::string &credential_id) const {
            std::shared_lock lock(mutex_);
            auto it = status_index_.find(credential_id);
            if (it == status_index_.end()) {
                return dp::Result<CredentialStatusEntry, dp::Error>::err(dp::Error::not_found("Credential not found"));
            }
            return dp::Result<CredentialStatusEntry, dp::Error>::ok(it->second);
        }

        /// Get all credentials by issuer
        inline std::vector<std::string> getCredentialsByIssuer(const std::string &issuer_did) const {
            std::shared_lock lock(mutex_);
            std::vector<std::string> result;
            for (const auto &[cred_id, entry] : status_index_) {
                if (entry.issuer_did == issuer_did) {
                    result.push_back(cred_id);
                }
            }
            return result;
        }

        /// Get all revoked credentials
        inline std::vector<std::string> getRevokedCredentials() const {
            std::shared_lock lock(mutex_);
            std::vector<std::string> result;
            for (const auto &[cred_id, entry] : status_index_) {
                if (entry.status == CredentialStatus::Revoked) {
                    result.push_back(cred_id);
                }
            }
            return result;
        }

        /// Get all suspended credentials
        inline std::vector<std::string> getSuspendedCredentials() const {
            std::shared_lock lock(mutex_);
            std::vector<std::string> result;
            for (const auto &[cred_id, entry] : status_index_) {
                if (entry.status == CredentialStatus::Suspended) {
                    result.push_back(cred_id);
                }
            }
            return result;
        }

        /// Get the number of credentials in the list
        inline size_t size() const {
            std::shared_lock lock(mutex_);
            return status_index_.size();
        }

        /// Clear the status list (for testing)
        inline void clear() {
            std::unique_lock lock(mutex_);
            status_index_.clear();
        }

      private:
        std::unordered_map<std::string, CredentialStatusEntry> status_index_;
        mutable std::shared_mutex mutex_;
    };

} // namespace blockit
