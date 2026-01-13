#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
using namespace blockit::storage;

TEST_SUITE("Validator Storage Tests") {
    TEST_CASE("Store and load validator") {
        std::filesystem::path test_dir = "/tmp/blockit_test_validator_storage";
        std::filesystem::remove_all(test_dir);

        FileStore store;
        auto open_result = store.open(String(test_dir.c_str()));
        REQUIRE(open_result.is_ok());

        auto init_result = store.initializeCoreSchema();
        REQUIRE(init_result.is_ok());

        // Create validator record
        ValidatorRecord record;
        record.validator_id = String("validator_001");
        record.participant_id = String("alice");
        record.weight = 10;
        record.status = 0; // ACTIVE
        record.last_seen = 1234567890;
        record.created_at = 1234567890;

        // Store validator
        auto store_result = store.storeValidator(record);
        REQUIRE(store_result.is_ok());

        // Load validator (from pending)
        auto loaded = store.loadValidator(String("validator_001"));
        REQUIRE(loaded.has_value());
        CHECK(std::string(loaded->validator_id.c_str()) == "validator_001");
        CHECK(std::string(loaded->participant_id.c_str()) == "alice");
        CHECK(loaded->weight == 10);
        CHECK(loaded->status == 0);

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Load all validators") {
        std::filesystem::path test_dir = "/tmp/blockit_test_validator_storage_all";
        std::filesystem::remove_all(test_dir);

        FileStore store;
        auto open_result = store.open(String(test_dir.c_str()));
        REQUIRE(open_result.is_ok());

        auto init_result = store.initializeCoreSchema();
        REQUIRE(init_result.is_ok());

        // Store multiple validators
        for (int i = 0; i < 3; i++) {
            ValidatorRecord record;
            record.validator_id = String(("validator_" + std::to_string(i)).c_str());
            record.participant_id = String(("participant_" + std::to_string(i)).c_str());
            record.weight = i + 1;
            record.status = 0;
            store.storeValidator(record);
        }

        auto all = store.loadAllValidators();
        CHECK(all.size() == 3);

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Update validator status") {
        std::filesystem::path test_dir = "/tmp/blockit_test_validator_update";
        std::filesystem::remove_all(test_dir);

        FileStore store;
        auto open_result = store.open(String(test_dir.c_str()));
        REQUIRE(open_result.is_ok());

        auto init_result = store.initializeCoreSchema();
        REQUIRE(init_result.is_ok());

        // Store validator
        ValidatorRecord record;
        record.validator_id = String("validator_001");
        record.participant_id = String("alice");
        record.weight = 10;
        record.status = 0; // ACTIVE
        store.storeValidator(record);

        // Update status to OFFLINE
        auto update_result = store.updateValidatorStatus(String("validator_001"), 1);
        REQUIRE(update_result.is_ok());

        // Load and verify
        auto loaded = store.loadValidator(String("validator_001"));
        REQUIRE(loaded.has_value());
        CHECK(loaded->status == 1); // OFFLINE

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Validator count") {
        std::filesystem::path test_dir = "/tmp/blockit_test_validator_count";
        std::filesystem::remove_all(test_dir);

        FileStore store;
        auto open_result = store.open(String(test_dir.c_str()));
        REQUIRE(open_result.is_ok());

        auto init_result = store.initializeCoreSchema();
        REQUIRE(init_result.is_ok());

        CHECK(store.getValidatorCount() == 0);

        ValidatorRecord record;
        record.validator_id = String("validator_001");
        record.participant_id = String("alice");
        store.storeValidator(record);

        CHECK(store.getValidatorCount() == 1);

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Validator not found") {
        std::filesystem::path test_dir = "/tmp/blockit_test_validator_notfound";
        std::filesystem::remove_all(test_dir);

        FileStore store;
        auto open_result = store.open(String(test_dir.c_str()));
        REQUIRE(open_result.is_ok());

        auto init_result = store.initializeCoreSchema();
        REQUIRE(init_result.is_ok());

        auto loaded = store.loadValidator(String("nonexistent"));
        CHECK_FALSE(loaded.has_value());

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Validator persistence after flush") {
        std::filesystem::path test_dir = "/tmp/blockit_test_validator_persist";
        std::filesystem::remove_all(test_dir);

        {
            FileStore store;
            auto open_result = store.open(String(test_dir.c_str()));
            REQUIRE(open_result.is_ok());

            auto init_result = store.initializeCoreSchema();
            REQUIRE(init_result.is_ok());

            ValidatorRecord record;
            record.validator_id = String("validator_001");
            record.participant_id = String("alice");
            record.weight = 5;
            record.status = 0;
            store.storeValidator(record);

            // Use transaction to flush
            auto tx = store.beginTransaction();
            store.storeValidator(record);
            tx->commit();
        }

        // Reopen and verify
        {
            FileStore store;
            auto open_result = store.open(String(test_dir.c_str()));
            REQUIRE(open_result.is_ok());

            auto loaded = store.loadValidator(String("validator_001"));
            REQUIRE(loaded.has_value());
            CHECK(std::string(loaded->participant_id.c_str()) == "alice");
            CHECK(loaded->weight == 5);
        }

        std::filesystem::remove_all(test_dir);
    }
}
