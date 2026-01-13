#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>

using namespace blockit::ledger;

TEST_SUITE("Validator Serialization Tests") {
    TEST_CASE("Serialize and deserialize validator") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator1("alice", key_result.value(), 5);

        auto serialized = validator1.serialize();
        CHECK(!serialized.empty());

        auto deserialize_result = Validator::deserialize(serialized);
        REQUIRE(deserialize_result.is_ok());

        const auto& validator2 = deserialize_result.value();
        CHECK(validator1.getId() == validator2.getId());
        CHECK(validator1.getParticipantId() == validator2.getParticipantId());
        CHECK(validator1.getWeight() == validator2.getWeight());
        CHECK(validator1.getStatus() == validator2.getStatus());
    }

    TEST_CASE("Serialize validator with different statuses") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        // Test ACTIVE status
        {
            Validator validator("alice", key_result.value());
            validator.setStatus(ValidatorStatus::ACTIVE);

            auto serialized = validator.serialize();
            auto result = Validator::deserialize(serialized);
            REQUIRE(result.is_ok());
            CHECK(result.value().getStatus() == ValidatorStatus::ACTIVE);
        }

        // Test OFFLINE status
        {
            Validator validator("bob", key_result.value());
            validator.setStatus(ValidatorStatus::OFFLINE);

            auto serialized = validator.serialize();
            auto result = Validator::deserialize(serialized);
            REQUIRE(result.is_ok());
            CHECK(result.value().getStatus() == ValidatorStatus::OFFLINE);
        }

        // Test REVOKED status
        {
            Validator validator("charlie", key_result.value());
            validator.setStatus(ValidatorStatus::REVOKED);

            auto serialized = validator.serialize();
            auto result = Validator::deserialize(serialized);
            REQUIRE(result.is_ok());
            CHECK(result.value().getStatus() == ValidatorStatus::REVOKED);
        }
    }

    TEST_CASE("Serialize validator with various weights") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::vector<int> weights = {0, 1, 10, 100, 1000, 10000};

        for (int weight : weights) {
            Validator validator("test", key_result.value(), weight);

            auto serialized = validator.serialize();
            auto result = Validator::deserialize(serialized);
            REQUIRE(result.is_ok());
            CHECK(result.value().getWeight() == weight);
        }
    }

    TEST_CASE("Serialize validator with long participant ID") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string long_id(1000, 'x'); // 1000 character ID
        Validator validator(long_id, key_result.value());

        auto serialized = validator.serialize();
        auto result = Validator::deserialize(serialized);
        REQUIRE(result.is_ok());
        CHECK(result.value().getParticipantId() == long_id);
    }

    TEST_CASE("Deserialize invalid validator data") {
        std::vector<uint8_t> invalid_data(10, 0xFF); // Too short

        auto result = Validator::deserialize(invalid_data);
        CHECK_FALSE(result.is_ok());
    }

    TEST_CASE("Deserialize empty data") {
        std::vector<uint8_t> empty;

        auto result = Validator::deserialize(empty);
        CHECK_FALSE(result.is_ok());
    }

    TEST_CASE("Serialized validator can sign and verify") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator1("alice", key_result.value());

        // Sign with original
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto sign_result = validator1.sign(data);
        REQUIRE(sign_result.is_ok());

        // Serialize and deserialize
        auto serialized = validator1.serialize();
        auto deserialize_result = Validator::deserialize(serialized);
        REQUIRE(deserialize_result.is_ok());

        auto& validator2 = deserialize_result.value();

        // Verify with deserialized
        auto verify_result = validator2.verify(data, sign_result.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);

        // Deserialized can also sign
        auto sign_result2 = validator2.sign(data);
        REQUIRE(sign_result2.is_ok());
    }

    TEST_CASE("Last seen timestamp preserved") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator1("alice", key_result.value());
        validator1.updateActivity();

        auto original_last_seen = validator1.getLastSeen();

        auto serialized = validator1.serialize();
        auto result = Validator::deserialize(serialized);
        REQUIRE(result.is_ok());

        CHECK(result.value().getLastSeen() == original_last_seen);
    }

    TEST_CASE("Key identity preserved through serialization") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator1("alice", key_result.value());
        std::string original_id = validator1.getId();

        auto serialized = validator1.serialize();
        auto result = Validator::deserialize(serialized);
        REQUIRE(result.is_ok());

        CHECK(result.value().getId() == original_id);
        CHECK(result.value().getIdentity().getPublicKey() == key_result.value().getPublicKey());
    }
}
