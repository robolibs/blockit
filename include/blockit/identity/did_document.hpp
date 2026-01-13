#pragma once

#include "did.hpp"
#include "service.hpp"
#include "verification_method.hpp"
#include <algorithm>
#include <chrono>
#include <datapod/datapod.hpp>

namespace blockit {

    /// DID Document status
    enum class DIDDocumentStatus : dp::u8 {
        Active = 0,
        Deactivated = 1,
    };

    /// DID Document following W3C DID Core v1.0
    /// This is the core identity document that describes the DID subject
    class DIDDocument {
      public:
        DIDDocument() = default;

        /// Create a new DID Document with initial key
        inline static DIDDocument create(const Key &key) {
            DID did = DID::fromKey(key);
            return create(did, key);
        }

        /// Create with custom DID (for testing or importing)
        inline static DIDDocument create(const DID &did, const Key &key) {
            DIDDocument doc;
            doc.did_ = dp::String(did.toString().c_str());

            // Set controller to self by default
            doc.controllers_.push_back(dp::String(did.toString().c_str()));

            // Add initial verification method
            auto vm = VerificationMethod::fromKey(did, "key-1", key);
            doc.verification_methods_.push_back(vm);

            // Add key reference to authentication and assertionMethod
            doc.authentication_.push_back(dp::String(did.withFragment("key-1").c_str()));
            doc.assertion_method_.push_back(dp::String(did.withFragment("key-1").c_str()));

            // Set timestamps
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            doc.created_ = now;
            doc.updated_ = now;
            doc.version_ = 1;
            doc.status_ = static_cast<dp::u8>(DIDDocumentStatus::Active);

            return doc;
        }

        // === Core Properties ===

        /// Get the DID (id property)
        inline DID getId() const {
            auto result = DID::parse(std::string(did_.c_str()));
            if (result.is_ok()) {
                return result.value();
            }
            return DID();
        }

        /// Get the DID as string
        inline std::string getIdString() const { return std::string(did_.c_str()); }

        /// Get controller DID(s)
        inline std::vector<DID> getControllers() const {
            std::vector<DID> result;
            for (const auto &c : controllers_) {
                auto parsed = DID::parse(std::string(c.c_str()));
                if (parsed.is_ok()) {
                    result.push_back(parsed.value());
                }
            }
            return result;
        }

        /// Get controller DIDs as strings
        inline std::vector<std::string> getControllerStrings() const {
            std::vector<std::string> result;
            for (const auto &c : controllers_) {
                result.push_back(std::string(c.c_str()));
            }
            return result;
        }

        /// Set controller(s) - replaces existing
        inline void setController(const DID &controller) {
            controllers_.clear();
            controllers_.push_back(dp::String(controller.toString().c_str()));
            touch();
        }

        /// Add a controller
        inline void addController(const DID &controller) {
            controllers_.push_back(dp::String(controller.toString().c_str()));
            touch();
        }

        /// Check if a DID is a controller
        inline bool isController(const DID &did) const {
            std::string did_str = did.toString();
            for (const auto &c : controllers_) {
                if (std::string(c.c_str()) == did_str) {
                    return true;
                }
            }
            return false;
        }

        /// Get also-known-as identifiers
        inline std::vector<std::string> getAlsoKnownAs() const {
            std::vector<std::string> result;
            for (const auto &aka : also_known_as_) {
                result.push_back(std::string(aka.c_str()));
            }
            return result;
        }

        /// Add an also-known-as identifier
        inline void addAlsoKnownAs(const std::string &aka) {
            also_known_as_.push_back(dp::String(aka.c_str()));
            touch();
        }

        // === Verification Methods ===

        /// Get all verification methods
        inline const dp::Vector<VerificationMethod> &getVerificationMethods() const { return verification_methods_; }

        /// Get verification methods as vector
        inline std::vector<VerificationMethod> getVerificationMethodsVec() const {
            return std::vector<VerificationMethod>(verification_methods_.begin(), verification_methods_.end());
        }

        /// Add a verification method
        inline void addVerificationMethod(const VerificationMethod &method) {
            verification_methods_.push_back(method);
            touch();
        }

        /// Remove a verification method by ID
        inline dp::Result<void, dp::Error> removeVerificationMethod(const std::string &id) {
            auto it = std::find_if(verification_methods_.begin(), verification_methods_.end(),
                                   [&id](const VerificationMethod &vm) { return vm.getId() == id; });

            if (it == verification_methods_.end()) {
                return dp::Result<void, dp::Error>::err(dp::Error::not_found("Verification method not found"));
            }

            verification_methods_.erase(it);

            // Also remove from all relationships
            removeFromRelationship(authentication_, id);
            removeFromRelationship(assertion_method_, id);
            removeFromRelationship(key_agreement_, id);
            removeFromRelationship(capability_invocation_, id);
            removeFromRelationship(capability_delegation_, id);

            touch();
            return dp::Result<void, dp::Error>::ok();
        }

        /// Get verification method by ID
        inline dp::Result<VerificationMethod, dp::Error> getVerificationMethod(const std::string &id) const {
            for (const auto &vm : verification_methods_) {
                if (vm.getId() == id) {
                    return dp::Result<VerificationMethod, dp::Error>::ok(vm);
                }
            }
            return dp::Result<VerificationMethod, dp::Error>::err(
                dp::Error::not_found("Verification method not found"));
        }

        /// Check if a verification method exists
        inline bool hasVerificationMethod(const std::string &id) const {
            for (const auto &vm : verification_methods_) {
                if (vm.getId() == id) {
                    return true;
                }
            }
            return false;
        }

        // === Verification Relationships ===

        /// Add key reference to authentication
        inline void addAuthentication(const std::string &key_id) {
            authentication_.push_back(dp::String(key_id.c_str()));
            touch();
        }

        /// Add key reference to assertionMethod
        inline void addAssertionMethod(const std::string &key_id) {
            assertion_method_.push_back(dp::String(key_id.c_str()));
            touch();
        }

        /// Add key reference to keyAgreement
        inline void addKeyAgreement(const std::string &key_id) {
            key_agreement_.push_back(dp::String(key_id.c_str()));
            touch();
        }

        /// Add key reference to capabilityInvocation
        inline void addCapabilityInvocation(const std::string &key_id) {
            capability_invocation_.push_back(dp::String(key_id.c_str()));
            touch();
        }

        /// Add key reference to capabilityDelegation
        inline void addCapabilityDelegation(const std::string &key_id) {
            capability_delegation_.push_back(dp::String(key_id.c_str()));
            touch();
        }

        /// Get authentication key IDs
        inline std::vector<std::string> getAuthentication() const { return toStringVector(authentication_); }

        /// Get assertionMethod key IDs
        inline std::vector<std::string> getAssertionMethod() const { return toStringVector(assertion_method_); }

        /// Get keyAgreement key IDs
        inline std::vector<std::string> getKeyAgreement() const { return toStringVector(key_agreement_); }

        /// Get capabilityInvocation key IDs
        inline std::vector<std::string> getCapabilityInvocation() const {
            return toStringVector(capability_invocation_);
        }

        /// Get capabilityDelegation key IDs
        inline std::vector<std::string> getCapabilityDelegation() const {
            return toStringVector(capability_delegation_);
        }

        /// Check if a key is used for authentication
        inline bool canAuthenticate(const std::string &key_id) const { return containsKeyId(authentication_, key_id); }

        /// Check if a key is used for assertions
        inline bool canAssert(const std::string &key_id) const { return containsKeyId(assertion_method_, key_id); }

        // === Services ===

        /// Get all services
        inline const dp::Vector<Service> &getServices() const { return services_; }

        /// Get services as vector
        inline std::vector<Service> getServicesVec() const {
            return std::vector<Service>(services_.begin(), services_.end());
        }

        /// Add a service endpoint
        inline void addService(const Service &service) {
            services_.push_back(service);
            touch();
        }

        /// Remove a service by ID
        inline dp::Result<void, dp::Error> removeService(const std::string &id) {
            auto it =
                std::find_if(services_.begin(), services_.end(), [&id](const Service &s) { return s.getId() == id; });

            if (it == services_.end()) {
                return dp::Result<void, dp::Error>::err(dp::Error::not_found("Service not found"));
            }

            services_.erase(it);
            touch();
            return dp::Result<void, dp::Error>::ok();
        }

        /// Get service by ID
        inline dp::Result<Service, dp::Error> getService(const std::string &id) const {
            for (const auto &s : services_) {
                if (s.getId() == id) {
                    return dp::Result<Service, dp::Error>::ok(s);
                }
            }
            return dp::Result<Service, dp::Error>::err(dp::Error::not_found("Service not found"));
        }

        /// Check if a service exists
        inline bool hasService(const std::string &id) const {
            for (const auto &s : services_) {
                if (s.getId() == id) {
                    return true;
                }
            }
            return false;
        }

        // === Document Management ===

        /// Get document status
        inline DIDDocumentStatus getStatus() const { return static_cast<DIDDocumentStatus>(status_); }

        /// Deactivate the DID Document
        inline void deactivate() {
            status_ = static_cast<dp::u8>(DIDDocumentStatus::Deactivated);
            touch();
        }

        /// Check if active
        inline bool isActive() const { return status_ == static_cast<dp::u8>(DIDDocumentStatus::Active); }

        /// Get creation timestamp (milliseconds since epoch)
        inline dp::i64 getCreated() const { return created_; }

        /// Get last updated timestamp (milliseconds since epoch)
        inline dp::i64 getUpdated() const { return updated_; }

        /// Update the updated timestamp and increment version
        inline void touch() {
            updated_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            version_++;
        }

        /// Get version number (increments on each update)
        inline dp::u32 getVersion() const { return version_; }

        // === Serialization ===

        /// Serialize to binary using datapod
        inline dp::ByteBuf serialize() const {
            auto &self = const_cast<DIDDocument &>(*this);
            return dp::serialize<dp::Mode::WITH_VERSION>(self);
        }

        /// Deserialize from binary using datapod
        inline static dp::Result<DIDDocument, dp::Error> deserialize(const dp::ByteBuf &data) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, DIDDocument>(data);
                return dp::Result<DIDDocument, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<DIDDocument, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        /// Deserialize from raw bytes
        inline static dp::Result<DIDDocument, dp::Error> deserialize(const dp::u8 *data, dp::usize size) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, DIDDocument>(data, size);
                return dp::Result<DIDDocument, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<DIDDocument, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        /// Serialize to JSON-LD format (for interoperability)
        inline std::string toJsonLd() const {
            std::string json = "{\n";
            json += "  \"@context\": [\"https://www.w3.org/ns/did/v1\"],\n";
            json += "  \"id\": \"" + std::string(did_.c_str()) + "\",\n";

            // Controllers
            if (!controllers_.empty()) {
                if (controllers_.size() == 1) {
                    json += "  \"controller\": \"" + std::string(controllers_[0].c_str()) + "\",\n";
                } else {
                    json += "  \"controller\": [";
                    for (size_t i = 0; i < controllers_.size(); i++) {
                        if (i > 0)
                            json += ", ";
                        json += "\"" + std::string(controllers_[i].c_str()) + "\"";
                    }
                    json += "],\n";
                }
            }

            // Also known as
            if (!also_known_as_.empty()) {
                json += "  \"alsoKnownAs\": [";
                for (size_t i = 0; i < also_known_as_.size(); i++) {
                    if (i > 0)
                        json += ", ";
                    json += "\"" + std::string(also_known_as_[i].c_str()) + "\"";
                }
                json += "],\n";
            }

            // Verification methods
            if (!verification_methods_.empty()) {
                json += "  \"verificationMethod\": [\n";
                for (size_t i = 0; i < verification_methods_.size(); i++) {
                    const auto &vm = verification_methods_[i];
                    json += "    {\n";
                    json += "      \"id\": \"" + vm.getId() + "\",\n";
                    json += "      \"type\": \"" + vm.getTypeString() + "\",\n";
                    json += "      \"controller\": \"" + vm.getController() + "\",\n";
                    json += "      \"publicKeyMultibase\": \"" + vm.getPublicKeyMultibase() + "\"\n";
                    json += "    }";
                    if (i < verification_methods_.size() - 1)
                        json += ",";
                    json += "\n";
                }
                json += "  ],\n";
            }

            // Authentication
            if (!authentication_.empty()) {
                json += "  \"authentication\": [";
                for (size_t i = 0; i < authentication_.size(); i++) {
                    if (i > 0)
                        json += ", ";
                    json += "\"" + std::string(authentication_[i].c_str()) + "\"";
                }
                json += "],\n";
            }

            // Assertion method
            if (!assertion_method_.empty()) {
                json += "  \"assertionMethod\": [";
                for (size_t i = 0; i < assertion_method_.size(); i++) {
                    if (i > 0)
                        json += ", ";
                    json += "\"" + std::string(assertion_method_[i].c_str()) + "\"";
                }
                json += "],\n";
            }

            // Key agreement
            if (!key_agreement_.empty()) {
                json += "  \"keyAgreement\": [";
                for (size_t i = 0; i < key_agreement_.size(); i++) {
                    if (i > 0)
                        json += ", ";
                    json += "\"" + std::string(key_agreement_[i].c_str()) + "\"";
                }
                json += "],\n";
            }

            // Capability invocation
            if (!capability_invocation_.empty()) {
                json += "  \"capabilityInvocation\": [";
                for (size_t i = 0; i < capability_invocation_.size(); i++) {
                    if (i > 0)
                        json += ", ";
                    json += "\"" + std::string(capability_invocation_[i].c_str()) + "\"";
                }
                json += "],\n";
            }

            // Capability delegation
            if (!capability_delegation_.empty()) {
                json += "  \"capabilityDelegation\": [";
                for (size_t i = 0; i < capability_delegation_.size(); i++) {
                    if (i > 0)
                        json += ", ";
                    json += "\"" + std::string(capability_delegation_[i].c_str()) + "\"";
                }
                json += "],\n";
            }

            // Services
            if (!services_.empty()) {
                json += "  \"service\": [\n";
                for (size_t i = 0; i < services_.size(); i++) {
                    const auto &svc = services_[i];
                    json += "    {\n";
                    json += "      \"id\": \"" + svc.getId() + "\",\n";
                    json += "      \"type\": \"" + svc.getTypeString() + "\",\n";
                    json += "      \"serviceEndpoint\": \"" + svc.getServiceEndpoint() + "\"\n";
                    json += "    }";
                    if (i < services_.size() - 1)
                        json += ",";
                    json += "\n";
                }
                json += "  ],\n";
            }

            // Remove trailing comma and close
            if (json.size() >= 2 && json.substr(json.size() - 2) == ",\n") {
                json = json.substr(0, json.size() - 2) + "\n";
            }
            json += "}";

            return json;
        }

        /// Datapod serialization
        auto members() {
            return std::tie(did_, controllers_, also_known_as_, verification_methods_, authentication_,
                            assertion_method_, key_agreement_, capability_invocation_, capability_delegation_,
                            services_, status_, created_, updated_, version_);
        }
        auto members() const {
            return std::tie(did_, controllers_, also_known_as_, verification_methods_, authentication_,
                            assertion_method_, key_agreement_, capability_invocation_, capability_delegation_,
                            services_, status_, created_, updated_, version_);
        }

      private:
        dp::String did_;                       // The DID string
        dp::Vector<dp::String> controllers_;   // Controller DIDs
        dp::Vector<dp::String> also_known_as_; // Alternative identifiers

        dp::Vector<VerificationMethod> verification_methods_;

        // Verification relationships (store key IDs as references)
        dp::Vector<dp::String> authentication_;
        dp::Vector<dp::String> assertion_method_;
        dp::Vector<dp::String> key_agreement_;
        dp::Vector<dp::String> capability_invocation_;
        dp::Vector<dp::String> capability_delegation_;

        dp::Vector<Service> services_;

        dp::u8 status_{0}; // DIDDocumentStatus
        dp::i64 created_{0};
        dp::i64 updated_{0};
        dp::u32 version_{1};

        // Helper to convert dp::Vector<dp::String> to std::vector<std::string>
        inline std::vector<std::string> toStringVector(const dp::Vector<dp::String> &vec) const {
            std::vector<std::string> result;
            for (const auto &s : vec) {
                result.push_back(std::string(s.c_str()));
            }
            return result;
        }

        // Helper to check if a key ID is in a relationship vector
        inline bool containsKeyId(const dp::Vector<dp::String> &vec, const std::string &key_id) const {
            for (const auto &s : vec) {
                if (std::string(s.c_str()) == key_id) {
                    return true;
                }
            }
            return false;
        }

        // Helper to remove a key ID from a relationship vector
        inline void removeFromRelationship(dp::Vector<dp::String> &vec, const std::string &key_id) {
            auto it = std::find_if(vec.begin(), vec.end(),
                                   [&key_id](const dp::String &s) { return std::string(s.c_str()) == key_id; });
            if (it != vec.end()) {
                vec.erase(it);
            }
        }
    };

} // namespace blockit
