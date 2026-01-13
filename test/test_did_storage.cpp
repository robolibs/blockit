#include "blockit/identity/did_document.hpp"
#include "blockit/storage/file_store.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
using namespace blockit::storage;
namespace fs = std::filesystem;

TEST_SUITE("DID Storage Tests") {

    TEST_CASE("Store and load DID") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_1";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            auto open_result = store.open(dp::String(temp_dir.string().c_str()));
            REQUIRE(open_result.is_ok());

            // Create a DID document
            auto key = Key::generate().value();
            auto did = DID::fromKey(key);
            auto doc = DIDDocument::create(did, key);
            auto doc_bytes = doc.serialize();

            // Create DID record
            DIDRecord record;
            record.did = dp::String(did.toString().c_str());
            record.document = dp::Vector<dp::u8>(doc_bytes.begin(), doc_bytes.end());
            record.version = 1;
            record.created_at = 1000;
            record.updated_at = 1000;
            record.status = 0; // Active

            // Store the DID
            auto tx = store.beginTransaction();
            auto store_result = store.storeDID(record);
            REQUIRE(store_result.is_ok());
            tx->commit();

            // Load the DID
            auto loaded = store.loadDID(record.did);
            REQUIRE(loaded.has_value());
            CHECK(std::string(loaded->did.c_str()) == did.toString());
            CHECK(loaded->version == 1);
            CHECK(loaded->status == 0);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Load all DIDs") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_2";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            auto open_result = store.open(dp::String(temp_dir.string().c_str()));
            REQUIRE(open_result.is_ok());

            // Create multiple DIDs
            auto tx = store.beginTransaction();
            for (int i = 0; i < 5; i++) {
                auto key = Key::generate().value();
                auto did = DID::fromKey(key);
                auto doc = DIDDocument::create(did, key);
                auto doc_bytes = doc.serialize();

                DIDRecord record;
                record.did = dp::String(did.toString().c_str());
                record.document = dp::Vector<dp::u8>(doc_bytes.begin(), doc_bytes.end());
                record.version = 1;
                record.created_at = 1000 + i;
                record.updated_at = 1000 + i;
                record.status = 0;

                auto store_result = store.storeDID(record);
                REQUIRE(store_result.is_ok());
            }
            tx->commit();

            // Load all DIDs
            auto all_dids = store.loadAllDIDs();
            CHECK(all_dids.size() == 5);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Update DID status") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_3";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            auto open_result = store.open(dp::String(temp_dir.string().c_str()));
            REQUIRE(open_result.is_ok());

            // Create and store a DID
            auto key = Key::generate().value();
            auto did = DID::fromKey(key);
            auto doc = DIDDocument::create(did, key);
            auto doc_bytes = doc.serialize();

            DIDRecord record;
            record.did = dp::String(did.toString().c_str());
            record.document = dp::Vector<dp::u8>(doc_bytes.begin(), doc_bytes.end());
            record.version = 1;
            record.created_at = 1000;
            record.updated_at = 1000;
            record.status = 0; // Active

            auto tx = store.beginTransaction();
            store.storeDID(record);
            tx->commit();

            // Update status to deactivated (1)
            auto tx2 = store.beginTransaction();
            auto update_result = store.updateDIDStatus(record.did, 1);
            REQUIRE(update_result.is_ok());
            tx2->commit();

            // Verify status was updated
            auto loaded = store.loadDID(record.did);
            REQUIRE(loaded.has_value());
            CHECK(loaded->status == 1);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Get DID count") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_4";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            auto open_result = store.open(dp::String(temp_dir.string().c_str()));
            REQUIRE(open_result.is_ok());

            CHECK(store.getDIDCount() == 0);

            // Add DIDs
            auto tx = store.beginTransaction();
            for (int i = 0; i < 3; i++) {
                auto key = Key::generate().value();
                auto did = DID::fromKey(key);

                DIDRecord record;
                record.did = dp::String(did.toString().c_str());
                record.version = 1;
                store.storeDID(record);
            }
            tx->commit();

            CHECK(store.getDIDCount() == 3);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("DID persistence across reopen") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_5";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        std::string did_str;

        // Store a DID
        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto key = Key::generate().value();
            auto did = DID::fromKey(key);
            did_str = did.toString();

            DIDRecord record;
            record.did = dp::String(did_str.c_str());
            record.version = 1;
            record.status = 0;

            auto tx = store.beginTransaction();
            store.storeDID(record);
            tx->commit();
            store.close();
        }

        // Reopen and verify persistence
        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto loaded = store.loadDID(dp::String(did_str.c_str()));
            REQUIRE(loaded.has_value());
            CHECK(std::string(loaded->did.c_str()) == did_str);

            CHECK(store.getDIDCount() == 1);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Transaction rollback clears pending DIDs") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_6";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto key = Key::generate().value();
            auto did = DID::fromKey(key);

            DIDRecord record;
            record.did = dp::String(did.toString().c_str());
            record.version = 1;

            {
                auto tx = store.beginTransaction();
                store.storeDID(record);
                tx->rollback();
            }

            // DID should not be stored after rollback
            auto loaded = store.loadDID(record.did);
            CHECK_FALSE(loaded.has_value());
            CHECK(store.getDIDCount() == 0);

            store.close();
        }

        fs::remove_all(temp_dir);
    }

    TEST_CASE("Load non-existent DID returns empty") {
        auto temp_dir = fs::temp_directory_path() / "blockit_did_test_7";
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        {
            FileStore store;
            store.open(dp::String(temp_dir.string().c_str()));

            auto loaded = store.loadDID(dp::String("did:blockit:nonexistent"));
            CHECK_FALSE(loaded.has_value());

            store.close();
        }

        fs::remove_all(temp_dir);
    }
}
