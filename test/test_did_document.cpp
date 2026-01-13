#include "blockit/identity/did_document.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("DID Document Tests") {

    TEST_CASE("Create document with initial key") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());

        CHECK(doc.isActive());
        CHECK(doc.getVerificationMethods().size() == 1);
        CHECK(doc.getAuthentication().size() == 1);
        CHECK(doc.getAssertionMethod().size() == 1);
        CHECK(doc.getVersion() == 1);
        CHECK(doc.getCreated() > 0);
        CHECK(doc.getUpdated() > 0);
    }

    TEST_CASE("Document has correct DID") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto did = DID::fromKey(key_result.value());
        auto doc = DIDDocument::create(key_result.value());

        CHECK(doc.getId() == did);
        CHECK(doc.getIdString() == did.toString());
    }

    TEST_CASE("Document has self as controller by default") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        auto controllers = doc.getControllers();
        REQUIRE(controllers.size() == 1);
        CHECK(controllers[0] == did);
        CHECK(doc.isController(did));
    }

    TEST_CASE("Add and remove verification methods") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto doc = DIDDocument::create(key1_result.value());
        auto did = doc.getId();

        CHECK(doc.getVerificationMethods().size() == 1);

        // Add second key
        auto vm2 = VerificationMethod::fromKey(did, "key-2", key2_result.value());
        doc.addVerificationMethod(vm2);

        CHECK(doc.getVerificationMethods().size() == 2);
        CHECK(doc.hasVerificationMethod(did.withFragment("key-2")));

        // Remove second key
        auto remove_result = doc.removeVerificationMethod(did.withFragment("key-2"));
        CHECK(remove_result.is_ok());
        CHECK(doc.getVerificationMethods().size() == 1);
        CHECK_FALSE(doc.hasVerificationMethod(did.withFragment("key-2")));
    }

    TEST_CASE("Remove non-existent verification method fails") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto result = doc.removeVerificationMethod("did:blockit:xxx#nonexistent");
        CHECK(result.is_err());
    }

    TEST_CASE("Get verification method by ID") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        auto vm_result = doc.getVerificationMethod(did.withFragment("key-1"));
        REQUIRE(vm_result.is_ok());

        auto vm = vm_result.value();
        CHECK(vm.getId() == did.withFragment("key-1"));
        CHECK(vm.getController() == did.toString());
    }

    TEST_CASE("Add verification relationships") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        // Add to key agreement
        doc.addKeyAgreement(did.withFragment("key-1"));
        CHECK(doc.getKeyAgreement().size() == 1);

        // Add to capability invocation
        doc.addCapabilityInvocation(did.withFragment("key-1"));
        CHECK(doc.getCapabilityInvocation().size() == 1);

        // Add to capability delegation
        doc.addCapabilityDelegation(did.withFragment("key-1"));
        CHECK(doc.getCapabilityDelegation().size() == 1);
    }

    TEST_CASE("Check authentication and assertion capabilities") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        CHECK(doc.canAuthenticate(did.withFragment("key-1")));
        CHECK(doc.canAssert(did.withFragment("key-1")));
        CHECK_FALSE(doc.canAuthenticate(did.withFragment("nonexistent")));
    }

    TEST_CASE("Add and remove services") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();

        CHECK(doc.getServices().size() == 0);

        // Add service
        Service svc(did.withFragment("swarm"), ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");
        doc.addService(svc);

        CHECK(doc.getServices().size() == 1);
        CHECK(doc.hasService(did.withFragment("swarm")));

        // Get service
        auto svc_result = doc.getService(did.withFragment("swarm"));
        REQUIRE(svc_result.is_ok());
        CHECK(svc_result.value().getServiceEndpoint() == "udp://192.168.1.100:9000");

        // Remove service
        auto remove_result = doc.removeService(did.withFragment("swarm"));
        CHECK(remove_result.is_ok());
        CHECK(doc.getServices().size() == 0);
    }

    TEST_CASE("Document deactivation") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        CHECK(doc.isActive());
        CHECK(doc.getStatus() == DIDDocumentStatus::Active);

        doc.deactivate();
        CHECK_FALSE(doc.isActive());
        CHECK(doc.getStatus() == DIDDocumentStatus::Deactivated);
    }

    TEST_CASE("Document version increments on changes") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        auto did = doc.getId();
        dp::u32 initial_version = doc.getVersion();

        // Add a service
        Service svc(did.withFragment("test"), ServiceType::SwarmCoordinator, "test://endpoint");
        doc.addService(svc);

        CHECK(doc.getVersion() == initial_version + 1);
    }

    TEST_CASE("Also known as identifiers") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto doc = DIDDocument::create(key_result.value());
        CHECK(doc.getAlsoKnownAs().empty());

        doc.addAlsoKnownAs("https://example.com/robot-007");
        doc.addAlsoKnownAs("urn:uuid:12345");

        auto akas = doc.getAlsoKnownAs();
        REQUIRE(akas.size() == 2);
        CHECK(akas[0] == "https://example.com/robot-007");
        CHECK(akas[1] == "urn:uuid:12345");
    }

    TEST_CASE("Multiple controllers") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto doc = DIDDocument::create(key1_result.value());
        auto did1 = doc.getId();
        auto did2 = DID::fromKey(key2_result.value());

        // Add second controller
        doc.addController(did2);

        auto controllers = doc.getControllers();
        CHECK(controllers.size() == 2);
        CHECK(doc.isController(did1));
        CHECK(doc.isController(did2));
    }

    TEST_CASE("Set controller replaces existing") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto doc = DIDDocument::create(key1_result.value());
        auto did1 = doc.getId();
        auto did2 = DID::fromKey(key2_result.value());

        // Replace controller
        doc.setController(did2);

        auto controllers = doc.getControllers();
        CHECK(controllers.size() == 1);
        CHECK_FALSE(doc.isController(did1));
        CHECK(doc.isController(did2));
    }

    TEST_CASE("Remove verification method also removes from relationships") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        auto doc = DIDDocument::create(key1_result.value());
        auto did = doc.getId();

        // Add second key with all relationships
        auto vm2 = VerificationMethod::fromKey(did, "key-2", key2_result.value());
        doc.addVerificationMethod(vm2);
        doc.addAuthentication(did.withFragment("key-2"));
        doc.addAssertionMethod(did.withFragment("key-2"));
        doc.addKeyAgreement(did.withFragment("key-2"));

        CHECK(doc.getAuthentication().size() == 2);
        CHECK(doc.getAssertionMethod().size() == 2);
        CHECK(doc.getKeyAgreement().size() == 1);

        // Remove key-2
        doc.removeVerificationMethod(did.withFragment("key-2"));

        // Relationships should also be removed
        CHECK(doc.getAuthentication().size() == 1);
        CHECK(doc.getAssertionMethod().size() == 1);
        CHECK(doc.getKeyAgreement().size() == 0);
    }
}
