#include "blockit/identity/credential_status.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("Credential Status Tests") {

    TEST_CASE("Record credential issuance") {
        CredentialStatusList status_list;

        CHECK(status_list.size() == 0);

        status_list.recordIssue("cred-001", "did:blockit:issuer1");

        CHECK(status_list.size() == 1);
        CHECK(status_list.exists("cred-001"));
        CHECK(status_list.isActive("cred-001"));
        CHECK_FALSE(status_list.isRevoked("cred-001"));
        CHECK_FALSE(status_list.isSuspended("cred-001"));
    }

    TEST_CASE("Get credential status") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");

        auto status_result = status_list.getStatus("cred-001");
        REQUIRE(status_result.is_ok());
        CHECK(status_result.value() == CredentialStatus::Active);
    }

    TEST_CASE("Get status of non-existent credential") {
        CredentialStatusList status_list;

        CHECK_FALSE(status_list.exists("nonexistent"));

        auto status_result = status_list.getStatus("nonexistent");
        CHECK(status_result.is_err());
    }

    TEST_CASE("Revoke credential") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordRevoke("cred-001", "did:blockit:issuer1", "Compromised key");
        REQUIRE(result.is_ok());

        CHECK(status_list.isRevoked("cred-001"));
        CHECK_FALSE(status_list.isActive("cred-001"));

        auto entry_result = status_list.getStatusEntry("cred-001");
        REQUIRE(entry_result.is_ok());
        CHECK(entry_result.value().reason == "Compromised key");
    }

    TEST_CASE("Cannot revoke non-existent credential") {
        CredentialStatusList status_list;

        auto result = status_list.recordRevoke("nonexistent", "did:blockit:issuer1");
        CHECK(result.is_err());
    }

    TEST_CASE("Cannot revoke already revoked credential") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordRevoke("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordRevoke("cred-001", "did:blockit:issuer1");
        CHECK(result.is_err());
    }

    TEST_CASE("Only issuer can revoke") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordRevoke("cred-001", "did:blockit:other-issuer");
        CHECK(result.is_err());

        // Still active
        CHECK(status_list.isActive("cred-001"));
    }

    TEST_CASE("Suspend credential") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordSuspend("cred-001", "did:blockit:issuer1", "Pending review");
        REQUIRE(result.is_ok());

        CHECK(status_list.isSuspended("cred-001"));
        CHECK_FALSE(status_list.isActive("cred-001"));
        CHECK_FALSE(status_list.isRevoked("cred-001"));
    }

    TEST_CASE("Cannot suspend non-existent credential") {
        CredentialStatusList status_list;

        auto result = status_list.recordSuspend("nonexistent", "did:blockit:issuer1");
        CHECK(result.is_err());
    }

    TEST_CASE("Cannot suspend already suspended credential") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordSuspend("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordSuspend("cred-001", "did:blockit:issuer1");
        CHECK(result.is_err());
    }

    TEST_CASE("Cannot suspend revoked credential") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordRevoke("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordSuspend("cred-001", "did:blockit:issuer1");
        CHECK(result.is_err());
    }

    TEST_CASE("Only issuer can suspend") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordSuspend("cred-001", "did:blockit:other-issuer");
        CHECK(result.is_err());

        CHECK(status_list.isActive("cred-001"));
    }

    TEST_CASE("Unsuspend credential") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordSuspend("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordUnsuspend("cred-001", "did:blockit:issuer1");
        REQUIRE(result.is_ok());

        CHECK(status_list.isActive("cred-001"));
        CHECK_FALSE(status_list.isSuspended("cred-001"));
    }

    TEST_CASE("Cannot unsuspend non-suspended credential") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordUnsuspend("cred-001", "did:blockit:issuer1");
        CHECK(result.is_err());
    }

    TEST_CASE("Only issuer can unsuspend") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordSuspend("cred-001", "did:blockit:issuer1");

        auto result = status_list.recordUnsuspend("cred-001", "did:blockit:other-issuer");
        CHECK(result.is_err());

        CHECK(status_list.isSuspended("cred-001"));
    }

    TEST_CASE("Get credentials by issuer") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordIssue("cred-002", "did:blockit:issuer1");
        status_list.recordIssue("cred-003", "did:blockit:issuer2");

        auto issuer1_creds = status_list.getCredentialsByIssuer("did:blockit:issuer1");
        CHECK(issuer1_creds.size() == 2);

        auto issuer2_creds = status_list.getCredentialsByIssuer("did:blockit:issuer2");
        CHECK(issuer2_creds.size() == 1);
        CHECK(issuer2_creds[0] == "cred-003");
    }

    TEST_CASE("Get revoked credentials") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordIssue("cred-002", "did:blockit:issuer1");
        status_list.recordIssue("cred-003", "did:blockit:issuer1");

        status_list.recordRevoke("cred-001", "did:blockit:issuer1");
        status_list.recordRevoke("cred-003", "did:blockit:issuer1");

        auto revoked = status_list.getRevokedCredentials();
        CHECK(revoked.size() == 2);
    }

    TEST_CASE("Get suspended credentials") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordIssue("cred-002", "did:blockit:issuer1");

        status_list.recordSuspend("cred-002", "did:blockit:issuer1");

        auto suspended = status_list.getSuspendedCredentials();
        CHECK(suspended.size() == 1);
        CHECK(suspended[0] == "cred-002");
    }

    TEST_CASE("Clear status list") {
        CredentialStatusList status_list;
        status_list.recordIssue("cred-001", "did:blockit:issuer1");
        status_list.recordIssue("cred-002", "did:blockit:issuer1");

        CHECK(status_list.size() == 2);

        status_list.clear();

        CHECK(status_list.size() == 0);
        CHECK_FALSE(status_list.exists("cred-001"));
    }

    TEST_CASE("Credential operation serialization round-trip") {
        auto op = CredentialOperation::createRevoke("cred-001", "did:blockit:issuer1", "Test reason");

        auto bytes = op.toBytes();
        CHECK_FALSE(bytes.empty());

        auto restored_result = CredentialOperation::fromBytes(bytes);
        REQUIRE(restored_result.is_ok());

        auto restored = restored_result.value();
        CHECK(restored.getOperationType() == CredentialOperationType::Revoke);
        CHECK(restored.getCredentialId() == "cred-001");
        CHECK(restored.getIssuerDID() == "did:blockit:issuer1");
        CHECK(restored.getReason() == "Test reason");
    }

    TEST_CASE("Apply operations") {
        CredentialStatusList status_list;

        // Apply Issue operation
        auto issue_op = CredentialOperation(CredentialOperationType::Issue, "cred-001", "did:blockit:issuer1");
        auto issue_result = status_list.applyOperation(issue_op);
        REQUIRE(issue_result.is_ok());
        CHECK(status_list.isActive("cred-001"));

        // Apply Suspend operation
        auto suspend_op = CredentialOperation::createSuspend("cred-001", "did:blockit:issuer1", "Pending review");
        auto suspend_result = status_list.applyOperation(suspend_op);
        REQUIRE(suspend_result.is_ok());
        CHECK(status_list.isSuspended("cred-001"));

        // Apply Unsuspend operation
        auto unsuspend_op = CredentialOperation::createUnsuspend("cred-001", "did:blockit:issuer1");
        auto unsuspend_result = status_list.applyOperation(unsuspend_op);
        REQUIRE(unsuspend_result.is_ok());
        CHECK(status_list.isActive("cred-001"));

        // Apply Revoke operation
        auto revoke_op = CredentialOperation::createRevoke("cred-001", "did:blockit:issuer1");
        auto revoke_result = status_list.applyOperation(revoke_op);
        REQUIRE(revoke_result.is_ok());
        CHECK(status_list.isRevoked("cred-001"));
    }

    TEST_CASE("Issue operation with credential data") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        auto cred =
            VerifiableCredential::create("urn:uuid:test-cred", CredentialType::RobotAuthorization, issuer_did, subject_did);
        cred.setClaim("robot_id", "ROBOT_007");
        cred.sign(issuer_key, issuer_did.withFragment("key-1"));

        auto op = CredentialOperation::createIssue(cred);

        CHECK(op.getOperationType() == CredentialOperationType::Issue);
        CHECK(op.getCredentialId() == "urn:uuid:test-cred");
        CHECK(op.getIssuerDID() == issuer_did.toString());

        // Extract credential from operation
        auto extracted_result = op.getCredential();
        REQUIRE(extracted_result.is_ok());

        auto extracted = extracted_result.value();
        CHECK(extracted.getId() == cred.getId());
        CHECK(extracted.getClaim("robot_id").value() == "ROBOT_007");
    }

    TEST_CASE("Credential operation type strings") {
        CHECK(credentialOperationTypeToString(CredentialOperationType::Issue) == "issue");
        CHECK(credentialOperationTypeToString(CredentialOperationType::Revoke) == "revoke");
        CHECK(credentialOperationTypeToString(CredentialOperationType::Suspend) == "suspend");
        CHECK(credentialOperationTypeToString(CredentialOperationType::Unsuspend) == "unsuspend");
    }
}
