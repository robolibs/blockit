#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
using namespace blockit::storage;

// Test data structure
struct SensorData {
    std::string sensor_id;
    double temperature;
    double humidity;
    int64_t timestamp;

    std::string to_string() const {
        return "SensorData{" + sensor_id + "," + std::to_string(temperature) + "," + std::to_string(humidity) + "," +
               std::to_string(timestamp) + "}";
    }

    auto members() { return std::tie(sensor_id, temperature, humidity, timestamp); }
    auto members() const { return std::tie(sensor_id, temperature, humidity, timestamp); }
};

TEST_SUITE("Blockit PoA Integration Tests") {
    TEST_CASE("Full PoA workflow with FileStore") {
        std::filesystem::path test_dir = "/tmp/blockit_test_full_poa";
        std::filesystem::remove_all(test_dir);

        // Setup
        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        auto alice_key = Key::generate();
        auto bob_key = Key::generate();
        REQUIRE(alice_key.is_ok());
        REQUIRE(bob_key.is_ok());

        consensus.addValidator("alice", alice_key.value(), 10);
        consensus.addValidator("bob", bob_key.value(), 10);

        // Open storage
        FileStore store;
        auto open_result = store.open(String(test_dir.c_str()));
        REQUIRE(open_result.is_ok());
        store.initializeCoreSchema();

        // Create and sign a block
        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<SensorData> tx("sensor_reading_001", SensorData{"temp_sensor_1", 25.5, 60.0, 1234567890}, 100);
        tx.signTransaction(privateKey);

        Block<SensorData> block({tx});
        block.setProposer("alice");

        auto hash = block.calculateHash();
        REQUIRE(hash.is_ok());
        std::vector<uint8_t> hash_bytes(hash.value().begin(), hash.value().end());

        // Both validators sign
        auto alice_sig = alice_key.value().sign(hash_bytes);
        auto bob_sig = bob_key.value().sign(hash_bytes);
        REQUIRE(alice_sig.is_ok());
        REQUIRE(bob_sig.is_ok());

        block.addValidatorSignature(alice_key.value().getId(), "alice", alice_sig.value());
        block.addValidatorSignature(bob_key.value().getId(), "bob", bob_sig.value());

        // Store validators
        for (auto *validator : consensus.getAllValidators()) {
            ValidatorRecord record;
            record.validator_id = String(validator->getId().c_str());
            record.participant_id = String(validator->getParticipantId().c_str());
            auto key_data = validator->getIdentity().serialize();
            record.identity_data = dp::Vector<dp::u8>(key_data.begin(), key_data.end());
            record.weight = validator->getWeight();
            record.status = static_cast<dp::i32>(validator->getStatus());
            store.storeValidator(record);
        }

        // Commit
        auto tx_guard = store.beginTransaction();
        tx_guard->commit();

        // Verify
        auto all_validators = store.loadAllValidators();
        CHECK(all_validators.size() == 2);
        CHECK(block.countValidSignatures() == 2);
        CHECK(block.getProposer() == "alice");

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Validator recovery from storage") {
        std::filesystem::path test_dir = "/tmp/blockit_test_validator_recovery";
        std::filesystem::remove_all(test_dir);

        std::string alice_id, bob_id;

        // First session - create and save validators
        {
            FileStore store;
            store.open(String(test_dir.c_str()));
            store.initializeCoreSchema();

            auto alice_key = Key::generate();
            auto bob_key = Key::generate();
            REQUIRE(alice_key.is_ok());
            REQUIRE(bob_key.is_ok());

            alice_id = alice_key.value().getId();
            bob_id = bob_key.value().getId();

            ValidatorRecord alice_record;
            alice_record.validator_id = String(alice_id.c_str());
            alice_record.participant_id = String("alice");
            auto alice_data = alice_key.value().serialize();
            alice_record.identity_data = dp::Vector<dp::u8>(alice_data.begin(), alice_data.end());
            alice_record.weight = 10;
            alice_record.status = 0;
            store.storeValidator(alice_record);

            ValidatorRecord bob_record;
            bob_record.validator_id = String(bob_id.c_str());
            bob_record.participant_id = String("bob");
            auto bob_data = bob_key.value().serialize();
            bob_record.identity_data = dp::Vector<dp::u8>(bob_data.begin(), bob_data.end());
            bob_record.weight = 20;
            bob_record.status = 0;
            store.storeValidator(bob_record);

            auto tx = store.beginTransaction();
            tx->commit();
        }

        // Second session - recover validators
        {
            FileStore store;
            store.open(String(test_dir.c_str()));

            auto all = store.loadAllValidators();
            REQUIRE(all.size() == 2);

            // Rebuild PoA consensus from storage
            PoAConfig config;
            config.initial_required_signatures = 2;
            PoAConsensus consensus(config);

            for (const auto &record : all) {
                std::vector<uint8_t> key_data(record.identity_data.begin(), record.identity_data.end());
                auto key_result = Key::deserialize(key_data);
                REQUIRE(key_result.is_ok());

                consensus.addValidator(std::string(record.participant_id.c_str()), key_result.value(), record.weight);
            }

            CHECK(consensus.getActiveValidatorCount() == 2);
            CHECK(consensus.getRequiredSignatures() == 2);

            // Verify we can get validators by ID
            auto alice = consensus.getValidator(alice_id);
            auto bob = consensus.getValidator(bob_id);
            REQUIRE(alice.is_ok());
            REQUIRE(bob.is_ok());
            CHECK(alice.value()->getWeight() == 10);
            CHECK(bob.value()->getWeight() == 20);
        }

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Block signature verification") {
        auto alice_key = Key::generate();
        auto bob_key = Key::generate();
        REQUIRE(alice_key.is_ok());
        REQUIRE(bob_key.is_ok());

        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<SensorData> tx("tx_001", SensorData{"sensor", 20.0, 50.0, 123}, 100);
        tx.signTransaction(privateKey);

        Block<SensorData> block({tx});

        auto hash = block.calculateHash();
        REQUIRE(hash.is_ok());
        std::vector<uint8_t> hash_bytes(hash.value().begin(), hash.value().end());

        // Sign with Alice's key
        auto sig = alice_key.value().sign(hash_bytes);
        REQUIRE(sig.is_ok());

        // Verify with Alice's key (should pass)
        auto verify_alice = alice_key.value().verify(hash_bytes, sig.value());
        REQUIRE(verify_alice.is_ok());
        CHECK(verify_alice.value() == true);

        // Verify with Bob's key (should fail - wrong key)
        auto verify_bob = bob_key.value().verify(hash_bytes, sig.value());
        REQUIRE(verify_bob.is_ok());
        CHECK(verify_bob.value() == false);
    }
}
