#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>
#include <thread>

using namespace blockit::ledger;

TEST_SUITE("PoA Proposal Tests") {
    TEST_CASE("Create proposal") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        consensus.addValidator("alice", key_result.value());

        auto proposal_id = consensus.createProposal("block_hash_123", "alice");
        CHECK(!proposal_id.empty());
        CHECK(proposal_id == "proposal_block_hash_123");
    }

    TEST_CASE("Add signature to proposal") {
        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        auto proposal_id = consensus.createProposal("block_hash_123", "alice");

        auto data = std::vector<uint8_t>{0x01};
        auto sign_result1 = key1.value().sign(data);
        auto sign_result2 = key2.value().sign(data);
        REQUIRE(sign_result1.is_ok());
        REQUIRE(sign_result2.is_ok());

        auto add_result1 = consensus.addSignature(proposal_id, key1.value().getId(), sign_result1.value());
        REQUIRE(add_result1.is_ok());
        CHECK(add_result1.value() == false); // Not quorum yet

        auto add_result2 = consensus.addSignature(proposal_id, key2.value().getId(), sign_result2.value());
        REQUIRE(add_result2.is_ok());
        CHECK(add_result2.value() == true); // Quorum reached
    }

    TEST_CASE("Proposal ready check") {
        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        auto proposal_id = consensus.createProposal("block_hash_123", "alice");

        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key1.value().sign(data);
        REQUIRE(sign_result.is_ok());

        consensus.addSignature(proposal_id, key1.value().getId(), sign_result.value());

        CHECK_FALSE(consensus.isProposalReady(proposal_id)); // Only 1 signature

        auto sign_result2 = key2.value().sign(data);
        REQUIRE(sign_result2.is_ok());

        consensus.addSignature(proposal_id, key2.value().getId(), sign_result2.value());

        CHECK(consensus.isProposalReady(proposal_id)); // 2 signatures, ready
    }

    TEST_CASE("Duplicate signature rejected") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());

        auto proposal_id = consensus.createProposal("block_hash_123", "alice");
        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key.value().sign(data);
        REQUIRE(sign_result.is_ok());

        auto add_result1 = consensus.addSignature(proposal_id, key.value().getId(), sign_result.value());
        CHECK(add_result1.is_ok());

        auto add_result2 = consensus.addSignature(proposal_id, key.value().getId(), sign_result.value());
        CHECK_FALSE(add_result2.is_ok()); // Duplicate signature rejected
    }

    TEST_CASE("Get finalized signatures") {
        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        auto proposal_id = consensus.createProposal("block_hash_123", "alice");
        auto data = std::vector<uint8_t>{0x01};
        auto sign_result1 = key1.value().sign(data);
        auto sign_result2 = key2.value().sign(data);
        REQUIRE(sign_result1.is_ok());
        REQUIRE(sign_result2.is_ok());

        consensus.addSignature(proposal_id, key1.value().getId(), sign_result1.value());
        consensus.addSignature(proposal_id, key2.value().getId(), sign_result2.value());

        auto get_result = consensus.getFinalizedSignatures(proposal_id);
        REQUIRE(get_result.is_ok());
        CHECK(get_result.value().size() == 2);
    }

    TEST_CASE("Get finalized signatures without quorum fails") {
        PoAConfig config;
        config.initial_required_signatures = 2;

        PoAConsensus consensus(config);

        auto key1 = Key::generate();
        auto key2 = Key::generate();
        REQUIRE(key1.is_ok());
        REQUIRE(key2.is_ok());

        consensus.addValidator("alice", key1.value());
        consensus.addValidator("bob", key2.value());

        auto proposal_id = consensus.createProposal("block_hash_123", "alice");
        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key1.value().sign(data);
        REQUIRE(sign_result.is_ok());

        consensus.addSignature(proposal_id, key1.value().getId(), sign_result.value());

        auto get_result = consensus.getFinalizedSignatures(proposal_id);
        CHECK_FALSE(get_result.is_ok()); // Not quorum
    }

    TEST_CASE("Remove proposal") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());
        auto proposal_id = consensus.createProposal("block_hash_123", "alice");

        CHECK(consensus.isProposalReady(proposal_id) == false); // Exists but not ready

        consensus.removeProposal(proposal_id);

        // After removal, operations should handle gracefully
        CHECK(consensus.isProposalReady(proposal_id) == false);
    }

    TEST_CASE("Proposal expiration") {
        PoAConfig config;
        config.signature_timeout_ms = 100; // 100ms for testing

        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());
        auto proposal_id = consensus.createProposal("block_hash_123", "alice");

        // Wait for expiration
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key.value().sign(data);
        REQUIRE(sign_result.is_ok());

        auto add_result = consensus.addSignature(proposal_id, key.value().getId(), sign_result.value());
        CHECK_FALSE(add_result.is_ok()); // Proposal expired
    }

    TEST_CASE("Cleanup expired proposals") {
        PoAConfig config;
        config.signature_timeout_ms = 50;

        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());
        consensus.createProposal("block_hash_123", "alice");

        // Wait for expiration
        std::this_thread::sleep_for(std::chrono::milliseconds(75));

        consensus.cleanupExpired();
        // Expired proposals should be removed (no crash)
    }

    TEST_CASE("Signature to non-existent proposal fails") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());

        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key.value().sign(data);
        REQUIRE(sign_result.is_ok());

        auto add_result = consensus.addSignature("non_existent_proposal", key.value().getId(), sign_result.value());
        CHECK_FALSE(add_result.is_ok());
    }

    TEST_CASE("Get finalized signatures from non-existent proposal fails") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto get_result = consensus.getFinalizedSignatures("non_existent_proposal");
        CHECK_FALSE(get_result.is_ok());
    }

    TEST_CASE("Multiple proposals can exist simultaneously") {
        PoAConfig config;
        config.initial_required_signatures = 1;

        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());

        auto proposal1 = consensus.createProposal("block_hash_1", "alice");
        auto proposal2 = consensus.createProposal("block_hash_2", "alice");
        auto proposal3 = consensus.createProposal("block_hash_3", "alice");

        CHECK(proposal1 != proposal2);
        CHECK(proposal2 != proposal3);

        // Add signature to only proposal2
        auto data = std::vector<uint8_t>{0x01};
        auto sign_result = key.value().sign(data);
        REQUIRE(sign_result.is_ok());

        consensus.addSignature(proposal2, key.value().getId(), sign_result.value());

        CHECK_FALSE(consensus.isProposalReady(proposal1));
        CHECK(consensus.isProposalReady(proposal2));
        CHECK_FALSE(consensus.isProposalReady(proposal3));
    }

    TEST_CASE("Signature stores actual signature data") {
        PoAConfig config;
        config.initial_required_signatures = 1;

        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("alice", key.value());

        auto proposal_id = consensus.createProposal("block_hash_123", "alice");

        auto data = std::vector<uint8_t>{0x01, 0x02, 0x03};
        auto sign_result = key.value().sign(data);
        REQUIRE(sign_result.is_ok());

        consensus.addSignature(proposal_id, key.value().getId(), sign_result.value());

        auto get_result = consensus.getFinalizedSignatures(proposal_id);
        REQUIRE(get_result.is_ok());

        const auto& signatures = get_result.value();
        CHECK(signatures.size() == 1);
        CHECK(signatures[0].signature == sign_result.value());
        CHECK(signatures[0].validator_id == key.value().getId());
        CHECK(signatures[0].participant_id == "alice");
    }
}
