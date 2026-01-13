#pragma once

#include "did.hpp"
#include <blockit/key.hpp>
#include <chrono>
#include <datapod/datapod.hpp>
#include <map>

namespace blockit {

    /// Credential types for robot swarms
    enum class CredentialType : dp::u8 {
        VerifiableCredential = 0, // Base type (always included)
        RobotAuthorization = 1,   // Robot is authorized for swarm
        CapabilityGrant = 2,      // Robot has specific capability
        ZoneAccess = 3,           // Robot can access specific zone
        TaskCertification = 4,    // Robot completed task certification
        SensorCalibration = 5,    // Sensor calibration certificate
        SwarmMembership = 6,      // Robot is member of swarm
        Custom = 255,
    };

    /// Get string name for credential type
    inline std::string credentialTypeToString(CredentialType type) {
        switch (type) {
        case CredentialType::VerifiableCredential:
            return "VerifiableCredential";
        case CredentialType::RobotAuthorization:
            return "RobotAuthorization";
        case CredentialType::CapabilityGrant:
            return "CapabilityGrant";
        case CredentialType::ZoneAccess:
            return "ZoneAccess";
        case CredentialType::TaskCertification:
            return "TaskCertification";
        case CredentialType::SensorCalibration:
            return "SensorCalibration";
        case CredentialType::SwarmMembership:
            return "SwarmMembership";
        case CredentialType::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    /// Credential status
    enum class CredentialStatus : dp::u8 {
        Active = 0,
        Revoked = 1,
        Suspended = 2,
        Expired = 3,
    };

    /// Get string name for credential status
    inline std::string credentialStatusToString(CredentialStatus status) {
        switch (status) {
        case CredentialStatus::Active:
            return "active";
        case CredentialStatus::Revoked:
            return "revoked";
        case CredentialStatus::Suspended:
            return "suspended";
        case CredentialStatus::Expired:
            return "expired";
        default:
            return "unknown";
        }
    }

    /// Credential subject (the entity the credential is about)
    struct CredentialSubject {
        dp::String id;                          // DID of subject
        dp::String type;                        // Subject type description
        dp::Map<dp::String, dp::String> claims; // Key-value claims

        CredentialSubject() = default;

        CredentialSubject(const std::string &subject_id, const std::string &subject_type = "")
            : id(dp::String(subject_id.c_str())), type(dp::String(subject_type.c_str())) {}

        /// Get subject ID
        inline std::string getId() const { return std::string(id.c_str()); }

        /// Get subject type
        inline std::string getType() const { return std::string(type.c_str()); }

        /// Set a claim
        inline void setClaim(const std::string &key, const std::string &value) {
            claims[dp::String(key.c_str())] = dp::String(value.c_str());
        }

        /// Get a claim
        inline dp::Result<std::string, dp::Error> getClaim(const std::string &key) const {
            auto it = claims.find(dp::String(key.c_str()));
            if (it == claims.end()) {
                return dp::Result<std::string, dp::Error>::err(dp::Error::not_found("Claim not found"));
            }
            return dp::Result<std::string, dp::Error>::ok(std::string(it->second.c_str()));
        }

        /// Check if claim exists
        inline bool hasClaim(const std::string &key) const {
            return claims.find(dp::String(key.c_str())) != claims.end();
        }

        /// Get all claims as std::map
        inline std::map<std::string, std::string> getAllClaims() const {
            std::map<std::string, std::string> result;
            for (const auto &[k, v] : claims) {
                result[std::string(k.c_str())] = std::string(v.c_str());
            }
            return result;
        }

        /// Serialization
        auto members() { return std::tie(id, type, claims); }
        auto members() const { return std::tie(id, type, claims); }
    };

    /// Proof of credential (Ed25519 signature)
    struct CredentialProof {
        dp::String type;                // "Ed25519Signature2020"
        dp::String created;             // ISO8601 timestamp
        dp::String verification_method; // Key ID used to sign (e.g., "did:blockit:xxx#key-1")
        dp::String proof_purpose;       // "assertionMethod"
        dp::Vector<dp::u8> proof_value; // Signature bytes

        CredentialProof() = default;

        /// Get type string
        inline std::string getType() const { return std::string(type.c_str()); }

        /// Get created timestamp string
        inline std::string getCreated() const { return std::string(created.c_str()); }

        /// Get verification method
        inline std::string getVerificationMethod() const { return std::string(verification_method.c_str()); }

        /// Get proof purpose
        inline std::string getProofPurpose() const { return std::string(proof_purpose.c_str()); }

        /// Get proof value as vector
        inline std::vector<uint8_t> getProofValue() const {
            return std::vector<uint8_t>(proof_value.begin(), proof_value.end());
        }

        /// Check if proof exists
        inline bool isEmpty() const { return proof_value.empty(); }

        /// Serialization
        auto members() { return std::tie(type, created, verification_method, proof_purpose, proof_value); }
        auto members() const { return std::tie(type, created, verification_method, proof_purpose, proof_value); }
    };

    /// Verifiable Credential following W3C VC Data Model v2.0
    class VerifiableCredential {
      public:
        VerifiableCredential() = default;

        /// Create a new credential
        inline static VerifiableCredential create(const std::string &id, CredentialType type, const DID &issuer,
                                                  const DID &subject) {
            VerifiableCredential vc;
            vc.id_ = dp::String(id.c_str());

            // Always include base VerifiableCredential type
            vc.types_.push_back(static_cast<dp::u8>(CredentialType::VerifiableCredential));
            if (type != CredentialType::VerifiableCredential) {
                vc.types_.push_back(static_cast<dp::u8>(type));
            }

            vc.issuer_ = dp::String(issuer.toString().c_str());

            // Set timestamps
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            vc.issuance_date_ = now;
            vc.expiration_date_ = 0; // No expiration by default

            // Initialize subject
            vc.subject_ = CredentialSubject(subject.toString());
            vc.status_ = static_cast<dp::u8>(CredentialStatus::Active);

            return vc;
        }

        // === Core Properties ===

        /// Get credential ID
        inline std::string getId() const { return std::string(id_.c_str()); }

        /// Get credential type(s)
        inline std::vector<CredentialType> getTypes() const {
            std::vector<CredentialType> result;
            for (const auto &t : types_) {
                result.push_back(static_cast<CredentialType>(t));
            }
            return result;
        }

        /// Get type strings
        inline std::vector<std::string> getTypeStrings() const {
            std::vector<std::string> result;
            for (const auto &t : types_) {
                result.push_back(credentialTypeToString(static_cast<CredentialType>(t)));
            }
            return result;
        }

        /// Check if has a specific type
        inline bool hasType(CredentialType type) const {
            for (const auto &t : types_) {
                if (static_cast<CredentialType>(t) == type) {
                    return true;
                }
            }
            return false;
        }

        /// Add a type
        inline void addType(CredentialType type) {
            if (!hasType(type)) {
                types_.push_back(static_cast<dp::u8>(type));
            }
        }

        /// Get issuer DID
        inline DID getIssuer() const {
            auto result = DID::parse(std::string(issuer_.c_str()));
            if (result.is_ok()) {
                return result.value();
            }
            return DID();
        }

        /// Get issuer as string
        inline std::string getIssuerString() const { return std::string(issuer_.c_str()); }

        /// Get issuance date (milliseconds since epoch)
        inline dp::i64 getIssuanceDate() const { return issuance_date_; }

        /// Get expiration date (milliseconds since epoch, 0 = no expiration)
        inline dp::i64 getExpirationDate() const { return expiration_date_; }

        /// Set expiration date
        inline void setExpirationDate(dp::i64 expiration) { expiration_date_ = expiration; }

        /// Set expiration from duration
        inline void setExpiresIn(std::chrono::milliseconds duration) {
            auto now = std::chrono::system_clock::now();
            auto exp = now + duration;
            expiration_date_ = std::chrono::duration_cast<std::chrono::milliseconds>(exp.time_since_epoch()).count();
        }

        /// Check if expired
        inline bool isExpired() const {
            if (expiration_date_ == 0) {
                return false;
            }
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            return now >= expiration_date_;
        }

        // === Subject ===

        /// Get credential subject
        inline const CredentialSubject &getCredentialSubject() const { return subject_; }

        /// Get subject DID
        inline DID getSubjectDID() const {
            auto result = DID::parse(subject_.getId());
            if (result.is_ok()) {
                return result.value();
            }
            return DID();
        }

        /// Set a claim on the subject
        inline void setClaim(const std::string &key, const std::string &value) { subject_.setClaim(key, value); }

        /// Get a claim value
        inline dp::Result<std::string, dp::Error> getClaim(const std::string &key) const {
            return subject_.getClaim(key);
        }

        /// Check if has a claim
        inline bool hasClaim(const std::string &key) const { return subject_.hasClaim(key); }

        // === Status ===

        /// Get credential status
        inline CredentialStatus getStatus() const { return static_cast<CredentialStatus>(status_); }

        /// Set status
        inline void setStatus(CredentialStatus status) { status_ = static_cast<dp::u8>(status); }

        /// Check if credential is valid (active and not expired)
        inline bool isValid() const { return getStatus() == CredentialStatus::Active && !isExpired(); }

        /// Revoke the credential
        inline void revoke() { status_ = static_cast<dp::u8>(CredentialStatus::Revoked); }

        /// Suspend the credential
        inline void suspend() { status_ = static_cast<dp::u8>(CredentialStatus::Suspended); }

        // === Proof ===

        /// Get the canonical bytes to sign (all fields except proof)
        inline std::vector<uint8_t> getSigningInput() const {
            // Create a copy without proof for signing
            VerifiableCredential vc_copy = *this;
            vc_copy.proof_ = CredentialProof(); // Clear proof

            auto serialized = dp::serialize<dp::Mode::WITH_VERSION>(vc_copy);
            return std::vector<uint8_t>(serialized.begin(), serialized.end());
        }

        /// Sign the credential (creates proof)
        inline dp::Result<void, dp::Error> sign(const Key &issuer_key, const std::string &key_id) {
            // Get bytes to sign
            auto signing_input = getSigningInput();

            // Sign
            auto sig_result = issuer_key.sign(signing_input);
            if (!sig_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(sig_result.error());
            }

            // Create proof
            proof_.type = dp::String("Ed25519Signature2020");

            // ISO8601 timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
            proof_.created = dp::String(ss.str().c_str());

            proof_.verification_method = dp::String(key_id.c_str());
            proof_.proof_purpose = dp::String("assertionMethod");

            auto sig = sig_result.value();
            proof_.proof_value = dp::Vector<dp::u8>(sig.begin(), sig.end());

            return dp::Result<void, dp::Error>::ok();
        }

        /// Verify the credential signature
        inline dp::Result<bool, dp::Error> verify(const Key &issuer_public_key) const {
            if (proof_.isEmpty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Credential has no proof"));
            }

            // Get bytes that were signed
            auto signing_input = getSigningInput();

            // Get signature
            std::vector<uint8_t> signature(proof_.proof_value.begin(), proof_.proof_value.end());

            // Verify
            return issuer_public_key.verify(signing_input, signature);
        }

        /// Get the proof
        inline const CredentialProof &getProof() const { return proof_; }

        /// Check if credential has proof
        inline bool hasProof() const { return !proof_.isEmpty(); }

        // === Serialization ===

        /// Serialize to binary
        inline dp::ByteBuf serialize() const {
            auto &self = const_cast<VerifiableCredential &>(*this);
            return dp::serialize<dp::Mode::WITH_VERSION>(self);
        }

        /// Deserialize from binary
        inline static dp::Result<VerifiableCredential, dp::Error> deserialize(const dp::ByteBuf &data) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, VerifiableCredential>(data);
                return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<VerifiableCredential, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        /// Serialize to JSON-LD format (for interoperability)
        inline std::string toJsonLd() const {
            std::string json = "{\n";
            json += "  \"@context\": [\"https://www.w3.org/2018/credentials/v1\"],\n";
            json += "  \"id\": \"" + std::string(id_.c_str()) + "\",\n";

            // Types
            json += "  \"type\": [";
            auto types = getTypeStrings();
            for (size_t i = 0; i < types.size(); i++) {
                if (i > 0)
                    json += ", ";
                json += "\"" + types[i] + "\"";
            }
            json += "],\n";

            json += "  \"issuer\": \"" + std::string(issuer_.c_str()) + "\",\n";

            // Issuance date as ISO8601
            auto iss_time = std::chrono::system_clock::time_point() + std::chrono::milliseconds(issuance_date_);
            auto iss_time_t = std::chrono::system_clock::to_time_t(iss_time);
            std::stringstream iss_ss;
            iss_ss << std::put_time(std::gmtime(&iss_time_t), "%Y-%m-%dT%H:%M:%SZ");
            json += "  \"issuanceDate\": \"" + iss_ss.str() + "\",\n";

            // Expiration date if set
            if (expiration_date_ > 0) {
                auto exp_time = std::chrono::system_clock::time_point() + std::chrono::milliseconds(expiration_date_);
                auto exp_time_t = std::chrono::system_clock::to_time_t(exp_time);
                std::stringstream exp_ss;
                exp_ss << std::put_time(std::gmtime(&exp_time_t), "%Y-%m-%dT%H:%M:%SZ");
                json += "  \"expirationDate\": \"" + exp_ss.str() + "\",\n";
            }

            // Credential subject
            json += "  \"credentialSubject\": {\n";
            json += "    \"id\": \"" + subject_.getId() + "\"";
            auto claims = subject_.getAllClaims();
            for (const auto &[k, v] : claims) {
                json += ",\n    \"" + k + "\": \"" + v + "\"";
            }
            json += "\n  }";

            // Proof if exists
            if (!proof_.isEmpty()) {
                json += ",\n  \"proof\": {\n";
                json += "    \"type\": \"" + proof_.getType() + "\",\n";
                json += "    \"created\": \"" + proof_.getCreated() + "\",\n";
                json += "    \"verificationMethod\": \"" + proof_.getVerificationMethod() + "\",\n";
                json += "    \"proofPurpose\": \"" + proof_.getProofPurpose() + "\"\n";
                json += "  }";
            }

            json += "\n}";
            return json;
        }

        /// Datapod serialization
        auto members() {
            return std::tie(id_, types_, issuer_, issuance_date_, expiration_date_, subject_, status_, proof_);
        }
        auto members() const {
            return std::tie(id_, types_, issuer_, issuance_date_, expiration_date_, subject_, status_, proof_);
        }

      private:
        dp::String id_;
        dp::Vector<dp::u8> types_; // CredentialType values
        dp::String issuer_;
        dp::i64 issuance_date_{0};
        dp::i64 expiration_date_{0};
        CredentialSubject subject_;
        dp::u8 status_{0};
        CredentialProof proof_;
    };

} // namespace blockit
