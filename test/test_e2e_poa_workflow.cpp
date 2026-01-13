#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <filesystem>

using namespace blockit;
using namespace blockit::storage;

// Robot swarm task data
struct SwarmTask {
    std::string task_id;
    std::string task_type;
    std::string assigned_to;
    std::string status;
    double priority;

    std::string to_string() const {
        return "SwarmTask{" + task_id + "," + task_type + "," + assigned_to + "," + status + "," +
               std::to_string(priority) + "}";
    }

    auto members() { return std::tie(task_id, task_type, assigned_to, status, priority); }
    auto members() const { return std::tie(task_id, task_type, assigned_to, status, priority); }
};

TEST_SUITE("End-to-End PoA Workflow Tests") {
    TEST_CASE("Complete robot swarm consensus workflow") {
        std::filesystem::path test_dir = "/tmp/blockit_test_e2e_workflow";
        std::filesystem::remove_all(test_dir);

        // === Phase 1: Initialize swarm leaders as validators ===
        PoAConfig config;
        config.initial_required_signatures = 2;
        config.minimum_required_signatures = 1;
        config.signature_timeout_ms = 30000;

        PoAConsensus consensus(config);

        auto leader1_key = Key::generate();
        auto leader2_key = Key::generate();
        auto leader3_key = Key::generate();
        REQUIRE(leader1_key.is_ok());
        REQUIRE(leader2_key.is_ok());
        REQUIRE(leader3_key.is_ok());

        consensus.addValidator("leader_alpha", leader1_key.value(), 10);
        consensus.addValidator("leader_beta", leader2_key.value(), 10);
        consensus.addValidator("leader_gamma", leader3_key.value(), 10);

        CHECK(consensus.getActiveValidatorCount() == 3);
        CHECK(consensus.getRequiredSignatures() == 2);

        // === Phase 2: Create task assignment transaction ===
        auto privateKey = std::make_shared<Crypto>("swarm_key");
        Transaction<SwarmTask> task_tx("task_001", SwarmTask{"task_001", "patrol", "robot_scout_1", "pending", 0.8},
                                       100);
        task_tx.signTransaction(privateKey);

        // === Phase 3: Create block and propose ===
        Block<SwarmTask> block({task_tx});
        block.setProposer("leader_alpha");

        auto block_hash = block.calculateHash();
        REQUIRE(block_hash.is_ok());

        auto proposal_id = consensus.createProposal(block_hash.value(), leader1_key.value().getId());
        CHECK(!proposal_id.empty());

        // === Phase 4: Collect signatures (quorum = 2) ===
        std::vector<uint8_t> hash_bytes(block_hash.value().begin(), block_hash.value().end());

        // Leader Alpha signs
        auto sig1 = leader1_key.value().sign(hash_bytes);
        REQUIRE(sig1.is_ok());
        auto result1 = consensus.addSignature(proposal_id, leader1_key.value().getId(), sig1.value());
        REQUIRE(result1.is_ok());
        CHECK_FALSE(result1.value()); // Not quorum yet

        block.addValidatorSignature(leader1_key.value().getId(), "leader_alpha", sig1.value());
        CHECK_FALSE(consensus.isProposalReady(proposal_id));

        // Leader Beta signs
        auto sig2 = leader2_key.value().sign(hash_bytes);
        REQUIRE(sig2.is_ok());
        auto result2 = consensus.addSignature(proposal_id, leader2_key.value().getId(), sig2.value());
        REQUIRE(result2.is_ok());
        CHECK(result2.value()); // Quorum reached!

        block.addValidatorSignature(leader2_key.value().getId(), "leader_beta", sig2.value());
        CHECK(consensus.isProposalReady(proposal_id));

        // === Phase 5: Verify finalized signatures ===
        auto finalized = consensus.getFinalizedSignatures(proposal_id);
        REQUIRE(finalized.is_ok());
        CHECK(finalized.value().size() == 2);

        // === Phase 6: Persist to storage ===
        FileStore store;
        auto open_result = store.open(String(test_dir.c_str()));
        REQUIRE(open_result.is_ok());
        store.initializeCoreSchema();

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

        auto tx = store.beginTransaction();
        tx->commit();

        // === Phase 7: Verify persistence ===
        auto all = store.loadAllValidators();
        CHECK(all.size() == 3);

        // Block has all required signatures
        CHECK(block.countValidSignatures() >= 2);
        CHECK(block.getProposer() == "leader_alpha");

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Validator failure and recovery scenario") {
        std::filesystem::path test_dir = "/tmp/blockit_test_e2e_failure";
        std::filesystem::remove_all(test_dir);

        PoAConfig config;
        config.initial_required_signatures = 2;
        config.minimum_required_signatures = 1;

        PoAConsensus consensus(config);

        auto leader1_key = Key::generate();
        auto leader2_key = Key::generate();
        REQUIRE(leader1_key.is_ok());
        REQUIRE(leader2_key.is_ok());

        consensus.addValidator("leader_alpha", leader1_key.value());
        consensus.addValidator("leader_beta", leader2_key.value());

        // Initially need 2 signatures
        CHECK(consensus.getRequiredSignatures() == 2);

        // Leader Beta goes offline
        consensus.markOffline(leader2_key.value().getId());
        CHECK(consensus.getActiveValidatorCount() == 1);

        // With only 1 active validator, threshold reduces
        CHECK(consensus.getRequiredSignatures() == 1);

        // Create and sign block with single validator
        auto privateKey = std::make_shared<Crypto>("swarm_key");
        Transaction<SwarmTask> tx("emergency_task",
                                  SwarmTask{"emergency", "return_to_base", "all", "urgent", 1.0}, 100);
        tx.signTransaction(privateKey);

        Block<SwarmTask> block({tx});
        block.setProposer("leader_alpha");

        auto hash = block.calculateHash();
        REQUIRE(hash.is_ok());
        std::vector<uint8_t> hash_bytes(hash.value().begin(), hash.value().end());

        auto proposal_id = consensus.createProposal(hash.value(), leader1_key.value().getId());

        auto sig = leader1_key.value().sign(hash_bytes);
        REQUIRE(sig.is_ok());

        auto result = consensus.addSignature(proposal_id, leader1_key.value().getId(), sig.value());
        REQUIRE(result.is_ok());
        CHECK(result.value()); // Single signature is enough

        block.addValidatorSignature(leader1_key.value().getId(), "leader_alpha", sig.value());
        CHECK(consensus.isProposalReady(proposal_id));

        // Leader Beta comes back online
        consensus.markOnline(leader2_key.value().getId());
        CHECK(consensus.getActiveValidatorCount() == 2);
        CHECK(consensus.getRequiredSignatures() == 2);

        std::filesystem::remove_all(test_dir);
    }

    TEST_CASE("Validator revocation workflow") {
        PoAConfig config;
        config.initial_required_signatures = 2;
        config.minimum_required_signatures = 1;

        PoAConsensus consensus(config);

        auto leader1_key = Key::generate();
        auto leader2_key = Key::generate();
        auto compromised_key = Key::generate();
        REQUIRE(leader1_key.is_ok());
        REQUIRE(leader2_key.is_ok());
        REQUIRE(compromised_key.is_ok());

        consensus.addValidator("leader_alpha", leader1_key.value());
        consensus.addValidator("leader_beta", leader2_key.value());
        consensus.addValidator("compromised", compromised_key.value());

        CHECK(consensus.getActiveValidatorCount() == 3);

        // Revoke compromised validator
        consensus.revokeValidator(compromised_key.value().getId());

        // Check status
        auto validator = consensus.getValidator(compromised_key.value().getId());
        REQUIRE(validator.is_ok());
        CHECK(validator.value()->getStatus() == ValidatorStatus::REVOKED);
        CHECK_FALSE(validator.value()->canSign());

        // Active count reduced
        CHECK(consensus.getActiveValidatorCount() == 2);

        // Cannot sign with revoked key
        auto privateKey = std::make_shared<Crypto>("swarm_key");
        Transaction<SwarmTask> tx("task_001", SwarmTask{"task", "explore", "robot_1", "pending", 0.5}, 100);
        tx.signTransaction(privateKey);

        Block<SwarmTask> block({tx});
        auto hash = block.calculateHash();
        REQUIRE(hash.is_ok());

        auto proposal_id = consensus.createProposal(hash.value(), leader1_key.value().getId());

        // Try to add signature from revoked validator - should fail
        std::vector<uint8_t> hash_bytes(hash.value().begin(), hash.value().end());
        auto bad_sig = compromised_key.value().sign(hash_bytes);
        REQUIRE(bad_sig.is_ok());

        auto bad_result = consensus.addSignature(proposal_id, compromised_key.value().getId(), bad_sig.value());
        CHECK_FALSE(bad_result.is_ok()); // Should be rejected

        // Good validators can still sign
        auto good_sig = leader1_key.value().sign(hash_bytes);
        REQUIRE(good_sig.is_ok());
        auto good_result = consensus.addSignature(proposal_id, leader1_key.value().getId(), good_sig.value());
        CHECK(good_result.is_ok());
    }

    TEST_CASE("Rate limiting prevents spam") {
        PoAConfig config;
        config.max_proposals_per_hour = 3;
        config.min_seconds_between_proposals = 0; // Disable time-based limiting for test

        PoAConsensus consensus(config);

        auto key = Key::generate();
        REQUIRE(key.is_ok());

        consensus.addValidator("spammer", key.value());

        // First few proposals should succeed
        for (int i = 0; i < 3; i++) {
            auto can_propose = consensus.canPropose(key.value().getId());
            CHECK(can_propose.is_ok());
            consensus.recordProposal(key.value().getId());
        }

        // 4th proposal should be rate limited
        auto can_propose = consensus.canPropose(key.value().getId());
        CHECK_FALSE(can_propose.is_ok());
    }
}
