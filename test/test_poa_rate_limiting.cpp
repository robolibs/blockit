#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>
#include <thread>

using namespace blockit;

TEST_SUITE("PoA Rate Limiting Tests") {
    TEST_CASE("Check can propose") {
        PoAConfig config;
        config.max_proposals_per_hour = 10;
        config.min_seconds_between_proposals = 0; // No delay for testing

        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());

        auto can_propose = consensus.canPropose(validator_id);
        CHECK(can_propose.is_ok()); // First proposal, should succeed
    }

    TEST_CASE("Non-existent validator cannot propose") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto can_propose = consensus.canPropose("non_existent_id");
        CHECK_FALSE(can_propose.is_ok());
    }

    TEST_CASE("Inactive validator cannot propose") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());
        consensus.markOffline(validator_id);

        auto can_propose = consensus.canPropose(validator_id);
        CHECK_FALSE(can_propose.is_ok());
    }

    TEST_CASE("Rate limit exceeded") {
        PoAConfig config;
        config.max_proposals_per_hour = 2;
        config.min_seconds_between_proposals = 0;

        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());

        // Make 3 rapid proposals
        consensus.recordProposal(validator_id);
        consensus.recordProposal(validator_id);
        consensus.recordProposal(validator_id);

        auto can_propose = consensus.canPropose(validator_id);
        CHECK_FALSE(can_propose.is_ok()); // Rate limit exceeded
    }

    TEST_CASE("Proposal count tracking") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());

        CHECK(consensus.getProposalCount(validator_id) == 0);

        consensus.recordProposal(validator_id);
        CHECK(consensus.getProposalCount(validator_id) == 1);

        consensus.recordProposal(validator_id);
        CHECK(consensus.getProposalCount(validator_id) == 2);
    }

    TEST_CASE("Minimum time between proposals") {
        PoAConfig config;
        config.max_proposals_per_hour = 100;
        config.min_seconds_between_proposals = 1; // 1 second minimum

        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        std::string validator_id = key_result.value().getId();
        consensus.addValidator("alice", key_result.value());

        // Record first proposal
        consensus.recordProposal(validator_id);

        // Try immediately - should fail
        auto can_propose = consensus.canPropose(validator_id);
        CHECK_FALSE(can_propose.is_ok());

        // Wait for backoff period
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        // Should succeed now
        auto can_propose2 = consensus.canPropose(validator_id);
        CHECK(can_propose2.is_ok());
    }

    TEST_CASE("Proposal count for unknown validator") {
        PoAConfig config;
        PoAConsensus consensus(config);

        CHECK(consensus.getProposalCount("unknown_validator") == 0);
    }

    TEST_CASE("Multiple validators rate limited independently") {
        PoAConfig config;
        config.max_proposals_per_hour = 2;
        config.min_seconds_between_proposals = 0;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        std::string v1_id = key1.value().getId();
        std::string v2_id = key2.value().getId();

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        // Exhaust Alice's limit
        consensus.recordProposal(v1_id);
        consensus.recordProposal(v1_id);
        consensus.recordProposal(v1_id);

        // Alice can't propose
        CHECK_FALSE(consensus.canPropose(v1_id).is_ok());

        // But Bob still can
        CHECK(consensus.canPropose(v2_id).is_ok());
    }

    TEST_CASE("Record proposal for unknown validator") {
        PoAConfig config;
        PoAConsensus consensus(config);

        // Should not crash, just track it
        consensus.recordProposal("unknown_validator");
        CHECK(consensus.getProposalCount("unknown_validator") == 1);
    }

    TEST_CASE("Config can be updated") {
        PoAConfig config;
        config.max_proposals_per_hour = 5;

        PoAConsensus consensus(config);

        CHECK(consensus.getConfig().max_proposals_per_hour == 5);

        PoAConfig new_config;
        new_config.max_proposals_per_hour = 10;
        consensus.setConfig(new_config);

        CHECK(consensus.getConfig().max_proposals_per_hour == 10);
    }
}
