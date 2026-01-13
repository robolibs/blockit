#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>

using namespace blockit::ledger;

TEST_SUITE("PoA Quorum Tests") {
    TEST_CASE("Calculate required signatures") {
        PoAConfig config;
        config.initial_required_signatures = 2;
        config.minimum_required_signatures = 1;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        auto key3 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());
        REQUIRE(key3.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        CHECK(consensus.getRequiredSignatures() == 2); // 2 active, 2 required

        consensus.addValidator("charlie", key3.value());
        CHECK(consensus.getRequiredSignatures() == 2); // Still 2
    }

    TEST_CASE("Dynamic threshold with offline validators") {
        PoAConfig config;
        config.initial_required_signatures = 2;
        config.minimum_required_signatures = 1;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        auto key3 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());
        REQUIRE(key3.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());
        consensus.addValidator("charlie", key3.value());

        consensus.markOffline(key3.value().getId()); // Charlie offline
        CHECK(consensus.getRequiredSignatures() == 2); // 2 active, 2 required
    }

    TEST_CASE("Reduce to minimum when insufficient") {
        PoAConfig config;
        config.initial_required_signatures = 3;
        config.minimum_required_signatures = 1;

        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());

        CHECK(consensus.getRequiredSignatures() == 1); // Only 1 active, reduce to minimum
    }

    TEST_CASE("Check quorum with signatures") {
        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        std::vector<BlockSignature> signatures;

        // Add Alice's signature
        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key1.value().sign(data);
        REQUIRE(sign_result.is_ok());

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        signatures.push_back({
            key1.value().getId(),
            "alice",
            sign_result.value(),
            now_ms
        });

        CHECK_FALSE(consensus.hasQuorum(signatures)); // Only 1 of 2

        // Add Bob's signature
        auto sign_result2 = key2.value().sign(data);
        REQUIRE(sign_result2.is_ok());

        signatures.push_back({
            key2.value().getId(),
            "bob",
            sign_result2.value(),
            now_ms
        });

        CHECK(consensus.hasQuorum(signatures)); // 2 of 2, quorum reached
    }

    TEST_CASE("Quorum counts unique validators only") {
        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        std::vector<BlockSignature> signatures;

        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key1.value().sign(data);
        REQUIRE(sign_result.is_ok());

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Add Alice's signature twice
        signatures.push_back({key1.value().getId(), "alice", sign_result.value(), now_ms});
        signatures.push_back({key1.value().getId(), "alice", sign_result.value(), now_ms});

        // Should not reach quorum (only 1 unique signer)
        CHECK_FALSE(consensus.hasQuorum(signatures));
    }

    TEST_CASE("Get active validator count") {
        PoAConfig config;
        PoAConsensus consensus(config);

        CHECK(consensus.getActiveValidatorCount() == 0);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        CHECK(consensus.getActiveValidatorCount() == 1);

        consensus.addValidator("bob", key2.value());
        CHECK(consensus.getActiveValidatorCount() == 2);

        consensus.markOffline(key1.value().getId());
        CHECK(consensus.getActiveValidatorCount() == 1);
    }

    TEST_CASE("Get total active weight") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        auto key3 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());
        REQUIRE(key3.is_ok());

        consensus.addValidator("alice", key1.value(), 10);
        consensus.addValidator("bob", key2.value(), 20);
        consensus.addValidator("charlie", key3.value(), 30);

        CHECK(consensus.getTotalActiveWeight() == 60);

        consensus.markOffline(key2.value().getId());
        CHECK(consensus.getTotalActiveWeight() == 40); // Without Bob
    }

    TEST_CASE("Required signatures with no validators") {
        PoAConfig config;
        config.initial_required_signatures = 2;
        config.minimum_required_signatures = 1;

        PoAConsensus consensus(config);

        // With no validators, should return minimum (or 0 if min > active)
        int required = consensus.getRequiredSignatures();
        CHECK(required >= 0);
        CHECK(required <= config.minimum_required_signatures);
    }

    TEST_CASE("Quorum threshold configurable") {
        PoAConfig config;
        config.initial_required_signatures = 5;
        config.minimum_required_signatures = 2;

        PoAConsensus consensus(config);

        // Add 10 validators
        std::vector<Key> keys;
        for (int i = 0; i < 10; i++) {
            auto key = Key::generate();
            REQUIRE(key.is_ok());
            keys.push_back(key.value());
            consensus.addValidator("validator_" + std::to_string(i), key.value());
        }

        CHECK(consensus.getRequiredSignatures() == 5);

        // Mark 6 as offline (leaving only 4 active)
        for (int i = 0; i < 6; i++) {
            consensus.markOffline(keys[i].getId());
        }

        // Should adjust to 4 (all remaining active) or minimum if 4 < minimum
        int required = consensus.getRequiredSignatures();
        CHECK(required <= 4);
        CHECK(required >= config.minimum_required_signatures);
    }
}
