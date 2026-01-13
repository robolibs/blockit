#include "blockit/identity/verifiable_credential.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("Verifiable Credential Tests") {

    TEST_CASE("Create a basic credential") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto vc = VerifiableCredential::create("urn:uuid:12345", CredentialType::RobotAuthorization, issuer_did,
                                               subject_did);

        CHECK(vc.getId() == "urn:uuid:12345");
        CHECK(vc.getIssuer() == issuer_did);
        CHECK(vc.getSubjectDID() == subject_did);
        CHECK(vc.isValid());
        CHECK_FALSE(vc.hasProof());
    }

    TEST_CASE("Credential has correct types") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto vc = VerifiableCredential::create("id", CredentialType::ZoneAccess, issuer_did, subject_did);

        // Should have both VerifiableCredential and ZoneAccess types
        auto types = vc.getTypes();
        CHECK(types.size() == 2);
        CHECK(vc.hasType(CredentialType::VerifiableCredential));
        CHECK(vc.hasType(CredentialType::ZoneAccess));
    }

    TEST_CASE("Add claims to credential") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto vc = VerifiableCredential::create("id", CredentialType::RobotAuthorization, issuer_did, subject_did);

        vc.setClaim("robot_id", "ROBOT_007");
        vc.setClaim("capability", "patrol");
        vc.setClaim("zone", "warehouse_a");

        CHECK(vc.hasClaim("robot_id"));
        CHECK(vc.getClaim("robot_id").value() == "ROBOT_007");
        CHECK(vc.getClaim("capability").value() == "patrol");
        CHECK(vc.getClaim("zone").value() == "warehouse_a");
    }

    TEST_CASE("Get non-existent claim fails") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto vc = VerifiableCredential::create("id", CredentialType::RobotAuthorization, DID::fromKey(issuer_key),
                                               DID::fromKey(subject_key));

        auto result = vc.getClaim("nonexistent");
        CHECK(result.is_err());
    }

    TEST_CASE("Sign and verify credential") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto vc = VerifiableCredential::create("urn:uuid:12345", CredentialType::RobotAuthorization, issuer_did,
                                               subject_did);

        vc.setClaim("robot_id", "ROBOT_007");

        // Sign
        auto sign_result = vc.sign(issuer_key, issuer_did.withFragment("key-1"));
        REQUIRE(sign_result.is_ok());

        CHECK(vc.hasProof());
        CHECK(vc.getProof().getType() == "Ed25519Signature2020");
        CHECK(vc.getProof().getProofPurpose() == "assertionMethod");

        // Verify
        auto verify_result = vc.verify(issuer_key);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Verify with wrong key fails") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto wrong_key = Key::generate().value();

        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto vc = VerifiableCredential::create("id", CredentialType::RobotAuthorization, issuer_did, subject_did);

        vc.sign(issuer_key, issuer_did.withFragment("key-1"));

        // Verify with wrong key
        auto verify_result = vc.verify(wrong_key);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == false);
    }

    TEST_CASE("Verify unsigned credential fails") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto vc = VerifiableCredential::create("id", CredentialType::RobotAuthorization, DID::fromKey(issuer_key),
                                               DID::fromKey(subject_key));

        auto verify_result = vc.verify(issuer_key);
        CHECK(verify_result.is_err());
    }

    TEST_CASE("Credential expiration") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto vc = VerifiableCredential::create("id", CredentialType::ZoneAccess, DID::fromKey(issuer_key),
                                               DID::fromKey(subject_key));

        // No expiration by default
        CHECK(vc.getExpirationDate() == 0);
        CHECK_FALSE(vc.isExpired());

        // Set expiration in the past
        auto past = std::chrono::system_clock::now() - std::chrono::hours(1);
        auto past_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(past.time_since_epoch()).count();
        vc.setExpirationDate(past_ms);

        CHECK(vc.isExpired());
        CHECK_FALSE(vc.isValid()); // Expired = not valid

        // Set expiration in the future
        auto future = std::chrono::system_clock::now() + std::chrono::hours(24);
        auto future_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(future.time_since_epoch()).count();
        vc.setExpirationDate(future_ms);

        CHECK_FALSE(vc.isExpired());
    }

    TEST_CASE("Set expiration using duration") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto vc = VerifiableCredential::create("id", CredentialType::ZoneAccess, DID::fromKey(issuer_key),
                                               DID::fromKey(subject_key));

        // Set to expire in 1 hour
        vc.setExpiresIn(std::chrono::hours(1));

        CHECK(vc.getExpirationDate() > 0);
        CHECK_FALSE(vc.isExpired());
    }

    TEST_CASE("Credential status changes") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto vc = VerifiableCredential::create("id", CredentialType::RobotAuthorization, DID::fromKey(issuer_key),
                                               DID::fromKey(subject_key));

        CHECK(vc.getStatus() == CredentialStatus::Active);
        CHECK(vc.isValid());

        // Revoke
        vc.revoke();
        CHECK(vc.getStatus() == CredentialStatus::Revoked);
        CHECK_FALSE(vc.isValid());

        // Reset and suspend
        vc.setStatus(CredentialStatus::Active);
        vc.suspend();
        CHECK(vc.getStatus() == CredentialStatus::Suspended);
        CHECK_FALSE(vc.isValid());
    }

    TEST_CASE("Credential type strings") {
        CHECK(credentialTypeToString(CredentialType::VerifiableCredential) == "VerifiableCredential");
        CHECK(credentialTypeToString(CredentialType::RobotAuthorization) == "RobotAuthorization");
        CHECK(credentialTypeToString(CredentialType::CapabilityGrant) == "CapabilityGrant");
        CHECK(credentialTypeToString(CredentialType::ZoneAccess) == "ZoneAccess");
        CHECK(credentialTypeToString(CredentialType::TaskCertification) == "TaskCertification");
        CHECK(credentialTypeToString(CredentialType::SensorCalibration) == "SensorCalibration");
        CHECK(credentialTypeToString(CredentialType::SwarmMembership) == "SwarmMembership");
    }

    TEST_CASE("Credential status strings") {
        CHECK(credentialStatusToString(CredentialStatus::Active) == "active");
        CHECK(credentialStatusToString(CredentialStatus::Revoked) == "revoked");
        CHECK(credentialStatusToString(CredentialStatus::Suspended) == "suspended");
        CHECK(credentialStatusToString(CredentialStatus::Expired) == "expired");
    }

    TEST_CASE("Credential serialization round-trip") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto vc = VerifiableCredential::create("urn:uuid:12345", CredentialType::RobotAuthorization, issuer_did,
                                               subject_did);

        vc.setClaim("robot_id", "ROBOT_007");
        vc.setClaim("capability", "patrol");
        vc.setExpiresIn(std::chrono::hours(24));
        vc.sign(issuer_key, issuer_did.withFragment("key-1"));

        // Serialize
        auto serialized = vc.serialize();
        CHECK_FALSE(serialized.empty());

        // Deserialize
        auto restored_result = VerifiableCredential::deserialize(serialized);
        REQUIRE(restored_result.is_ok());

        auto restored = restored_result.value();
        CHECK(restored.getId() == vc.getId());
        CHECK(restored.getIssuerString() == vc.getIssuerString());
        CHECK(restored.getClaim("robot_id").value() == "ROBOT_007");
        CHECK(restored.hasProof());

        // Verify restored credential
        auto verify_result = restored.verify(issuer_key);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Credential JSON-LD output") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto vc = VerifiableCredential::create("urn:uuid:12345", CredentialType::RobotAuthorization, issuer_did,
                                               subject_did);

        vc.setClaim("robot_id", "ROBOT_007");
        vc.sign(issuer_key, issuer_did.withFragment("key-1"));

        auto json = vc.toJsonLd();

        // Basic validation
        CHECK(json.find("@context") != std::string::npos);
        CHECK(json.find("https://www.w3.org/2018/credentials/v1") != std::string::npos);
        CHECK(json.find("VerifiableCredential") != std::string::npos);
        CHECK(json.find("RobotAuthorization") != std::string::npos);
        CHECK(json.find("credentialSubject") != std::string::npos);
        CHECK(json.find("robot_id") != std::string::npos);
        CHECK(json.find("ROBOT_007") != std::string::npos);
        CHECK(json.find("proof") != std::string::npos);
        CHECK(json.find("Ed25519Signature2020") != std::string::npos);
    }

    TEST_CASE("Add multiple types to credential") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto vc = VerifiableCredential::create("id", CredentialType::RobotAuthorization, DID::fromKey(issuer_key),
                                               DID::fromKey(subject_key));

        vc.addType(CredentialType::SwarmMembership);
        vc.addType(CredentialType::CapabilityGrant);

        auto types = vc.getTypes();
        CHECK(types.size() == 4); // VC + RobotAuth + SwarmMembership + CapabilityGrant
        CHECK(vc.hasType(CredentialType::SwarmMembership));
        CHECK(vc.hasType(CredentialType::CapabilityGrant));
    }

    TEST_CASE("Adding duplicate type is ignored") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto vc = VerifiableCredential::create("id", CredentialType::RobotAuthorization, DID::fromKey(issuer_key),
                                               DID::fromKey(subject_key));

        auto initial_count = vc.getTypes().size();
        vc.addType(CredentialType::RobotAuthorization); // Already exists

        CHECK(vc.getTypes().size() == initial_count);
    }

    TEST_CASE("CredentialSubject claims") {
        CredentialSubject subject("did:blockit:abc123", "Robot");

        subject.setClaim("name", "Scout-001");
        subject.setClaim("zone", "warehouse");

        CHECK(subject.getId() == "did:blockit:abc123");
        CHECK(subject.getType() == "Robot");
        CHECK(subject.hasClaim("name"));
        CHECK(subject.getClaim("name").value() == "Scout-001");

        auto all_claims = subject.getAllClaims();
        CHECK(all_claims.size() == 2);
        CHECK(all_claims["name"] == "Scout-001");
        CHECK(all_claims["zone"] == "warehouse");
    }
}
