#include "blockit/identity/did_registry.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("DID Registry Tests") {

    TEST_CASE("Create a new DID") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto result = registry.create(key_result.value());
        REQUIRE(result.is_ok());

        auto [doc, op] = result.value();

        CHECK_FALSE(doc.getId().isEmpty());
        CHECK(doc.isActive());
        CHECK(op.getOperationType() == DIDOperationType::Create);
        CHECK(op.getDID() == doc.getIdString());
    }

    TEST_CASE("Create duplicate DID fails") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto result1 = registry.create(key_result.value());
        REQUIRE(result1.is_ok());

        // Same key = same DID
        auto result2 = registry.create(key_result.value());
        CHECK(result2.is_err());
    }

    TEST_CASE("Resolve created DID") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto create_result = registry.create(key_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, op] = create_result.value();
        auto did = doc.getId();

        // Resolve by DID
        auto resolve_result = registry.resolve(did);
        REQUIRE(resolve_result.is_ok());
        CHECK(resolve_result.value().getIdString() == doc.getIdString());

        // Resolve by string
        auto resolve_str_result = registry.resolve(did.toString());
        REQUIRE(resolve_str_result.is_ok());
        CHECK(resolve_str_result.value().getIdString() == doc.getIdString());
    }

    TEST_CASE("Resolve non-existent DID fails") {
        DIDRegistry registry;

        auto result = registry.resolve("did:blockit:0000000000000000000000000000000000000000000000000000000000000000");
        CHECK(result.is_err());
    }

    TEST_CASE("Update DID document") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto create_result = registry.create(key_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, _] = create_result.value();
        auto did = doc.getId();

        // Modify document
        Service svc(did.withFragment("swarm"), ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");
        doc.addService(svc);

        // Update
        auto update_result = registry.update(did, doc, key_result.value());
        REQUIRE(update_result.is_ok());

        auto op = update_result.value();
        CHECK(op.getOperationType() == DIDOperationType::Update);

        // Verify update was applied
        auto resolved = registry.resolve(did).value();
        CHECK(resolved.hasService(did.withFragment("swarm")));
    }

    TEST_CASE("Update by non-controller fails") {
        DIDRegistry registry;

        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto create_result = registry.create(key1_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, _] = create_result.value();
        auto did = doc.getId();

        // Try to update with different key
        auto update_result = registry.update(did, doc, key2_result.value());
        CHECK(update_result.is_err());
    }

    TEST_CASE("Deactivate DID") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto create_result = registry.create(key_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, _] = create_result.value();
        auto did = doc.getId();

        // Deactivate
        auto deactivate_result = registry.deactivate(did, key_result.value());
        REQUIRE(deactivate_result.is_ok());

        auto op = deactivate_result.value();
        CHECK(op.getOperationType() == DIDOperationType::Deactivate);

        // Verify deactivation
        auto resolved = registry.resolve(did).value();
        CHECK_FALSE(resolved.isActive());
    }

    TEST_CASE("Deactivate by non-controller fails") {
        DIDRegistry registry;

        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto create_result = registry.create(key1_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, _] = create_result.value();
        auto did = doc.getId();

        // Try to deactivate with different key
        auto deactivate_result = registry.deactivate(did, key2_result.value());
        CHECK(deactivate_result.is_err());
    }

    TEST_CASE("Cannot update deactivated DID") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto create_result = registry.create(key_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, _] = create_result.value();
        auto did = doc.getId();

        // Deactivate
        registry.deactivate(did, key_result.value());

        // Try to update
        auto update_result = registry.update(did, doc, key_result.value());
        CHECK(update_result.is_err());
    }

    TEST_CASE("Cannot deactivate already deactivated DID") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto create_result = registry.create(key_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, _] = create_result.value();
        auto did = doc.getId();

        // Deactivate twice
        registry.deactivate(did, key_result.value());
        auto result = registry.deactivate(did, key_result.value());
        CHECK(result.is_err());
    }

    TEST_CASE("Check if DID exists") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());

        CHECK_FALSE(registry.exists(did));
        CHECK_FALSE(registry.exists(did.toString()));

        registry.create(key_result.value());

        CHECK(registry.exists(did));
        CHECK(registry.exists(did.toString()));
    }

    TEST_CASE("Get all DIDs") {
        DIDRegistry registry;

        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        auto key3_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());
        REQUIRE(key3_result.is_ok());

        registry.create(key1_result.value());
        registry.create(key2_result.value());
        registry.create(key3_result.value());

        auto all_dids = registry.getAllDIDs();
        CHECK(all_dids.size() == 3);
    }

    TEST_CASE("Get active DIDs excludes deactivated") {
        DIDRegistry registry;

        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto [doc1, _] = registry.create(key1_result.value()).value();
        registry.create(key2_result.value());

        // Deactivate first DID
        registry.deactivate(doc1.getId(), key1_result.value());

        auto active_dids = registry.getActiveDIDs();
        CHECK(active_dids.size() == 1);
    }

    TEST_CASE("Get controlled DIDs") {
        DIDRegistry registry;

        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto [doc1, _] = registry.create(key1_result.value()).value();
        auto did1 = doc1.getId();

        auto [doc2, __] = registry.create(key2_result.value()).value();
        auto did2 = doc2.getId();

        // Add did1 as controller of did2
        doc2.addController(did1);
        registry.update(did2, doc2, key2_result.value());

        // did1 should control both itself and did2
        auto controlled = registry.getControlled(did1);
        CHECK(controlled.size() == 2);
    }

    TEST_CASE("Get DID history") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto create_result = registry.create(key_result.value());
        REQUIRE(create_result.is_ok());

        auto [doc, _] = create_result.value();
        auto did = doc.getId();

        // Make some updates
        doc.addService(Service(did.withFragment("svc1"), ServiceType::SwarmCoordinator, "endpoint1"));
        registry.update(did, doc, key_result.value());

        doc.addService(Service(did.withFragment("svc2"), ServiceType::TaskQueue, "endpoint2"));
        registry.update(did, doc, key_result.value());

        // Check history
        auto history = registry.getHistory(did);
        CHECK(history.size() == 3); // create + 2 updates
    }

    TEST_CASE("Registry size") {
        DIDRegistry registry;
        CHECK(registry.size() == 0);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        registry.create(key_result.value());
        CHECK(registry.size() == 1);
    }

    TEST_CASE("Clear registry") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        registry.create(key_result.value());
        CHECK(registry.size() == 1);

        registry.clear();
        CHECK(registry.size() == 0);
    }

    TEST_CASE("Verify signature") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto [doc, _] = registry.create(key_result.value()).value();
        auto did = doc.getId();

        // Sign some data
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        auto sig_result = key_result.value().sign(data);
        REQUIRE(sig_result.is_ok());

        // Verify using registry
        auto verify_result = registry.verifySignature(did, did.withFragment("key-1"), data, sig_result.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Verify signature with wrong data fails") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto [doc, _] = registry.create(key_result.value()).value();
        auto did = doc.getId();

        // Sign some data
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        auto sig_result = key_result.value().sign(data);
        REQUIRE(sig_result.is_ok());

        // Verify with different data
        std::vector<uint8_t> wrong_data = {0x05, 0x06, 0x07, 0x08};
        auto verify_result = registry.verifySignature(did, did.withFragment("key-1"), wrong_data, sig_result.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == false);
    }

    TEST_CASE("Verify authentication") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto [doc, _] = registry.create(key_result.value()).value();
        auto did = doc.getId();

        // Create a challenge-response
        std::vector<uint8_t> challenge = {0xDE, 0xAD, 0xBE, 0xEF};
        auto sig_result = key_result.value().sign(challenge);
        REQUIRE(sig_result.is_ok());

        // Verify authentication
        auto auth_result =
            registry.verifyAuthentication(did, challenge, sig_result.value(), did.withFragment("key-1"));
        REQUIRE(auth_result.is_ok());
        CHECK(auth_result.value() == true);
    }

    TEST_CASE("Apply operation from blockchain") {
        DIDRegistry registry;

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        // Simulate creating an operation as if it came from blockchain
        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();
        auto serialized = doc.serialize();
        std::vector<uint8_t> doc_bytes(serialized.begin(), serialized.end());

        DIDOperation op(DIDOperationType::Create, did.toString(), doc_bytes, did.toString());

        // Apply operation
        auto apply_result = registry.applyOperation(op);
        REQUIRE(apply_result.is_ok());

        // Verify DID exists
        CHECK(registry.exists(did));
        auto resolved = registry.resolve(did).value();
        CHECK(resolved.getIdString() == did.toString());
    }

    TEST_CASE("DIDOperation serialization round-trip") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto serialized = doc.serialize();
        std::vector<uint8_t> doc_bytes(serialized.begin(), serialized.end());

        DIDOperation op(DIDOperationType::Create, doc.getIdString(), doc_bytes, doc.getIdString());

        // Serialize
        auto op_bytes = op.toBytes();
        CHECK_FALSE(op_bytes.empty());

        // Deserialize
        auto restored_result = DIDOperation::fromBytes(op_bytes);
        REQUIRE(restored_result.is_ok());

        auto restored = restored_result.value();
        CHECK(restored.getOperationType() == op.getOperationType());
        CHECK(restored.getDID() == op.getDID());
        CHECK(restored.getSignerDID() == op.getSignerDID());
    }
}
