#pragma once

#include "did.hpp"
#include <blockit/key.hpp>
#include <datapod/datapod.hpp>

namespace blockit {

    /// Verification method types supported by did:blockit
    enum class VerificationMethodType : dp::u8 {
        Ed25519VerificationKey2020 = 0, // For signing/authentication
        X25519KeyAgreementKey2020 = 1,  // For encryption (future)
    };

    /// Verification relationship types (how the key can be used)
    enum class VerificationRelationship : dp::u8 {
        Authentication = 0,       // Prove control of DID
        AssertionMethod = 1,      // Issue claims/credentials
        KeyAgreement = 2,         // Establish encrypted channel
        CapabilityInvocation = 3, // Invoke capabilities
        CapabilityDelegation = 4, // Delegate capabilities
    };

    /// Get string name for verification method type
    inline std::string verificationMethodTypeToString(VerificationMethodType type) {
        switch (type) {
        case VerificationMethodType::Ed25519VerificationKey2020:
            return "Ed25519VerificationKey2020";
        case VerificationMethodType::X25519KeyAgreementKey2020:
            return "X25519KeyAgreementKey2020";
        default:
            return "Unknown";
        }
    }

    /// Get string name for verification relationship
    inline std::string verificationRelationshipToString(VerificationRelationship rel) {
        switch (rel) {
        case VerificationRelationship::Authentication:
            return "authentication";
        case VerificationRelationship::AssertionMethod:
            return "assertionMethod";
        case VerificationRelationship::KeyAgreement:
            return "keyAgreement";
        case VerificationRelationship::CapabilityInvocation:
            return "capabilityInvocation";
        case VerificationRelationship::CapabilityDelegation:
            return "capabilityDelegation";
        default:
            return "unknown";
        }
    }

    /// A verification method represents cryptographic material for DID operations
    /// Following W3C DID Core v1.0 specification
    struct VerificationMethod {
        dp::String id;                 // e.g., "did:blockit:xxx#key-1"
        dp::u8 type{0};                // VerificationMethodType
        dp::String controller;         // DID that controls this key
        dp::Vector<dp::u8> public_key; // Ed25519 public key (32 bytes)

        VerificationMethod() = default;

        /// Create a verification method from parameters
        inline VerificationMethod(const std::string &id, VerificationMethodType type, const std::string &controller,
                                  const std::vector<uint8_t> &public_key)
            : id(dp::String(id.c_str())), type(static_cast<dp::u8>(type)), controller(dp::String(controller.c_str())),
              public_key(dp::Vector<dp::u8>(public_key.begin(), public_key.end())) {}

        /// Create from a Key
        inline static VerificationMethod
        fromKey(const DID &controller_did, const std::string &fragment, const Key &key,
                VerificationMethodType type = VerificationMethodType::Ed25519VerificationKey2020) {
            VerificationMethod vm;
            vm.id = dp::String(controller_did.withFragment(fragment).c_str());
            vm.type = static_cast<dp::u8>(type);
            vm.controller = dp::String(controller_did.toString().c_str());

            const auto &pub_key = key.getPublicKey();
            vm.public_key = dp::Vector<dp::u8>(pub_key.begin(), pub_key.end());

            return vm;
        }

        /// Get the ID as string
        inline std::string getId() const { return std::string(id.c_str()); }

        /// Get the controller as string
        inline std::string getController() const { return std::string(controller.c_str()); }

        /// Get the type as enum
        inline VerificationMethodType getType() const { return static_cast<VerificationMethodType>(type); }

        /// Get the type as string
        inline std::string getTypeString() const { return verificationMethodTypeToString(getType()); }

        /// Get public key as vector
        inline std::vector<uint8_t> getPublicKeyBytes() const {
            return std::vector<uint8_t>(public_key.begin(), public_key.end());
        }

        /// Get the public key as multibase-encoded string (z-base58)
        /// Using 'z' prefix for base58btc encoding per multibase spec
        inline std::string getPublicKeyMultibase() const {
            if (public_key.empty()) {
                return "";
            }

            // Simple base58 encoding (Bitcoin alphabet)
            static const char *ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

            std::vector<uint8_t> input(public_key.begin(), public_key.end());
            std::string result;

            // Count leading zeros
            size_t leading_zeros = 0;
            for (auto b : input) {
                if (b == 0)
                    leading_zeros++;
                else
                    break;
            }

            // Base58 encoding
            std::vector<uint8_t> digits;
            for (uint8_t byte : input) {
                int carry = byte;
                for (auto &digit : digits) {
                    carry += digit * 256;
                    digit = carry % 58;
                    carry /= 58;
                }
                while (carry > 0) {
                    digits.push_back(carry % 58);
                    carry /= 58;
                }
            }

            // Add leading '1's for zeros
            for (size_t i = 0; i < leading_zeros; i++) {
                result += '1';
            }

            // Convert digits to characters (reverse order)
            for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
                result += ALPHABET[*it];
            }

            // Prefix with 'z' for base58btc
            return "z" + result;
        }

        /// Create Key object from this verification method (for verification only)
        inline dp::Result<Key, dp::Error> toKey() const {
            if (public_key.empty()) {
                return dp::Result<Key, dp::Error>::err(
                    dp::Error::invalid_argument("No public key in verification method"));
            }

            std::vector<uint8_t> pub_key_vec(public_key.begin(), public_key.end());
            return Key::fromPublicKey(pub_key_vec);
        }

        /// Equality comparison
        inline bool operator==(const VerificationMethod &other) const {
            return std::string(id.c_str()) == std::string(other.id.c_str());
        }

        inline bool operator!=(const VerificationMethod &other) const { return !(*this == other); }

        /// Serialization
        auto members() { return std::tie(id, type, controller, public_key); }
        auto members() const { return std::tie(id, type, controller, public_key); }
    };

} // namespace blockit
