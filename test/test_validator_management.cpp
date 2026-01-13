#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>
#include <thread>

using namespace blockit;

TEST_SUITE("Validator Management Tests") {
    TEST_CASE("Create validator with permanent key") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        Validator validator("alice", key);

        CHECK(validator.getId() == key.getId());
        CHECK(validator.getParticipantId() == "alice");
        CHECK(validator.getIdentityType() == "key");
        CHECK(validator.getWeight() == 1);
        CHECK(validator.getStatus() == ValidatorStatus::ACTIVE);
        CHECK(validator.canSign());
    }

    TEST_CASE("Create validator with custom weight") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("bob", key_result.value(), 10);

        CHECK(validator.getWeight() == 10);
    }

    TEST_CASE("Create validator with expiring key") {
        auto now = std::chrono::system_clock::now();
        auto tomorrow = now + std::chrono::hours(24);

        auto key_result = Key::generateWithExpiration(tomorrow);
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        Validator validator("charlie", key);

        CHECK(validator.canSign()); // Not expired yet
    }

    TEST_CASE("Create validator with already expired key") {
        auto now = std::chrono::system_clock::now();
        auto yesterday = now - std::chrono::hours(24);

        auto key_result = Key::generateWithExpiration(yesterday);
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        Validator validator("dave", key);

        CHECK_FALSE(validator.canSign()); // Expired
        CHECK(validator.getStatus() == ValidatorStatus::ACTIVE); // Status still ACTIVE
    }

    TEST_CASE("Set validator weight") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        CHECK(validator.getWeight() == 1);
        validator.setWeight(10);
        CHECK(validator.getWeight() == 10);
    }

    TEST_CASE("Multiple validators with weights") {
        std::vector<std::unique_ptr<Validator>> validators;
        for (int i = 0; i < 5; i++) {
            auto key_result = Key::generate();
            REQUIRE(key_result.is_ok());

            int weight = (i + 1) * 10;
            validators.push_back(std::make_unique<Validator>(
                "validator_" + std::to_string(i),
                key_result.value(),
                weight
            ));
        }

        int total_weight = 0;
        for (const auto& v : validators) {
            total_weight += v->getWeight();
        }
        CHECK(total_weight == 150); // 10 + 20 + 30 + 40 + 50
    }

    TEST_CASE("Revoke validator") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        CHECK(validator.getStatus() == ValidatorStatus::ACTIVE);
        CHECK(validator.canSign());

        validator.revokeValidator();
        CHECK(validator.getStatus() == ValidatorStatus::REVOKED);
        CHECK_FALSE(validator.canSign());
    }

    TEST_CASE("Mark offline then online") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        validator.markOffline();
        CHECK(validator.getStatus() == ValidatorStatus::OFFLINE);
        CHECK_FALSE(validator.canSign());

        validator.markOnline();
        CHECK(validator.getStatus() == ValidatorStatus::ACTIVE);
        CHECK(validator.canSign());
    }

    TEST_CASE("Status prevents signing") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        // Normal signing should work
        std::vector<uint8_t> data = {0x01, 0x02};
        auto sign_result1 = validator.sign(data);
        CHECK(sign_result1.is_ok());

        // After revocation, signing should fail
        validator.revokeValidator();
        auto sign_result2 = validator.sign(data);
        CHECK_FALSE(sign_result2.is_ok());
    }

    TEST_CASE("Validator signs data") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto sign_result = validator.sign(data);

        REQUIRE(sign_result.is_ok());
        CHECK(sign_result.value().size() == 64); // Ed25519 signature
    }

    TEST_CASE("Validator verifies own signature") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto sign_result = validator.sign(data);
        REQUIRE(sign_result.is_ok());

        auto verify_result = validator.verify(data, sign_result.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Validator verifies other validator signature") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        Validator validator1("alice", key1_result.value());
        Validator validator2("bob", key2_result.value());

        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto sign_result = validator1.sign(data);
        REQUIRE(sign_result.is_ok());

        // Verification with different validator should fail
        auto verify_result = validator2.verify(data, sign_result.value());
        if (verify_result.is_ok()) {
            CHECK(verify_result.value() == false);
        }
    }

    TEST_CASE("Access underlying Key identity") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        const auto& identity = validator.getIdentity();
        CHECK(identity.getId() == key_result.value().getId());
    }
}
