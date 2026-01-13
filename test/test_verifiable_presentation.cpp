#include "blockit/identity/verifiable_presentation.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("Verifiable Presentation Tests") {

    TEST_CASE("Create a basic presentation") {
        auto holder_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);

        auto vp = VerifiablePresentation::create(holder_did);

        CHECK_FALSE(vp.getId().empty());
        CHECK(vp.getHolder() == holder_did);
        CHECK(vp.getCredentialCount() == 0);
        CHECK_FALSE(vp.hasCredentials());
        CHECK_FALSE(vp.hasProof());
    }

    TEST_CASE("Create presentation with custom ID") {
        auto holder_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);

        auto vp = VerifiablePresentation::create("urn:uuid:custom-id", holder_did);

        CHECK(vp.getId() == "urn:uuid:custom-id");
        CHECK(vp.getHolder() == holder_did);
    }

    TEST_CASE("Add credentials to presentation") {
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);
        auto issuer_did = DID::fromKey(issuer_key);

        // Create some credentials
        auto cred1 =
            VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did, holder_did);
        cred1.setClaim("robot_id", "ROBOT_001");
        cred1.sign(issuer_key, issuer_did.withFragment("key-1"));

        auto cred2 =
            VerifiableCredential::create("urn:uuid:cred2", CredentialType::ZoneAccess, issuer_did, holder_did);
        cred2.setClaim("zone", "warehouse_a");
        cred2.sign(issuer_key, issuer_did.withFragment("key-1"));

        // Create presentation
        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred1);
        vp.addCredential(cred2);

        CHECK(vp.getCredentialCount() == 2);
        CHECK(vp.hasCredentials());

        auto creds = vp.getCredentialsVector();
        CHECK(creds.size() == 2);
        CHECK(creds[0].getId() == "urn:uuid:cred1");
        CHECK(creds[1].getId() == "urn:uuid:cred2");
    }

    TEST_CASE("Get credential by index") {
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);
        auto issuer_did = DID::fromKey(issuer_key);

        auto cred =
            VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did, holder_did);

        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred);

        auto result = vp.getCredential(0);
        REQUIRE(result.is_ok());
        CHECK(result.value().getId() == "urn:uuid:cred1");

        // Out of range
        auto bad_result = vp.getCredential(10);
        CHECK(bad_result.is_err());
    }

    TEST_CASE("Find credential by ID") {
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);
        auto issuer_did = DID::fromKey(issuer_key);

        auto cred1 =
            VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did, holder_did);
        auto cred2 =
            VerifiableCredential::create("urn:uuid:cred2", CredentialType::ZoneAccess, issuer_did, holder_did);

        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred1);
        vp.addCredential(cred2);

        auto result = vp.findCredentialById("urn:uuid:cred2");
        REQUIRE(result.is_ok());
        CHECK(result.value().hasType(CredentialType::ZoneAccess));

        // Not found
        auto bad_result = vp.findCredentialById("urn:uuid:nonexistent");
        CHECK(bad_result.is_err());
    }

    TEST_CASE("Find credentials by type") {
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);
        auto issuer_did = DID::fromKey(issuer_key);

        auto cred1 =
            VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did, holder_did);
        auto cred2 =
            VerifiableCredential::create("urn:uuid:cred2", CredentialType::ZoneAccess, issuer_did, holder_did);
        auto cred3 =
            VerifiableCredential::create("urn:uuid:cred3", CredentialType::ZoneAccess, issuer_did, holder_did);

        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred1);
        vp.addCredential(cred2);
        vp.addCredential(cred3);

        auto zone_creds = vp.findCredentialsByType(CredentialType::ZoneAccess);
        CHECK(zone_creds.size() == 2);

        auto robot_creds = vp.findCredentialsByType(CredentialType::RobotAuthorization);
        CHECK(robot_creds.size() == 1);

        auto task_creds = vp.findCredentialsByType(CredentialType::TaskCertification);
        CHECK(task_creds.empty());
    }

    TEST_CASE("Set challenge and domain") {
        auto holder_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);

        auto vp = VerifiablePresentation::create(holder_did);

        CHECK_FALSE(vp.hasChallenge());
        CHECK_FALSE(vp.hasDomain());

        vp.setChallenge("random-challenge-12345");
        vp.setDomain("warehouse-a-gate");

        CHECK(vp.hasChallenge());
        CHECK(vp.hasDomain());
        CHECK(vp.getChallenge() == "random-challenge-12345");
        CHECK(vp.getDomain() == "warehouse-a-gate");
    }

    TEST_CASE("Sign and verify presentation") {
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);
        auto issuer_did = DID::fromKey(issuer_key);

        // Create credential
        auto cred =
            VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did, holder_did);
        cred.sign(issuer_key, issuer_did.withFragment("key-1"));

        // Create and sign presentation
        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred);
        vp.setChallenge("test-challenge");

        auto sign_result = vp.sign(holder_key, holder_did.withFragment("key-1"));
        REQUIRE(sign_result.is_ok());

        CHECK(vp.hasProof());
        CHECK(vp.getProof().getType() == "Ed25519Signature2020");
        CHECK(vp.getProof().getProofPurpose() == "authentication");

        // Verify with holder's key
        auto verify_result = vp.verifyWithKey(holder_key);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Verify with wrong key fails") {
        auto holder_key = Key::generate().value();
        auto wrong_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);

        auto vp = VerifiablePresentation::create(holder_did);
        vp.sign(holder_key, holder_did.withFragment("key-1"));

        auto verify_result = vp.verifyWithKey(wrong_key);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == false);
    }

    TEST_CASE("Verify unsigned presentation fails") {
        auto holder_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);

        auto vp = VerifiablePresentation::create(holder_did);

        auto verify_result = vp.verifyWithKey(holder_key);
        CHECK(verify_result.is_err());
    }

    TEST_CASE("Full verification with DID Registry") {
        DIDRegistry registry;

        // Create holder and issuer DIDs
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();

        auto holder_create_result = registry.create(holder_key);
        auto issuer_create_result = registry.create(issuer_key);
        REQUIRE(holder_create_result.is_ok());
        REQUIRE(issuer_create_result.is_ok());

        auto holder_did = holder_create_result.value().first.getId();
        auto issuer_did = issuer_create_result.value().first.getId();

        // Issue credential
        auto cred = VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did,
                                                 holder_did);
        cred.setClaim("robot_id", "ROBOT_007");
        cred.sign(issuer_key, issuer_did.withFragment("key-1"));

        // Create presentation
        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred);
        vp.setChallenge("gate-challenge-xyz");
        vp.setDomain("warehouse-a-gate");
        vp.sign(holder_key, holder_did.withFragment("key-1"));

        // Verify using registry (full verification)
        auto verify_result = vp.verify(registry);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Verification fails with invalid credential") {
        DIDRegistry registry;

        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();

        auto holder_create_result = registry.create(holder_key);
        auto issuer_create_result = registry.create(issuer_key);
        REQUIRE(holder_create_result.is_ok());
        REQUIRE(issuer_create_result.is_ok());

        auto holder_did = holder_create_result.value().first.getId();
        auto issuer_did = issuer_create_result.value().first.getId();

        // Issue credential and then revoke it
        auto cred = VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did,
                                                 holder_did);
        cred.sign(issuer_key, issuer_did.withFragment("key-1"));
        cred.revoke(); // Revoke the credential

        // Create presentation with revoked credential
        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred);
        vp.sign(holder_key, holder_did.withFragment("key-1"));

        // Verification should fail because credential is revoked
        auto verify_result = vp.verify(registry);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == false);
    }

    TEST_CASE("Verification fails with unknown holder") {
        DIDRegistry registry;

        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);

        // Only create issuer in registry, not holder
        auto issuer_create_result = registry.create(issuer_key);
        REQUIRE(issuer_create_result.is_ok());

        auto issuer_did = issuer_create_result.value().first.getId();

        // Issue credential
        auto cred = VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did,
                                                 holder_did);
        cred.sign(issuer_key, issuer_did.withFragment("key-1"));

        // Create presentation (holder not in registry)
        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred);
        vp.sign(holder_key, holder_did.withFragment("key-1"));

        // Verification should fail because holder is not in registry
        auto verify_result = vp.verify(registry);
        CHECK(verify_result.is_err());
    }

    TEST_CASE("Presentation serialization round-trip") {
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);
        auto issuer_did = DID::fromKey(issuer_key);

        auto cred =
            VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did, holder_did);
        cred.setClaim("robot_id", "ROBOT_007");
        cred.sign(issuer_key, issuer_did.withFragment("key-1"));

        auto vp = VerifiablePresentation::create("urn:uuid:vp-123", holder_did);
        vp.addCredential(cred);
        vp.setChallenge("test-challenge");
        vp.setDomain("test-domain");
        vp.sign(holder_key, holder_did.withFragment("key-1"));

        // Serialize
        auto serialized = vp.serialize();
        CHECK_FALSE(serialized.empty());

        // Deserialize
        auto restored_result = VerifiablePresentation::deserialize(serialized);
        REQUIRE(restored_result.is_ok());

        auto restored = restored_result.value();
        CHECK(restored.getId() == vp.getId());
        CHECK(restored.getHolderString() == vp.getHolderString());
        CHECK(restored.getCredentialCount() == vp.getCredentialCount());
        CHECK(restored.getChallenge() == vp.getChallenge());
        CHECK(restored.getDomain() == vp.getDomain());
        CHECK(restored.hasProof());

        // Verify restored presentation
        auto verify_result = restored.verifyWithKey(holder_key);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Presentation JSON-LD output") {
        auto holder_key = Key::generate().value();
        auto issuer_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);
        auto issuer_did = DID::fromKey(issuer_key);

        auto cred =
            VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer_did, holder_did);
        cred.setClaim("robot_id", "ROBOT_007");
        cred.sign(issuer_key, issuer_did.withFragment("key-1"));

        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred);
        vp.setChallenge("random-challenge");
        vp.setDomain("warehouse-gate");
        vp.sign(holder_key, holder_did.withFragment("key-1"));

        auto json = vp.toJsonLd();

        // Basic validation
        CHECK(json.find("@context") != std::string::npos);
        CHECK(json.find("https://www.w3.org/2018/credentials/v1") != std::string::npos);
        CHECK(json.find("VerifiablePresentation") != std::string::npos);
        CHECK(json.find("holder") != std::string::npos);
        CHECK(json.find("verifiableCredential") != std::string::npos);
        CHECK(json.find("challenge") != std::string::npos);
        CHECK(json.find("random-challenge") != std::string::npos);
        CHECK(json.find("domain") != std::string::npos);
        CHECK(json.find("warehouse-gate") != std::string::npos);
        CHECK(json.find("proof") != std::string::npos);
        CHECK(json.find("Ed25519Signature2020") != std::string::npos);
    }

    TEST_CASE("Empty presentation JSON-LD") {
        auto holder_key = Key::generate().value();
        auto holder_did = DID::fromKey(holder_key);

        auto vp = VerifiablePresentation::create(holder_did);

        auto json = vp.toJsonLd();

        CHECK(json.find("@context") != std::string::npos);
        CHECK(json.find("VerifiablePresentation") != std::string::npos);
        CHECK(json.find("holder") != std::string::npos);
        // Should not have verifiableCredential array since empty
        CHECK(json.find("verifiableCredential") == std::string::npos);
    }

    TEST_CASE("Multiple credentials verification") {
        DIDRegistry registry;

        // Create holder and two different issuers
        auto holder_key = Key::generate().value();
        auto issuer1_key = Key::generate().value();
        auto issuer2_key = Key::generate().value();

        auto holder_result = registry.create(holder_key);
        auto issuer1_result = registry.create(issuer1_key);
        auto issuer2_result = registry.create(issuer2_key);
        REQUIRE(holder_result.is_ok());
        REQUIRE(issuer1_result.is_ok());
        REQUIRE(issuer2_result.is_ok());

        auto holder_did = holder_result.value().first.getId();
        auto issuer1_did = issuer1_result.value().first.getId();
        auto issuer2_did = issuer2_result.value().first.getId();

        // Create credentials from different issuers
        auto cred1 = VerifiableCredential::create("urn:uuid:cred1", CredentialType::RobotAuthorization, issuer1_did,
                                                  holder_did);
        cred1.setClaim("robot_id", "ROBOT_007");
        cred1.sign(issuer1_key, issuer1_did.withFragment("key-1"));

        auto cred2 =
            VerifiableCredential::create("urn:uuid:cred2", CredentialType::ZoneAccess, issuer2_did, holder_did);
        cred2.setClaim("zone", "warehouse_a");
        cred2.sign(issuer2_key, issuer2_did.withFragment("key-1"));

        // Create presentation with both credentials
        auto vp = VerifiablePresentation::create(holder_did);
        vp.addCredential(cred1);
        vp.addCredential(cred2);
        vp.sign(holder_key, holder_did.withFragment("key-1"));

        // Should verify both credentials
        auto verify_result = vp.verify(registry);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }
}
