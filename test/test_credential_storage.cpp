#include "blockit/identity/verifiable_credential.hpp"
#include "blockit/storage/file_store.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
using namespace blockit::storage;
namespace fs = std::filesystem;

TEST_SUITE("Credential Storage Tests") {

    TEST_CASE("Store and load credential") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_1";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            auto open_result = store.open(dp::String(temp_dir.string().c_str()));
            REQUIRE(open_result.is_ok());

            // Create keys and DIDs
            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);
            auto subject_did = DID::fromKey(subject_key);

            // Create a credential
            auto vc = VerifiableCredential::create("cred-001", CredentialType::RobotAuthorization, issuer_did, subject_did);
            vc.setClaim("robot_id", "ROBOT_001");
            auto vc_bytes = vc.serialize();

            // Create credential record
            CredentialRecord record;
            record.credential_id = dp::String("cred-001");
            record.issuer_did = dp::String(issuer_did.toString().c_str());
            record.subject_did = dp::String(subject_did.toString().c_str());
            record.credential = dp::Vector<dp::u8>(vc_bytes.begin(), vc_bytes.end());
            record.status = 0; // Active
            record.issued_at = 1000;
            record.expires_at = 0;

            // Store the credential
            auto tx = store.beginTransaction();
            auto store_result = store.storeCredential(record);
            REQUIRE(store_result.is_ok());
            tx->commit();

            // Load the credential
            auto loaded = store.loadCredential(record.credential_id);
            REQUIRE(loaded.has_value());
            CHECK(std::string(loaded->credential_id.c_str()) == "cred-001");
            CHECK(std::string(loaded->issuer_did.c_str()) == issuer_did.toString());
            CHECK(std::string(loaded->subject_did.c_str()) == subject_did.toString());
            CHECK(loaded->status == 0);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Load all credentials") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_2";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            auto open_result = store.open(dp::String(temp_dir.string().c_str()));
            REQUIRE(open_result.is_ok());

            auto issuer_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);

            // Create multiple credentials
            auto tx = store.beginTransaction();
            for (int i = 0; i < 5; i++) {
                auto subject_key = Key::generate().value();
                auto subject_did = DID::fromKey(subject_key);

                CredentialRecord record;
                record.credential_id = dp::String(("cred-" + std::to_string(i)).c_str());
                record.issuer_did = dp::String(issuer_did.toString().c_str());
                record.subject_did = dp::String(subject_did.toString().c_str());
                record.status = 0;
                record.issued_at = 1000 + i;

                auto store_result = store.storeCredential(record);
                REQUIRE(store_result.is_ok());
            }
            tx->commit();

            // Load all credentials
            auto all_creds = store.loadAllCredentials();
            CHECK(all_creds.size() == 5);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Load credentials by subject") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_3";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto issuer_key = Key::generate().value();
            auto subject1_key = Key::generate().value();
            auto subject2_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);
            auto subject1_did = DID::fromKey(subject1_key);
            auto subject2_did = DID::fromKey(subject2_key);

            auto tx = store.beginTransaction();

            // 3 credentials for subject1
            for (int i = 0; i < 3; i++) {
                CredentialRecord record;
                record.credential_id = dp::String(("cred-s1-" + std::to_string(i)).c_str());
                record.issuer_did = dp::String(issuer_did.toString().c_str());
                record.subject_did = dp::String(subject1_did.toString().c_str());
                record.status = 0;
                store.storeCredential(record);
            }

            // 2 credentials for subject2
            for (int i = 0; i < 2; i++) {
                CredentialRecord record;
                record.credential_id = dp::String(("cred-s2-" + std::to_string(i)).c_str());
                record.issuer_did = dp::String(issuer_did.toString().c_str());
                record.subject_did = dp::String(subject2_did.toString().c_str());
                record.status = 0;
                store.storeCredential(record);
            }

            tx->commit();

            // Load by subject
            auto s1_creds = store.loadCredentialsBySubject(dp::String(subject1_did.toString().c_str()));
            CHECK(s1_creds.size() == 3);

            auto s2_creds = store.loadCredentialsBySubject(dp::String(subject2_did.toString().c_str()));
            CHECK(s2_creds.size() == 2);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Load credentials by issuer") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_4";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto issuer1_key = Key::generate().value();
            auto issuer2_key = Key::generate().value();
            auto subject_key = Key::generate().value();
            auto issuer1_did = DID::fromKey(issuer1_key);
            auto issuer2_did = DID::fromKey(issuer2_key);
            auto subject_did = DID::fromKey(subject_key);

            auto tx = store.beginTransaction();

            // 4 credentials from issuer1
            for (int i = 0; i < 4; i++) {
                CredentialRecord record;
                record.credential_id = dp::String(("cred-i1-" + std::to_string(i)).c_str());
                record.issuer_did = dp::String(issuer1_did.toString().c_str());
                record.subject_did = dp::String(subject_did.toString().c_str());
                record.status = 0;
                store.storeCredential(record);
            }

            // 2 credentials from issuer2
            for (int i = 0; i < 2; i++) {
                CredentialRecord record;
                record.credential_id = dp::String(("cred-i2-" + std::to_string(i)).c_str());
                record.issuer_did = dp::String(issuer2_did.toString().c_str());
                record.subject_did = dp::String(subject_did.toString().c_str());
                record.status = 0;
                store.storeCredential(record);
            }

            tx->commit();

            // Load by issuer
            auto i1_creds = store.loadCredentialsByIssuer(dp::String(issuer1_did.toString().c_str()));
            CHECK(i1_creds.size() == 4);

            auto i2_creds = store.loadCredentialsByIssuer(dp::String(issuer2_did.toString().c_str()));
            CHECK(i2_creds.size() == 2);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Update credential status") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_5";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);
            auto subject_did = DID::fromKey(subject_key);

            CredentialRecord record;
            record.credential_id = dp::String("cred-001");
            record.issuer_did = dp::String(issuer_did.toString().c_str());
            record.subject_did = dp::String(subject_did.toString().c_str());
            record.status = 0; // Active

            auto tx = store.beginTransaction();
            store.storeCredential(record);
            tx->commit();

            // Update status to revoked (1)
            auto tx2 = store.beginTransaction();
            auto update_result = store.updateCredentialStatus(record.credential_id, 1);
            REQUIRE(update_result.is_ok());
            tx2->commit();

            // Verify status was updated
            auto loaded = store.loadCredential(record.credential_id);
            REQUIRE(loaded.has_value());
            CHECK(loaded->status == 1);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Get credential count") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_6";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            CHECK(store.getCredentialCount() == 0);

            auto issuer_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);

            auto tx = store.beginTransaction();
            for (int i = 0; i < 4; i++) {
                auto subject_key = Key::generate().value();
                auto subject_did = DID::fromKey(subject_key);

                CredentialRecord record;
                record.credential_id = dp::String(("cred-" + std::to_string(i)).c_str());
                record.issuer_did = dp::String(issuer_did.toString().c_str());
                record.subject_did = dp::String(subject_did.toString().c_str());
                record.status = 0;
                store.storeCredential(record);
            }
            tx->commit();

            CHECK(store.getCredentialCount() == 4);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Credential persistence across reopen") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_7";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        std::string cred_id = "persistent-cred-001";
        std::string issuer_str, subject_str;

        // Store a credential
        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);
            auto subject_did = DID::fromKey(subject_key);
            issuer_str = issuer_did.toString();
            subject_str = subject_did.toString();

            CredentialRecord record;
            record.credential_id = dp::String(cred_id.c_str());
            record.issuer_did = dp::String(issuer_str.c_str());
            record.subject_did = dp::String(subject_str.c_str());
            record.status = 0;

            auto tx = store.beginTransaction();
            store.storeCredential(record);
            tx->commit();
            store.close();
        }

        // Reopen and verify persistence
        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto loaded = store.loadCredential(dp::String(cred_id.c_str()));
            REQUIRE(loaded.has_value());
            CHECK(std::string(loaded->credential_id.c_str()) == cred_id);
            CHECK(std::string(loaded->issuer_did.c_str()) == issuer_str);
            CHECK(std::string(loaded->subject_did.c_str()) == subject_str);

            CHECK(store.getCredentialCount() == 1);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Transaction rollback clears pending credentials") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_8";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);
            auto subject_did = DID::fromKey(subject_key);

            CredentialRecord record;
            record.credential_id = dp::String("cred-rollback");
            record.issuer_did = dp::String(issuer_did.toString().c_str());
            record.subject_did = dp::String(subject_did.toString().c_str());
            record.status = 0;

            {
                auto tx = store.beginTransaction();
                store.storeCredential(record);
                tx->rollback();
            }

            // Credential should not be stored after rollback
            auto loaded = store.loadCredential(record.credential_id);
            CHECK_FALSE(loaded.has_value());
            CHECK(store.getCredentialCount() == 0);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Load non-existent credential returns empty") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_9";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto loaded = store.loadCredential(dp::String("nonexistent-cred"));
            CHECK_FALSE(loaded.has_value());

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Deserialize credential from stored bytes") {
        auto temp_dir = fs::temp_directory_path() / "blockit_cred_test_10";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            // Create and sign a credential
            auto issuer_key = Key::generate().value();
            auto subject_key = Key::generate().value();
            auto issuer_did = DID::fromKey(issuer_key);
            auto subject_did = DID::fromKey(subject_key);

            auto vc = VerifiableCredential::create("cred-deserialize", CredentialType::RobotAuthorization, issuer_did,
                                                   subject_did);
            vc.setClaim("robot_id", "ROBOT_TEST");
            vc.sign(issuer_key, issuer_did.withFragment("key-1"));

            auto vc_bytes = vc.serialize();

            // Store it
            CredentialRecord record;
            record.credential_id = dp::String("cred-deserialize");
            record.issuer_did = dp::String(issuer_did.toString().c_str());
            record.subject_did = dp::String(subject_did.toString().c_str());
            record.credential = dp::Vector<dp::u8>(vc_bytes.begin(), vc_bytes.end());
            record.status = 0;

            auto tx = store.beginTransaction();
            store.storeCredential(record);
            tx->commit();

            // Load and deserialize
            auto loaded = store.loadCredential(record.credential_id);
            REQUIRE(loaded.has_value());

            std::vector<uint8_t> cred_data(loaded->credential.begin(), loaded->credential.end());
            dp::ByteBuf buf(cred_data.begin(), cred_data.end());
            auto deser_result = VerifiableCredential::deserialize(buf);
            REQUIRE(deser_result.is_ok());

            auto loaded_vc = deser_result.value();
            CHECK(loaded_vc.getId() == "cred-deserialize");
            CHECK(loaded_vc.getClaim("robot_id").value() == "ROBOT_TEST");
            CHECK(loaded_vc.hasProof());

            // Verify signature
            auto verify_result = loaded_vc.verify(issuer_key);
            REQUIRE(verify_result.is_ok());
            CHECK(verify_result.value() == true);

            store.close();
        }

        fs::remove_all(temp_dir);
    }
}
