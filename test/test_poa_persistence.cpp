#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
using namespace blockit::storage;

TEST_SUITE("PoA Persistence Tests") {
    TEST_CASE("Save and restore validators to/from storage") {
        std::filesystem::path test_dir = "/tmp/blockit_test_poa_persist";
        std::filesystem::remove_all(test_dir);

        // Create validators and save to storage
        {
            FileStore store;
            auto open_result = store.open(String(test_dir.c_str()));
            REQUIRE(open_result.is_ok());
            store.initializeCoreSchema();

            // Create keys and store validator records
            auto key1 = Key::generate();
            auto key2 = Key::generate();
            REQUIRE(key1.is_ok());
            REQUIRE(key2.is_ok());

            auto key1_data = key1.value().serialize();
            ValidatorRecord record1;
            record1.validator_id = String(key1.value().getId().c_str());
            record1.participant_id = String("alice");
            record1.identity_data = dp::Vector<dp::u8>(key1_data.begin(), key1_data.end());
            record1.weight = 10;
            record1.status = 0;
            store.storeValidator(record1);

            auto key2_data = key2.value().serialize();
            ValidatorRecord record2;
            record2.validator_id = String(key2.value().getId().c_str());
            record2.participant_id = String("bob");
            record2.identity_data = dp::Vector<dp::u8>(key2_data.begin(), key2_data.end());
            record2.weight = 20;
            record2.status = 0;
            store.storeValidator(record2);

            // Flush to disk
            auto tx = store.beginTransaction();
            tx->commit();
        }

        // Restore and verify
        {
            FileStore store;
            auto open_result = store.open(String(test_dir.c_str()));
            REQUIRE(open_result.is_ok());

            auto all = store.loadAllValidators();
            CHECK(all.size() == 2);

            // Verify we can reconstruct keys from identity_data
            for (const auto &record : all) {
                std::vector<uint8_t> key_data(record.identity_data.begin(), record.identity_data.end());
                auto key_result = Key::deserialize(key_data);
                REQUIRE(key_result.is_ok());
                CHECK(key_result.value().getId() == std::string(record.validator_id.c_str()));
            }
        }

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Validator status persistence") {
        std::filesystem::path test_dir = "/tmp/blockit_test_poa_status";
        std::filesystem::remove_all(test_dir);

        std::string validator_id;

        {
            FileStore store;
            store.open(String(test_dir.c_str()));
            store.initializeCoreSchema();

            auto key = Key::generate();
            REQUIRE(key.is_ok());
            validator_id = key.value().getId();

            ValidatorRecord record;
            record.validator_id = String(validator_id.c_str());
            record.participant_id = String("alice");
            record.status = 0; // ACTIVE
            store.storeValidator(record);

            auto tx = store.beginTransaction();
            tx->commit();
        }

        // Update status
        {
            FileStore store;
            store.open(String(test_dir.c_str()));

            auto update_result = store.updateValidatorStatus(String(validator_id.c_str()), 2); // REVOKED
            REQUIRE(update_result.is_ok());

            auto tx = store.beginTransaction();
            tx->commit();
        }

        // Verify status persisted
        {
            FileStore store;
            store.open(String(test_dir.c_str()));

            auto loaded = store.loadValidator(String(validator_id.c_str()));
            REQUIRE(loaded.has_value());
            CHECK(loaded->status == 2); // REVOKED
        }

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("PoAConsensus with storage integration") {
        std::filesystem::path test_dir = "/tmp/blockit_test_poa_consensus";
        std::filesystem::remove_all(test_dir);

        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        // Generate keys
        auto alice_key = Key::generate();
        auto bob_key = Key::generate();
        REQUIRE(alice_key.is_ok());
        REQUIRE(bob_key.is_ok());

        // Add validators
        consensus.addValidator("alice", alice_key.value(), 10);
        consensus.addValidator("bob", bob_key.value(), 20);

        // Save validators to storage
        {
            FileStore store;
            store.open(String(test_dir.c_str()));
            store.initializeCoreSchema();

            for (auto *validator : consensus.getAllValidators()) {
                ValidatorRecord record;
                record.validator_id = String(validator->getId().c_str());
                record.participant_id = String(validator->getParticipantId().c_str());
                record.weight = validator->getWeight();
                record.status = static_cast<dp::i32>(validator->getStatus());
                store.storeValidator(record);
            }

            auto tx = store.beginTransaction();
            tx->commit();
        }

        // Verify persistence
        {
            FileStore store;
            store.open(String(test_dir.c_str()));

            auto all = store.loadAllValidators();
            CHECK(all.size() == 2);

            int total_weight = 0;
            for (const auto &v : all) {
                total_weight += v.weight;
            }
            CHECK(total_weight == 30);
        }

        std::filesystem::remove_all(test_dir);
    }
}
