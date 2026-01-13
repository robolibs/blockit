#include "blockit/storage/blockit_store.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
namespace fs = std::filesystem;

// Test data type with required to_string() method
struct BlockitDIDTestData {
    dp::String name{};
    dp::i32 value{0};

    BlockitDIDTestData() = default;
    BlockitDIDTestData(const std::string &n, int v) : name(dp::String(n.c_str())), value(v) {}

    std::string to_string() const { return std::string(name.c_str()) + ":" + std::to_string(value); }

    auto members() { return std::tie(name, value); }
    auto members() const { return std::tie(name, value); }
};

TEST_SUITE("Blockit DID Integration Tests") {

    TEST_CASE("Initialize Blockit with DID support") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_1";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            auto init_result = blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"),
                                                  dp::String("genesis"), BlockitDIDTestData{"genesis", 0}, crypto);
            REQUIRE(init_result.is_ok());

            CHECK_FALSE(blockit.hasDID());

            blockit.initializeDID();

            CHECK(blockit.hasDID());
            CHECK(blockit.getDIDRegistry() != nullptr);
            CHECK(blockit.getCredentialStatusList() != nullptr);
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Create DID through Blockit") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_2";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            auto new_key = Key::generate().value();
            auto result = blockit.createDID(new_key);
            REQUIRE(result.is_ok());

            auto [doc, op] = result.value();
            CHECK(doc.isActive());
            CHECK(op.getOperationType() == DIDOperationType::Create);
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Resolve DID through Blockit") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_3";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            auto new_key = Key::generate().value();
            auto create_result = blockit.createDID(new_key);
            REQUIRE(create_result.is_ok());

            auto did = create_result.value().first.getId();

            // Resolve by DID
            auto resolve_result = blockit.resolveDID(did);
            REQUIRE(resolve_result.is_ok());
            CHECK(resolve_result.value().getId() == did);

            // Resolve by string
            auto resolve_str_result = blockit.resolveDID(did.toString());
            REQUIRE(resolve_str_result.is_ok());
            CHECK(resolve_str_result.value().getId() == did);
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Create robot identity through Blockit") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_4";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            auto robot_key = Key::generate().value();
            auto issuer_key = Key::generate().value();

            auto result = blockit.createRobotIdentity(robot_key, "ROBOT_007", {"patrol", "sensor_read", "report"},
                                                      issuer_key);
            REQUIRE(result.is_ok());

            auto [robot_doc, credential] = result.value();

            // Verify robot DID
            CHECK(robot_doc.isActive());

            // Verify credential
            CHECK(credential.hasType(CredentialType::RobotAuthorization));
            CHECK(credential.getClaim("robot_id").value() == "ROBOT_007");
            CHECK(credential.getClaim("capabilities").value() == "patrol,sensor_read,report");
            CHECK(credential.hasProof());

            // Verify robot DID is resolvable
            auto resolve_result = blockit.resolveDID(robot_doc.getId());
            REQUIRE(resolve_result.is_ok());

            // Verify credential is in status list
            auto status_list = blockit.getCredentialStatusList();
            CHECK(status_list->isActive(credential.getId()));
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Issue credential through Blockit") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_5";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();

            // Create subject DID
            auto subject_result = blockit.createDID(subject_key);
            REQUIRE(subject_result.is_ok());
            auto subject_did = subject_result.value().first.getId();

            // Issue credential
            std::map<std::string, std::string> claims = {{"access_level", "admin"}, {"department", "engineering"}};

            auto cred_result = blockit.issueCredential(issuer_key, subject_did, CredentialType::Custom, claims,
                                                       std::chrono::hours(24));
            REQUIRE(cred_result.is_ok());

            auto credential = cred_result.value();
            CHECK(credential.getClaim("access_level").value() == "admin");
            CHECK(credential.getClaim("department").value() == "engineering");
            CHECK(credential.hasProof());
            CHECK(credential.getExpirationDate() > 0);

            // Verify in status list
            auto status_list = blockit.getCredentialStatusList();
            CHECK(status_list->isActive(credential.getId()));
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Store and load credential through Blockit") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_6";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        std::string cred_id;

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();
            auto subject_result = blockit.createDID(subject_key);
            REQUIRE(subject_result.is_ok());
            auto subject_did = subject_result.value().first.getId();

            auto cred_result = blockit.issueCredential(issuer_key, subject_did, CredentialType::ZoneAccess,
                                                       {{"zone_id", "warehouse_a"}});
            REQUIRE(cred_result.is_ok());
            cred_id = cred_result.value().getId();
        }

        // Reopen and verify credential persisted
        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);

            auto loaded = blockit.loadCredential(cred_id);
            REQUIRE(loaded.has_value());
            CHECK(loaded->hasType(CredentialType::ZoneAccess));
            CHECK(loaded->getClaim("zone_id").value() == "warehouse_a");
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("DID not initialized error handling") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_7";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);

            // Don't initialize DID, should fail
            auto new_key = Key::generate().value();
            auto result = blockit.createDID(new_key);
            CHECK(result.is_err());
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Store not initialized error handling") {
        Blockit<BlockitDIDTestData> blockit;
        auto key = Key::generate().value();

        // Don't initialize store, should fail
        auto result = blockit.createDID(key);
        CHECK(result.is_err());
    }

    TEST_CASE("Multiple robot identities") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_8";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            Blockit<BlockitDIDTestData> blockit;
            auto key = Key::generate().value();
            auto crypto = std::make_shared<Crypto>("test_key");

            blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("test-chain"), dp::String("genesis"), BlockitDIDTestData{"genesis", 0},
                               crypto);
            blockit.initializeDID();

            auto issuer_key = Key::generate().value();

            // Create multiple robot identities
            for (int i = 0; i < 3; i++) {
                auto robot_key = Key::generate().value();
                auto result = blockit.createRobotIdentity(robot_key, "ROBOT_" + std::to_string(i), {"move", "sense"},
                                                          issuer_key);
                REQUIRE(result.is_ok());
            }

            // Verify all DIDs were created (3 robots + 1 issuer)
            auto registry = blockit.getDIDRegistry();
            CHECK(registry->size() == 4);

            // Verify all credentials are active
            auto status_list = blockit.getCredentialStatusList();
            CHECK(status_list->size() == 3);
        }

        fs::remove_all(temp_dir);
    }
}
