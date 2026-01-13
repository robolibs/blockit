#include "blockit/identity/verification_method.hpp"
#include "blockit/ledger/key.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("Verification Method Tests") {

    TEST_CASE("Create verification method from Key") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto vm = VerificationMethod::fromKey(did, "key-1", key_result.value());

        CHECK(vm.getId() == did.withFragment("key-1"));
        CHECK(vm.getController() == did.toString());
        CHECK(vm.getType() == VerificationMethodType::Ed25519VerificationKey2020);
        CHECK(vm.getPublicKeyBytes() == key_result.value().getPublicKey());
    }

    TEST_CASE("Create verification method with different type") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto vm =
            VerificationMethod::fromKey(did, "key-agreement-1", key_result.value(), VerificationMethodType::X25519KeyAgreementKey2020);

        CHECK(vm.getType() == VerificationMethodType::X25519KeyAgreementKey2020);
        CHECK(vm.getTypeString() == "X25519KeyAgreementKey2020");
    }

    TEST_CASE("Verification method type to string") {
        CHECK(verificationMethodTypeToString(VerificationMethodType::Ed25519VerificationKey2020) ==
              "Ed25519VerificationKey2020");
        CHECK(verificationMethodTypeToString(VerificationMethodType::X25519KeyAgreementKey2020) ==
              "X25519KeyAgreementKey2020");
    }

    TEST_CASE("Verification relationship to string") {
        CHECK(verificationRelationshipToString(VerificationRelationship::Authentication) == "authentication");
        CHECK(verificationRelationshipToString(VerificationRelationship::AssertionMethod) == "assertionMethod");
        CHECK(verificationRelationshipToString(VerificationRelationship::KeyAgreement) == "keyAgreement");
        CHECK(verificationRelationshipToString(VerificationRelationship::CapabilityInvocation) == "capabilityInvocation");
        CHECK(verificationRelationshipToString(VerificationRelationship::CapabilityDelegation) == "capabilityDelegation");
    }

    TEST_CASE("Get public key as multibase") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto vm = VerificationMethod::fromKey(did, "key-1", key_result.value());

        auto multibase = vm.getPublicKeyMultibase();
        CHECK_FALSE(multibase.empty());
        CHECK(multibase[0] == 'z'); // z prefix for base58btc
    }

    TEST_CASE("Convert verification method to Key for verification") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto vm = VerificationMethod::fromKey(did, "key-1", key_result.value());

        auto key_from_vm = vm.toKey();
        REQUIRE(key_from_vm.is_ok());

        // The key from verification method should have the same public key
        CHECK(key_from_vm.value().getPublicKey() == key_result.value().getPublicKey());

        // But no private key (verification only)
        CHECK_FALSE(key_from_vm.value().hasPrivateKey());
    }

    TEST_CASE("Verification method equality") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto vm1 = VerificationMethod::fromKey(did, "key-1", key_result.value());
        auto vm2 = VerificationMethod::fromKey(did, "key-1", key_result.value());
        auto vm3 = VerificationMethod::fromKey(did, "key-2", key_result.value());

        CHECK(vm1 == vm2);
        CHECK(vm1 != vm3);
    }

    TEST_CASE("Verification method with signature verification") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto vm = VerificationMethod::fromKey(did, "key-1", key_result.value());

        // Sign some data with the original key
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        auto sig_result = key_result.value().sign(data);
        REQUIRE(sig_result.is_ok());

        // Verify with the key extracted from verification method
        auto verify_key = vm.toKey();
        REQUIRE(verify_key.is_ok());

        auto verify_result = verify_key.value().verify(data, sig_result.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Create verification method with constructor") {
        std::vector<uint8_t> public_key(32, 0x42); // 32 bytes of 0x42
        std::string id = "did:blockit:abc123#key-1";
        std::string controller = "did:blockit:abc123";

        VerificationMethod vm(id, VerificationMethodType::Ed25519VerificationKey2020, controller, public_key);

        CHECK(vm.getId() == id);
        CHECK(vm.getController() == controller);
        CHECK(vm.getType() == VerificationMethodType::Ed25519VerificationKey2020);
        CHECK(vm.getPublicKeyBytes() == public_key);
    }

    TEST_CASE("Empty verification method fails toKey") {
        VerificationMethod empty_vm;
        auto key_result = empty_vm.toKey();
        CHECK(key_result.is_err());
    }

    TEST_CASE("Verification method serialization round-trip") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto vm = VerificationMethod::fromKey(did, "key-1", key_result.value());

        // Serialize
        auto serialized = dp::serialize<dp::Mode::WITH_VERSION>(vm);

        // Deserialize
        auto deserialized = dp::deserialize<dp::Mode::WITH_VERSION, VerificationMethod>(serialized);

        CHECK(deserialized.getId() == vm.getId());
        CHECK(deserialized.getController() == vm.getController());
        CHECK(deserialized.getType() == vm.getType());
        CHECK(deserialized.getPublicKeyBytes() == vm.getPublicKeyBytes());
    }
}
