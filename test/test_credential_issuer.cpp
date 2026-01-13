#include "blockit/identity/credential_issuer.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("Credential Issuer Tests") {

    TEST_CASE("Create credential issuer") {
        auto issuer_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        CHECK(issuer.getIssuerDID() == issuer_did);
        CHECK(issuer.getKeyId() == issuer_did.withFragment("key-1"));
    }

    TEST_CASE("Create issuer with custom key ID") {
        auto issuer_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);

        CredentialIssuer issuer(issuer_did, issuer_key, issuer_did.withFragment("custom-key"));

        CHECK(issuer.getKeyId() == issuer_did.withFragment("custom-key"));
    }

    TEST_CASE("Issue robot authorization credential") {
        auto issuer_key = Key::generate().value();
        auto robot_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto robot_did = DID::fromKey(robot_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto result = issuer.issueRobotAuthorization(robot_did, "ROBOT_007", {"patrol", "sensor_read", "report"});
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.hasType(CredentialType::RobotAuthorization));
        CHECK(cred.getIssuer() == issuer_did);
        CHECK(cred.getSubjectDID() == robot_did);
        CHECK(cred.getClaim("robot_id").value() == "ROBOT_007");
        CHECK(cred.getClaim("capabilities").value() == "patrol,sensor_read,report");
        CHECK(cred.hasProof());
        CHECK(cred.getExpirationDate() > 0);

        // Verify signature
        auto verify_result = cred.verify(issuer_key);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Issue capability grant credential") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto result = issuer.issueCapabilityGrant(subject_did, "write", "/data/sensors/*");
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.hasType(CredentialType::CapabilityGrant));
        CHECK(cred.getClaim("capability").value() == "write");
        CHECK(cred.getClaim("resource").value() == "/data/sensors/*");
        CHECK(cred.hasProof());
    }

    TEST_CASE("Issue zone access credential") {
        auto issuer_key = Key::generate().value();
        auto robot_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto robot_did = DID::fromKey(robot_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto result = issuer.issueZoneAccess(robot_did, "warehouse_a");
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.hasType(CredentialType::ZoneAccess));
        CHECK(cred.getClaim("zone_id").value() == "warehouse_a");
        CHECK(cred.hasProof());
    }

    TEST_CASE("Issue task certification credential") {
        auto issuer_key = Key::generate().value();
        auto robot_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto robot_did = DID::fromKey(robot_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto result = issuer.issueTaskCertification(robot_did, "TASK_001", "delivery", "completed");
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.hasType(CredentialType::TaskCertification));
        CHECK(cred.getClaim("task_id").value() == "TASK_001");
        CHECK(cred.getClaim("task_type").value() == "delivery");
        CHECK(cred.getClaim("result").value() == "completed");
        CHECK(cred.hasClaim("certified_at"));
        CHECK(cred.hasProof());
        // Task certifications don't expire by default
        CHECK(cred.getExpirationDate() == 0);
    }

    TEST_CASE("Issue sensor calibration credential") {
        auto issuer_key = Key::generate().value();
        auto robot_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto robot_did = DID::fromKey(robot_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        std::map<std::string, std::string> cal_data = {
            {"offset_x", "0.01"}, {"offset_y", "-0.02"}, {"gain", "1.05"}};

        auto result = issuer.issueSensorCalibration(robot_did, "lidar", "LIDAR_001", cal_data);
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.hasType(CredentialType::SensorCalibration));
        CHECK(cred.getClaim("sensor_type").value() == "lidar");
        CHECK(cred.getClaim("sensor_id").value() == "LIDAR_001");
        CHECK(cred.getClaim("cal_offset_x").value() == "0.01");
        CHECK(cred.getClaim("cal_offset_y").value() == "-0.02");
        CHECK(cred.getClaim("cal_gain").value() == "1.05");
        CHECK(cred.hasClaim("calibrated_at"));
        CHECK(cred.hasProof());
    }

    TEST_CASE("Issue swarm membership credential") {
        auto issuer_key = Key::generate().value();
        auto robot_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto robot_did = DID::fromKey(robot_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto result = issuer.issueSwarmMembership(robot_did, "swarm-alpha", "leader");
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.hasType(CredentialType::SwarmMembership));
        CHECK(cred.getClaim("swarm_id").value() == "swarm-alpha");
        CHECK(cred.getClaim("role").value() == "leader");
        CHECK(cred.hasClaim("joined_at"));
        CHECK(cred.hasProof());
    }

    TEST_CASE("Issue custom credential") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        std::map<std::string, std::string> claims = {{"custom_field", "custom_value"}, {"another_field", "another_value"}};

        auto result = issuer.issueCustomCredential(subject_did, CredentialType::Custom, claims);
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.hasType(CredentialType::Custom));
        CHECK(cred.getClaim("custom_field").value() == "custom_value");
        CHECK(cred.getClaim("another_field").value() == "another_value");
        CHECK(cred.hasProof());
    }

    TEST_CASE("Issue credential with custom expiration") {
        auto issuer_key = Key::generate().value();
        auto robot_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto robot_did = DID::fromKey(robot_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        // Issue with 1 hour expiration
        auto result = issuer.issueZoneAccess(robot_did, "restricted_zone", std::chrono::hours(1));
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(cred.getExpirationDate() > 0);
        CHECK_FALSE(cred.isExpired());

        // Should expire roughly 1 hour from now
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        auto exp_time = cred.getExpirationDate();
        auto diff_ms = exp_time - now;
        // Should be roughly 1 hour (3600000ms) - allow some tolerance
        CHECK(diff_ms > 3500000);
        CHECK(diff_ms < 3700000);
    }

    TEST_CASE("Issue and record in status list") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        CredentialIssuer issuer(issuer_did, issuer_key);
        CredentialStatusList status_list;

        auto result =
            issuer.issueAndRecord(status_list, subject_did, CredentialType::RobotAuthorization, {{"robot_id", "R001"}});
        REQUIRE(result.is_ok());

        auto cred = result.value();
        CHECK(status_list.exists(cred.getId()));
        CHECK(status_list.isActive(cred.getId()));
    }

    TEST_CASE("Revoke credential via issuer") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        CredentialIssuer issuer(issuer_did, issuer_key);
        CredentialStatusList status_list;

        auto cred_result =
            issuer.issueAndRecord(status_list, subject_did, CredentialType::RobotAuthorization, {{"robot_id", "R001"}});
        REQUIRE(cred_result.is_ok());
        auto cred_id = cred_result.value().getId();

        auto revoke_result = issuer.revoke(status_list, cred_id, "Security concern");
        REQUIRE(revoke_result.is_ok());

        CHECK(status_list.isRevoked(cred_id));
    }

    TEST_CASE("Suspend and unsuspend credential via issuer") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        CredentialIssuer issuer(issuer_did, issuer_key);
        CredentialStatusList status_list;

        auto cred_result =
            issuer.issueAndRecord(status_list, subject_did, CredentialType::RobotAuthorization, {{"robot_id", "R001"}});
        REQUIRE(cred_result.is_ok());
        auto cred_id = cred_result.value().getId();

        // Suspend
        auto suspend_result = issuer.suspend(status_list, cred_id, "Pending investigation");
        REQUIRE(suspend_result.is_ok());
        CHECK(status_list.isSuspended(cred_id));

        // Unsuspend
        auto unsuspend_result = issuer.unsuspend(status_list, cred_id);
        REQUIRE(unsuspend_result.is_ok());
        CHECK(status_list.isActive(cred_id));
    }

    TEST_CASE("Create revoke operation") {
        auto issuer_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto op = issuer.createRevokeOperation("cred-001", "Compromised");

        CHECK(op.getOperationType() == CredentialOperationType::Revoke);
        CHECK(op.getCredentialId() == "cred-001");
        CHECK(op.getIssuerDID() == issuer_did.toString());
        CHECK(op.getReason() == "Compromised");
    }

    TEST_CASE("Create suspend operation") {
        auto issuer_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto op = issuer.createSuspendOperation("cred-001", "Pending review");

        CHECK(op.getOperationType() == CredentialOperationType::Suspend);
        CHECK(op.getCredentialId() == "cred-001");
        CHECK(op.getReason() == "Pending review");
    }

    TEST_CASE("Create unsuspend operation") {
        auto issuer_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto op = issuer.createUnsuspendOperation("cred-001");

        CHECK(op.getOperationType() == CredentialOperationType::Unsuspend);
        CHECK(op.getCredentialId() == "cred-001");
    }

    TEST_CASE("Multiple issuers with separate status tracking") {
        auto issuer1_key = Key::generate().value();
        auto issuer2_key = Key::generate().value();
        auto subject_key = Key::generate().value();

        auto issuer1_did = DID::fromKey(issuer1_key);
        auto issuer2_did = DID::fromKey(issuer2_key);
        auto subject_did = DID::fromKey(subject_key);

        CredentialIssuer issuer1(issuer1_did, issuer1_key);
        CredentialIssuer issuer2(issuer2_did, issuer2_key);
        CredentialStatusList status_list;

        // Both issuers issue credentials
        auto cred1 = issuer1.issueAndRecord(status_list, subject_did, CredentialType::RobotAuthorization,
                                            {{"robot_id", "R001"}});
        auto cred2 = issuer2.issueAndRecord(status_list, subject_did, CredentialType::ZoneAccess, {{"zone", "A"}});

        REQUIRE(cred1.is_ok());
        REQUIRE(cred2.is_ok());

        CHECK(status_list.size() == 2);

        // Issuer1 cannot revoke issuer2's credential
        auto bad_revoke = issuer1.revoke(status_list, cred2.value().getId());
        CHECK(bad_revoke.is_err());

        // But issuer2 can
        auto good_revoke = issuer2.revoke(status_list, cred2.value().getId());
        CHECK(good_revoke.is_ok());
    }

    TEST_CASE("Credential IDs are unique") {
        auto issuer_key = Key::generate().value();
        auto subject_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);
        auto subject_did = DID::fromKey(subject_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        std::vector<std::string> ids;
        for (int i = 0; i < 10; i++) {
            auto result = issuer.issueRobotAuthorization(subject_did, "ROBOT_" + std::to_string(i), {});
            REQUIRE(result.is_ok());
            ids.push_back(result.value().getId());
        }

        // All IDs should be unique
        std::set<std::string> unique_ids(ids.begin(), ids.end());
        CHECK(unique_ids.size() == ids.size());
    }

    TEST_CASE("Set custom key ID") {
        auto issuer_key = Key::generate().value();
        auto issuer_did = DID::fromKey(issuer_key);

        CredentialIssuer issuer(issuer_did, issuer_key);
        issuer.setKeyId(issuer_did.withFragment("key-2"));

        CHECK(issuer.getKeyId() == issuer_did.withFragment("key-2"));
    }
}
