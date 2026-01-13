#pragma once

#include "did_registry.hpp"
#include "verifiable_credential.hpp"
#include <blockit/key.hpp>
#include <chrono>
#include <datapod/datapod.hpp>

namespace blockit {

    /// Verifiable Presentation - bundle of credentials for verification
    /// Following W3C VP Data Model
    class VerifiablePresentation {
      public:
        VerifiablePresentation() = default;

        /// Create a new presentation
        inline static VerifiablePresentation create(const DID &holder) {
            VerifiablePresentation vp;

            // Generate unique ID
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            vp.id_ = dp::String(("urn:uuid:vp-" + std::to_string(now)).c_str());

            vp.holder_ = dp::String(holder.toString().c_str());
            return vp;
        }

        /// Create a new presentation with specific ID
        inline static VerifiablePresentation create(const std::string &id, const DID &holder) {
            VerifiablePresentation vp;
            vp.id_ = dp::String(id.c_str());
            vp.holder_ = dp::String(holder.toString().c_str());
            return vp;
        }

        // === Core Properties ===

        /// Get presentation ID
        inline std::string getId() const { return std::string(id_.c_str()); }

        /// Get holder DID
        inline DID getHolder() const {
            auto result = DID::parse(std::string(holder_.c_str()));
            if (result.is_ok()) {
                return result.value();
            }
            return DID();
        }

        /// Get holder as string
        inline std::string getHolderString() const { return std::string(holder_.c_str()); }

        // === Credentials ===

        /// Add a credential to the presentation
        inline void addCredential(const VerifiableCredential &credential) { credentials_.push_back(credential); }

        /// Get all credentials
        inline const dp::Vector<VerifiableCredential> &getCredentials() const { return credentials_; }

        /// Get credentials as std::vector
        inline std::vector<VerifiableCredential> getCredentialsVector() const {
            std::vector<VerifiableCredential> result;
            for (const auto &cred : credentials_) {
                result.push_back(cred);
            }
            return result;
        }

        /// Get number of credentials
        inline size_t getCredentialCount() const { return credentials_.size(); }

        /// Check if presentation has any credentials
        inline bool hasCredentials() const { return !credentials_.empty(); }

        /// Get credential by index
        inline dp::Result<VerifiableCredential, dp::Error> getCredential(size_t index) const {
            if (index >= credentials_.size()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(
                    dp::Error::out_of_range("Credential index out of range"));
            }
            return dp::Result<VerifiableCredential, dp::Error>::ok(credentials_[index]);
        }

        /// Find credential by ID
        inline dp::Result<VerifiableCredential, dp::Error> findCredentialById(const std::string &id) const {
            for (const auto &cred : credentials_) {
                if (cred.getId() == id) {
                    return dp::Result<VerifiableCredential, dp::Error>::ok(cred);
                }
            }
            return dp::Result<VerifiableCredential, dp::Error>::err(dp::Error::not_found("Credential not found"));
        }

        /// Find credentials by type
        inline std::vector<VerifiableCredential> findCredentialsByType(CredentialType type) const {
            std::vector<VerifiableCredential> result;
            for (const auto &cred : credentials_) {
                if (cred.hasType(type)) {
                    result.push_back(cred);
                }
            }
            return result;
        }

        // === Challenge and Domain ===

        /// Set challenge (for authentication/anti-replay)
        inline void setChallenge(const std::string &challenge) { challenge_ = dp::String(challenge.c_str()); }

        /// Get challenge
        inline std::string getChallenge() const { return std::string(challenge_.c_str()); }

        /// Check if challenge is set
        inline bool hasChallenge() const { return !challenge_.empty(); }

        /// Set domain (for context binding)
        inline void setDomain(const std::string &domain) { domain_ = dp::String(domain.c_str()); }

        /// Get domain
        inline std::string getDomain() const { return std::string(domain_.c_str()); }

        /// Check if domain is set
        inline bool hasDomain() const { return !domain_.empty(); }

        // === Proof ===

        /// Get the canonical bytes to sign (all fields except proof)
        inline std::vector<uint8_t> getSigningInput() const {
            // Create a copy without proof for signing
            VerifiablePresentation vp_copy = *this;
            vp_copy.proof_ = CredentialProof(); // Clear proof

            auto serialized = dp::serialize<dp::Mode::WITH_VERSION>(vp_copy);
            return std::vector<uint8_t>(serialized.begin(), serialized.end());
        }

        /// Sign the presentation
        inline dp::Result<void, dp::Error> sign(const Key &holder_key, const std::string &key_id) {
            // Get bytes to sign
            auto signing_input = getSigningInput();

            // Sign
            auto sig_result = holder_key.sign(signing_input);
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
            proof_.proof_purpose = dp::String("authentication");

            auto sig = sig_result.value();
            proof_.proof_value = dp::Vector<dp::u8>(sig.begin(), sig.end());

            return dp::Result<void, dp::Error>::ok();
        }

        /// Verify the presentation signature only (holder's signature)
        inline dp::Result<bool, dp::Error> verifyPresentationSignature(const Key &holder_public_key) const {
            if (proof_.isEmpty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Presentation has no proof"));
            }

            // Get bytes that were signed
            auto signing_input = getSigningInput();

            // Get signature
            std::vector<uint8_t> signature(proof_.proof_value.begin(), proof_.proof_value.end());

            // Verify
            return holder_public_key.verify(signing_input, signature);
        }

        /// Verify the presentation and all contained credentials using DID registry
        /// This verifies:
        /// 1. Presentation signature by the holder
        /// 2. Each credential's signature by its issuer
        /// 3. Each credential's validity (not expired, not revoked)
        inline dp::Result<bool, dp::Error> verify(const DIDRegistry &registry) const {
            // 1. Verify presentation has proof
            if (proof_.isEmpty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Presentation has no proof"));
            }

            // 2. Resolve holder's DID to get their key
            auto holder_doc_result = registry.resolve(getHolderString());
            if (!holder_doc_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(holder_doc_result.error());
            }

            auto holder_doc = holder_doc_result.value();

            // Get the verification method used
            auto vm_result = holder_doc.getVerificationMethod(proof_.getVerificationMethod());
            if (!vm_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(vm_result.error());
            }

            // Convert to Key
            auto holder_key_result = vm_result.value().toKey();
            if (!holder_key_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(holder_key_result.error());
            }

            // Verify presentation signature
            auto vp_verify_result = verifyPresentationSignature(holder_key_result.value());
            if (!vp_verify_result.is_ok() || !vp_verify_result.value()) {
                return vp_verify_result;
            }

            // 3. Verify each credential
            for (const auto &credential : credentials_) {
                // Check validity
                if (!credential.isValid()) {
                    return dp::Result<bool, dp::Error>::ok(false);
                }

                // Resolve issuer's DID
                auto issuer_doc_result = registry.resolve(credential.getIssuerString());
                if (!issuer_doc_result.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(issuer_doc_result.error());
                }

                auto issuer_doc = issuer_doc_result.value();

                // Get issuer's verification method
                auto cred_vm_result = issuer_doc.getVerificationMethod(credential.getProof().getVerificationMethod());
                if (!cred_vm_result.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(cred_vm_result.error());
                }

                // Convert to Key
                auto issuer_key_result = cred_vm_result.value().toKey();
                if (!issuer_key_result.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(issuer_key_result.error());
                }

                // Verify credential signature
                auto cred_verify_result = credential.verify(issuer_key_result.value());
                if (!cred_verify_result.is_ok() || !cred_verify_result.value()) {
                    return cred_verify_result;
                }
            }

            return dp::Result<bool, dp::Error>::ok(true);
        }

        /// Verify presentation signature with holder's key directly (without registry lookup)
        inline dp::Result<bool, dp::Error> verifyWithKey(const Key &holder_key) const {
            return verifyPresentationSignature(holder_key);
        }

        /// Get the proof
        inline const CredentialProof &getProof() const { return proof_; }

        /// Check if presentation has proof
        inline bool hasProof() const { return !proof_.isEmpty(); }

        // === Serialization ===

        /// Serialize to binary
        inline dp::ByteBuf serialize() const {
            auto &self = const_cast<VerifiablePresentation &>(*this);
            return dp::serialize<dp::Mode::WITH_VERSION>(self);
        }

        /// Deserialize from binary
        inline static dp::Result<VerifiablePresentation, dp::Error> deserialize(const dp::ByteBuf &data) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, VerifiablePresentation>(data);
                return dp::Result<VerifiablePresentation, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<VerifiablePresentation, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        /// Serialize to JSON-LD format (for interoperability)
        inline std::string toJsonLd() const {
            std::string json = "{\n";
            json += "  \"@context\": [\"https://www.w3.org/2018/credentials/v1\"],\n";
            json += "  \"id\": \"" + std::string(id_.c_str()) + "\",\n";
            json += "  \"type\": [\"VerifiablePresentation\"],\n";
            json += "  \"holder\": \"" + std::string(holder_.c_str()) + "\"";

            // Verifiable credentials
            if (!credentials_.empty()) {
                json += ",\n  \"verifiableCredential\": [\n";
                for (size_t i = 0; i < credentials_.size(); i++) {
                    if (i > 0)
                        json += ",\n";
                    // Indent credential JSON
                    auto cred_json = credentials_[i].toJsonLd();
                    std::string indented;
                    std::istringstream stream(cred_json);
                    std::string line;
                    while (std::getline(stream, line)) {
                        indented += "    " + line + "\n";
                    }
                    if (!indented.empty() && indented.back() == '\n') {
                        indented.pop_back();
                    }
                    json += indented;
                }
                json += "\n  ]";
            }

            // Challenge and domain
            if (!challenge_.empty()) {
                json += ",\n  \"challenge\": \"" + std::string(challenge_.c_str()) + "\"";
            }
            if (!domain_.empty()) {
                json += ",\n  \"domain\": \"" + std::string(domain_.c_str()) + "\"";
            }

            // Proof
            if (!proof_.isEmpty()) {
                json += ",\n  \"proof\": {\n";
                json += "    \"type\": \"" + proof_.getType() + "\",\n";
                json += "    \"created\": \"" + proof_.getCreated() + "\",\n";
                json += "    \"verificationMethod\": \"" + proof_.getVerificationMethod() + "\",\n";
                json += "    \"proofPurpose\": \"" + proof_.getProofPurpose() + "\"";
                if (!challenge_.empty()) {
                    json += ",\n    \"challenge\": \"" + std::string(challenge_.c_str()) + "\"";
                }
                if (!domain_.empty()) {
                    json += ",\n    \"domain\": \"" + std::string(domain_.c_str()) + "\"";
                }
                json += "\n  }";
            }

            json += "\n}";
            return json;
        }

        /// Datapod serialization
        auto members() { return std::tie(id_, holder_, credentials_, challenge_, domain_, proof_); }
        auto members() const { return std::tie(id_, holder_, credentials_, challenge_, domain_, proof_); }

      private:
        dp::String id_;
        dp::String holder_;
        dp::Vector<VerifiableCredential> credentials_;
        dp::String challenge_;
        dp::String domain_;
        CredentialProof proof_;
    };

} // namespace blockit
