#include "blockit/identity/did_document.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("DID Document Serialization Tests") {

    TEST_CASE("Basic serialization round-trip") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());

        // Serialize
        auto serialized = doc.serialize();
        CHECK(serialized.size() > 0);

        // Deserialize
        auto deserialized_result = DIDDocument::deserialize(serialized);
        REQUIRE(deserialized_result.is_ok());

        auto restored = deserialized_result.value();
        CHECK(restored.getIdString() == doc.getIdString());
        CHECK(restored.getVersion() == doc.getVersion());
        CHECK(restored.isActive() == doc.isActive());
    }

    TEST_CASE("Serialization preserves verification methods") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto doc = DIDDocument::create(key1_result.value());
        auto did = doc.getId();

        // Add second verification method
        auto vm2 = VerificationMethod::fromKey(did, "key-2", key2_result.value());
        doc.addVerificationMethod(vm2);

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        CHECK(restored.getVerificationMethods().size() == 2);
        CHECK(restored.hasVerificationMethod(did.withFragment("key-1")));
        CHECK(restored.hasVerificationMethod(did.withFragment("key-2")));
    }

    TEST_CASE("Serialization preserves services") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        // Add services
        doc.addService(Service(did.withFragment("swarm"), ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000"));
        doc.addService(Service(did.withFragment("custom"), "RobotTelemetry", "https://api.example.com"));

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        CHECK(restored.getServices().size() == 2);
        CHECK(restored.hasService(did.withFragment("swarm")));
        CHECK(restored.hasService(did.withFragment("custom")));

        auto svc = restored.getService(did.withFragment("swarm")).value();
        CHECK(svc.getServiceType() == ServiceType::SwarmCoordinator);
        CHECK(svc.getServiceEndpoint() == "udp://192.168.1.100:9000");
    }

    TEST_CASE("Serialization preserves controllers") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto doc = DIDDocument::create(key1_result.value());
        auto did2 = DID::fromKey(key2_result.value());
        doc.addController(did2);

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        CHECK(restored.getControllers().size() == 2);
        CHECK(restored.isController(did2));
    }

    TEST_CASE("Serialization preserves verification relationships") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        doc.addKeyAgreement(did.withFragment("key-1"));
        doc.addCapabilityInvocation(did.withFragment("key-1"));
        doc.addCapabilityDelegation(did.withFragment("key-1"));

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        CHECK(restored.getAuthentication().size() == 1);
        CHECK(restored.getAssertionMethod().size() == 1);
        CHECK(restored.getKeyAgreement().size() == 1);
        CHECK(restored.getCapabilityInvocation().size() == 1);
        CHECK(restored.getCapabilityDelegation().size() == 1);
    }

    TEST_CASE("Serialization preserves also-known-as") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        doc.addAlsoKnownAs("https://example.com/robot");
        doc.addAlsoKnownAs("urn:uuid:12345");

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        auto akas = restored.getAlsoKnownAs();
        REQUIRE(akas.size() == 2);
        CHECK(akas[0] == "https://example.com/robot");
        CHECK(akas[1] == "urn:uuid:12345");
    }

    TEST_CASE("Serialization preserves deactivated status") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        doc.deactivate();

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        CHECK_FALSE(restored.isActive());
        CHECK(restored.getStatus() == DIDDocumentStatus::Deactivated);
    }

    TEST_CASE("Serialization preserves timestamps") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto created = doc.getCreated();
        auto updated = doc.getUpdated();

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        CHECK(restored.getCreated() == created);
        CHECK(restored.getUpdated() == updated);
    }

    TEST_CASE("JSON-LD output is valid") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        // Add a service
        doc.addService(Service(did.withFragment("swarm"), ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000"));

        auto json = doc.toJsonLd();

        // Basic validation
        CHECK(json.find("@context") != std::string::npos);
        CHECK(json.find("https://www.w3.org/ns/did/v1") != std::string::npos);
        CHECK(json.find("\"id\":") != std::string::npos);
        CHECK(json.find("did:blockit:") != std::string::npos);
        CHECK(json.find("verificationMethod") != std::string::npos);
        CHECK(json.find("Ed25519VerificationKey2020") != std::string::npos);
        CHECK(json.find("authentication") != std::string::npos);
        CHECK(json.find("service") != std::string::npos);
        CHECK(json.find("SwarmCoordinator") != std::string::npos);
    }

    TEST_CASE("JSON-LD includes all verification relationships") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        doc.addKeyAgreement(did.withFragment("key-1"));
        doc.addCapabilityInvocation(did.withFragment("key-1"));
        doc.addCapabilityDelegation(did.withFragment("key-1"));

        auto json = doc.toJsonLd();

        CHECK(json.find("\"authentication\"") != std::string::npos);
        CHECK(json.find("\"assertionMethod\"") != std::string::npos);
        CHECK(json.find("\"keyAgreement\"") != std::string::npos);
        CHECK(json.find("\"capabilityInvocation\"") != std::string::npos);
        CHECK(json.find("\"capabilityDelegation\"") != std::string::npos);
    }

    TEST_CASE("Deserialize from raw bytes") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());

        // Serialize
        auto serialized = doc.serialize();

        // Deserialize from raw bytes
        auto restored_result = DIDDocument::deserialize(serialized.data(), serialized.size());
        REQUIRE(restored_result.is_ok());

        CHECK(restored_result.value().getIdString() == doc.getIdString());
    }

    TEST_CASE("Complex document round-trip") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        auto key3_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());
        REQUIRE(key3_result.is_ok());

        auto doc = DIDDocument::create(key1_result.value());
        auto did = doc.getId();
        auto controller_did = DID::fromKey(key3_result.value());

        // Add everything
        doc.addController(controller_did);
        doc.addAlsoKnownAs("https://robot.example.com");

        auto vm2 = VerificationMethod::fromKey(did, "key-2", key2_result.value());
        doc.addVerificationMethod(vm2);
        doc.addAuthentication(did.withFragment("key-2"));
        doc.addKeyAgreement(did.withFragment("key-2"));

        doc.addService(Service(did.withFragment("swarm"), ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000"));
        doc.addService(Service(did.withFragment("creds"), ServiceType::CredentialRepository, "https://creds.example.com"));
        doc.addService(Service(did.withFragment("custom"), "CustomService", "mqtt://broker.example.com"));

        // Serialize and deserialize
        auto serialized = doc.serialize();
        auto restored = DIDDocument::deserialize(serialized).value();

        // Verify everything
        CHECK(restored.getIdString() == doc.getIdString());
        CHECK(restored.getControllers().size() == 2);
        CHECK(restored.getAlsoKnownAs().size() == 1);
        CHECK(restored.getVerificationMethods().size() == 2);
        CHECK(restored.getAuthentication().size() == 2);
        CHECK(restored.getKeyAgreement().size() == 1);
        CHECK(restored.getServices().size() == 3);
        CHECK(restored.getVersion() == doc.getVersion());
    }
}
