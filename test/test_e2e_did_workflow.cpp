#include "blockit/identity/credential_issuer.hpp"
#include "blockit/identity/verifiable_presentation.hpp"
#include "blockit/storage/blockit_store.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
namespace fs = std::filesystem;

// Test data type with required to_string() method
struct E2ETestData {
    dp::String name{};
    dp::i32 value{0};

    E2ETestData() = default;
    E2ETestData(const std::string &n, int v) : name(dp::String(n.c_str())), value(v) {}

    std::string to_string() const { return std::string(name.c_str()) + ":" + std::to_string(value); }

    auto members() { return std::tie(name, value); }
    auto members() const { return std::tie(name, value); }
};

TEST_SUITE("End-to-End DID Workflow Tests") {

    TEST_CASE("Complete robot onboarding workflow") {
        auto temp_dir = fs::temp_directory_path() / "blockit_e2e_did_1";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<E2ETestData> blockit;
            auto genesis_key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("robot-fleet-chain"),
                               dp::String("genesis"), E2ETestData{"genesis", 0}, crypto);
            blockit.initializeDID();

            // === Step 1: Create fleet manager identity ===
            auto fleet_manager_key = Key::generate().value();
            auto fleet_manager_result = blockit.createDID(fleet_manager_key);
            REQUIRE(fleet_manager_result.is_ok());
            auto fleet_manager_did = fleet_manager_result.value().first.getId();

            // === Step 2: Onboard a new robot ===
            auto robot_key = Key::generate().value();
            auto robot_result = blockit.createRobotIdentity(robot_key, "DELIVERY_BOT_001",
                                                            {"navigation", "pickup", "delivery", "charging"},
                                                            fleet_manager_key);
            REQUIRE(robot_result.is_ok());

            auto [robot_doc, robot_auth_cred] = robot_result.value();
            auto robot_did = robot_doc.getId();

            // === Step 3: Issue zone access credential ===
            auto zone_cred_result = blockit.issueCredential(fleet_manager_key, robot_did, CredentialType::ZoneAccess,
                                                            {{"zone_id", "warehouse_a"}, {"access_level", "full"}},
                                                            std::chrono::hours(8));
            REQUIRE(zone_cred_result.is_ok());
            auto zone_cred = zone_cred_result.value();

            // === Step 4: Robot creates a presentation ===
            auto presentation = VerifiablePresentation::create(robot_did);
            presentation.addCredential(robot_auth_cred);
            presentation.addCredential(zone_cred);

            // Set challenge for authentication
            std::string challenge = "unique-challenge-" + std::to_string(std::time(nullptr));
            presentation.setChallenge(challenge);

            // Sign the presentation
            auto sign_result = presentation.sign(robot_key, robot_did.withFragment("key-1"));
            REQUIRE(sign_result.is_ok());

            // === Step 5: Verify the presentation ===
            CHECK(presentation.hasChallenge());
            CHECK(presentation.getChallenge() == challenge);

            auto verify_result = presentation.verifyWithKey(robot_key);
            REQUIRE(verify_result.is_ok());
            CHECK(verify_result.value() == true);

            // === Step 6: Verify credentials in presentation ===
            auto creds = presentation.getCredentials();
            CHECK(creds.size() == 2);

            // Verify robot auth credential
            auto auth_verify = creds[0].verify(fleet_manager_key);
            REQUIRE(auth_verify.is_ok());
            CHECK(auth_verify.value() == true);

            // Verify zone access credential
            auto zone_verify = creds[1].verify(fleet_manager_key);
            REQUIRE(zone_verify.is_ok());
            CHECK(zone_verify.value() == true);

            // === Step 7: Check status list ===
            auto status_list = blockit.getCredentialStatusList();
            CHECK(status_list->isActive(robot_auth_cred.getId()));
            CHECK(status_list->isActive(zone_cred.getId()));
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Credential revocation workflow") {
        auto temp_dir = fs::temp_directory_path() / "blockit_e2e_did_2";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<E2ETestData> blockit;
            auto genesis_key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), E2ETestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            // Create issuer and subject
            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();

            auto issuer_result = blockit.createDID(issuer_key);
            REQUIRE(issuer_result.is_ok());
            auto issuer_did = issuer_result.value().first.getId();

            auto subject_result = blockit.createDID(subject_key);
            REQUIRE(subject_result.is_ok());
            auto subject_did = subject_result.value().first.getId();

            // Issue a credential
            auto cred_result = blockit.issueCredential(issuer_key, subject_did, CredentialType::CapabilityGrant,
                                                       {{"capability", "admin_access"}, {"resource", "/system/*"}});
            REQUIRE(cred_result.is_ok());
            auto credential = cred_result.value();

            // Verify it's active
            auto status_list = blockit.getCredentialStatusList();
            CHECK(status_list->isActive(credential.getId()));

            // Revoke the credential
            auto revoke_result = status_list->recordRevoke(credential.getId(), issuer_did.toString(), "Security breach");
            REQUIRE(revoke_result.is_ok());

            // Verify it's revoked
            CHECK(status_list->isRevoked(credential.getId()));
            CHECK_FALSE(status_list->isActive(credential.getId()));

            // Try to use the credential - verifier would check status
            auto status_result = status_list->getStatus(credential.getId());
            REQUIRE(status_result.is_ok());
            CHECK(status_result.value() == CredentialStatus::Revoked);
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Multi-party credential workflow") {
        auto temp_dir = fs::temp_directory_path() / "blockit_e2e_did_3";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<E2ETestData> blockit;
            auto genesis_key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("multi-issuer-chain"),
                               dp::String("genesis"), E2ETestData{"genesis", 0}, crypto);
            blockit.initializeDID();

            // Create multiple issuers (different authorities)
            auto fleet_mgr_key = Key::generate().value();
            auto safety_cert_key = Key::generate().value();
            auto maintenance_key = Key::generate().value();

            // Create robot
            auto robot_key = Key::generate().value();
            auto robot_result = blockit.createDID(robot_key);
            REQUIRE(robot_result.is_ok());
            auto robot_did = robot_result.value().first.getId();

            // Fleet manager issues robot authorization
            auto fleet_mgr_did = DID::fromKey(fleet_mgr_key);
            CredentialIssuer fleet_issuer(fleet_mgr_did, fleet_mgr_key);

            auto auth_result = fleet_issuer.issueRobotAuthorization(robot_did, "PATROL_BOT_01", {"patrol", "report"});
            REQUIRE(auth_result.is_ok());
            auto auth_cred = auth_result.value();

            // Safety authority issues safety certification
            auto safety_did = DID::fromKey(safety_cert_key);
            CredentialIssuer safety_issuer(safety_did, safety_cert_key);

            auto safety_result = safety_issuer.issueCustomCredential(
                robot_did, CredentialType::Custom,
                {{"cert_type", "safety"}, {"rating", "A"}, {"certified_for", "indoor_operations"}},
                std::chrono::hours(24 * 365));
            REQUIRE(safety_result.is_ok());
            auto safety_cred = safety_result.value();

            // Maintenance issues calibration certificate
            auto maint_did = DID::fromKey(maintenance_key);
            CredentialIssuer maint_issuer(maint_did, maintenance_key);

            std::map<std::string, std::string> cal_data = {{"sensor_offset", "0.01"}, {"last_service", "2024-01-15"}};
            auto maint_result = maint_issuer.issueSensorCalibration(robot_did, "lidar", "LIDAR_001", cal_data);
            REQUIRE(maint_result.is_ok());
            auto maint_cred = maint_result.value();

            // Robot bundles all credentials into a presentation
            auto presentation = VerifiablePresentation::create(robot_did);
            presentation.addCredential(auth_cred);
            presentation.addCredential(safety_cred);
            presentation.addCredential(maint_cred);
            presentation.sign(robot_key, robot_did.withFragment("key-1"));

            // Verify presentation
            auto verify_result = presentation.verifyWithKey(robot_key);
            REQUIRE(verify_result.is_ok());
            CHECK(verify_result.value() == true);

            // Verify each credential from different issuers
            auto creds = presentation.getCredentials();
            CHECK(creds.size() == 3);

            auto auth_verify = creds[0].verify(fleet_mgr_key);
            REQUIRE(auth_verify.is_ok());
            CHECK(auth_verify.value() == true);

            auto safety_verify = creds[1].verify(safety_cert_key);
            REQUIRE(safety_verify.is_ok());
            CHECK(safety_verify.value() == true);

            auto maint_verify = creds[2].verify(maintenance_key);
            REQUIRE(maint_verify.is_ok());
            CHECK(maint_verify.value() == true);
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("DID authentication challenge-response") {
        auto temp_dir = fs::temp_directory_path() / "blockit_e2e_did_4";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<E2ETestData> blockit;
            auto genesis_key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("auth-chain"), dp::String("genesis"), E2ETestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            // Create robot identity
            auto robot_key = Key::generate().value();
            auto robot_result = blockit.createDID(robot_key);
            REQUIRE(robot_result.is_ok());
            auto robot_doc = robot_result.value().first;
            auto robot_did = robot_doc.getId();

            // Verifier generates a challenge
            std::vector<uint8_t> challenge = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

            // Robot signs the challenge
            auto sign_result = robot_key.sign(challenge);
            REQUIRE(sign_result.is_ok());
            auto signature = sign_result.value();

            // Verifier resolves DID and gets verification method
            auto resolve_result = blockit.resolveDID(robot_did);
            REQUIRE(resolve_result.is_ok());
            auto resolved_doc = resolve_result.value();

            // Get the key from the document
            auto vms = resolved_doc.getVerificationMethods();
            REQUIRE(!vms.empty());

            auto key_result = vms[0].toKey();
            REQUIRE(key_result.is_ok());
            auto resolved_key = key_result.value();

            // Verify signature using resolved key
            auto verify_result = resolved_key.verify(challenge, signature);
            REQUIRE(verify_result.is_ok());
            CHECK(verify_result.value() == true);
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Swarm coordination with DIDs") {
        auto temp_dir = fs::temp_directory_path() / "blockit_e2e_did_5";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<E2ETestData> blockit;
            auto genesis_key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("swarm-chain"), dp::String("genesis"), E2ETestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            // Create swarm coordinator
            auto coordinator_key = Key::generate().value();
            auto coordinator_result = blockit.createDID(coordinator_key);
            REQUIRE(coordinator_result.is_ok());
            auto coordinator_did = coordinator_result.value().first.getId();

            // Create swarm members
            std::vector<std::pair<Key, DID>> swarm_members;
            for (int i = 0; i < 5; i++) {
                auto member_key = Key::generate().value();
                auto member_result = blockit.createDID(member_key);
                REQUIRE(member_result.is_ok());
                swarm_members.push_back({member_key, member_result.value().first.getId()});
            }

            // Coordinator issues swarm membership credentials
            CredentialIssuer coordinator(coordinator_did, coordinator_key);

            for (size_t i = 0; i < swarm_members.size(); i++) {
                std::string role = (i == 0) ? "leader" : "member";
                auto membership_result = coordinator.issueSwarmMembership(swarm_members[i].second, "swarm-alpha", role);
                REQUIRE(membership_result.is_ok());

                auto cred = membership_result.value();
                CHECK(cred.getClaim("swarm_id").value() == "swarm-alpha");
                CHECK(cred.getClaim("role").value() == role);

                // Record in status list
                auto status_list = blockit.getCredentialStatusList();
                status_list->recordIssue(cred.getId(), coordinator_did.toString());
            }

            // Verify all swarm members have active credentials
            auto status_list = blockit.getCredentialStatusList();
            CHECK(status_list->size() == 5);
        }

        fs::remove_all(temp_dir);
    }
}
