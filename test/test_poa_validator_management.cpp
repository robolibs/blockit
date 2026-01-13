#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>
#include <thread>

using namespace blockit;

TEST_SUITE("PoA Validator Management Tests") {
    TEST_CASE("Add validator to consensus") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto add_result = consensus.addValidator("alice", key_result.value());
        CHECK(add_result.is_ok());

        auto get_result = consensus.getValidator(key_result.value().getId());
        CHECK(get_result.is_ok());

        const auto* validator = get_result.value();
        CHECK(validator != nullptr);
        CHECK(validator->getParticipantId() == "alice");
    }

    TEST_CASE("Add duplicate validator fails") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto add_result1 = consensus.addValidator("alice", key_result.value());
        CHECK(add_result1.is_ok());

        // Same key (same validator ID) should fail
        auto add_result2 = consensus.addValidator("alice_duplicate", key_result.value());
        CHECK_FALSE(add_result2.is_ok());
    }

    TEST_CASE("Remove validator") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();

        consensus.addValidator("alice", key_result.value());
        auto get_result1 = consensus.getValidator(validator_id);
        CHECK(get_result1.is_ok());

        auto remove_result = consensus.removeValidator(validator_id);
        CHECK(remove_result.is_ok());

        auto get_result2 = consensus.getValidator(validator_id);
        CHECK_FALSE(get_result2.is_ok());
    }

    TEST_CASE("Remove non-existent validator fails") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto remove_result = consensus.removeValidator("non_existent_id");
        CHECK_FALSE(remove_result.is_ok());
    }

    TEST_CASE("Get active validators") {
        PoAConfig config;
        PoAConsensus consensus(config);

        // Add 3 validators
        auto key1 = Key::generate();
        auto key2 = Key::generate();
        auto key3 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());
        REQUIRE(key3.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());
        consensus.addValidator("charlie", key3.value());

        auto active = consensus.getActiveValidators();
        CHECK(active.size() == 3);
    }

    TEST_CASE("Get active validators excludes expired") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        REQUIRE(key1.is_ok());

        // Create an expired key
        auto yesterday = std::chrono::system_clock::now() - std::chrono::hours(24);
        auto expired_key = Key::generateWithExpiration(yesterday);
        REQUIRE(expired_key.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("dave", expired_key.value()); // Expired

        auto active = consensus.getActiveValidators();
        CHECK(active.size() == 1); // Only Alice, Dave excluded (expired)
    }

    TEST_CASE("Get all validators") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        consensus.addValidator("alice", key_result.value());

        auto all = consensus.getAllValidators();
        CHECK(all.size() == 1);
    }

    TEST_CASE("Mark validator online") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());

        // Mark offline first
        consensus.markOffline(validator_id);
        auto get_result1 = consensus.getValidator(validator_id);
        REQUIRE(get_result1.is_ok());
        CHECK(get_result1.value()->getStatus() == ValidatorStatus::OFFLINE);

        // Mark online
        consensus.markOnline(validator_id);
        auto get_result2 = consensus.getValidator(validator_id);
        REQUIRE(get_result2.is_ok());
        CHECK(get_result2.value()->getStatus() == ValidatorStatus::ACTIVE);
    }

    TEST_CASE("Mark validator offline") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());
        consensus.markOffline(validator_id);

        auto get_result = consensus.getValidator(validator_id);
        REQUIRE(get_result.is_ok());
        CHECK(get_result.value()->getStatus() == ValidatorStatus::OFFLINE);
    }

    TEST_CASE("Revoke validator") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());
        consensus.revokeValidator(validator_id);

        auto get_result = consensus.getValidator(validator_id);
        REQUIRE(get_result.is_ok());
        CHECK(get_result.value()->getStatus() == ValidatorStatus::REVOKED);
    }

    TEST_CASE("Update validator activity") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        consensus.updateActivity(validator_id);
        auto get_result = consensus.getValidator(validator_id);
        REQUIRE(get_result.is_ok());
        CHECK(get_result.value()->isOnline(60000)); // Online within 60s
    }

    TEST_CASE("Add validator with custom weight") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        consensus.addValidator("alice", key_result.value(), 10);

        auto get_result = consensus.getValidator(key_result.value().getId());
        REQUIRE(get_result.is_ok());
        CHECK(get_result.value()->getWeight() == 10);
    }

    TEST_CASE("Revoked validators excluded from active") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        auto active1 = consensus.getActiveValidators();
        CHECK(active1.size() == 2);

        consensus.revokeValidator(key1.value().getId());

        auto active2 = consensus.getActiveValidators();
        CHECK(active2.size() == 1);
    }

    TEST_CASE("Offline validators excluded from active") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        consensus.markOffline(key1.value().getId());

        auto active = consensus.getActiveValidators();
        CHECK(active.size() == 1);
    }
}
