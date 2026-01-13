#include "blockit/blockit.hpp"
#include <doctest/doctest.h>

using namespace blockit;

// Test data structure
struct RobotState {
    std::string robot_id;
    double x;
    double y;
    std::string status;

    std::string to_string() const {
        return "RobotState{" + robot_id + "," + std::to_string(x) + "," + std::to_string(y) + "," + status + "}";
    }

    auto members() { return std::tie(robot_id, x, y, status); }
    auto members() const { return std::tie(robot_id, x, y, status); }
};

TEST_SUITE("Chain PoA Integration Tests") {
    TEST_CASE("Add block with validator signatures") {
        // Create PoA consensus
        PoAConfig config;
        config.initial_required_signatures = 2;
        PoAConsensus consensus(config);

        // Generate validator keys
        auto alice_key = Key::generate();
        auto bob_key = Key::generate();
        auto charlie_key = Key::generate();
        REQUIRE(alice_key.is_ok());
        REQUIRE(bob_key.is_ok());
        REQUIRE(charlie_key.is_ok());

        // Add validators
        consensus.addValidator("alice", alice_key.value());
        consensus.addValidator("bob", bob_key.value());
        consensus.addValidator("charlie", charlie_key.value());

        CHECK(consensus.getActiveValidatorCount() == 3);
        CHECK(consensus.getRequiredSignatures() == 2);

        // Create a transaction
        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<RobotState> tx("tx_001", RobotState{"robot_1", 10.0, 20.0, "active"}, 100);
        tx.signTransaction(privateKey);

        // Create a block
        Block<RobotState> block({tx});
        block.setProposer("alice");

        // Create proposal
        auto block_hash = block.calculateHash();
        REQUIRE(block_hash.is_ok());

        auto proposal_id = consensus.createProposal(block_hash.value(), alice_key.value().getId());

        // Alice signs
        std::vector<uint8_t> hash_bytes(block_hash.value().begin(), block_hash.value().end());
        auto alice_sig = alice_key.value().sign(hash_bytes);
        REQUIRE(alice_sig.is_ok());

        auto add_alice = consensus.addSignature(proposal_id, alice_key.value().getId(), alice_sig.value());
        REQUIRE(add_alice.is_ok());
        CHECK_FALSE(add_alice.value()); // Not quorum yet

        // Add signature to block
        block.addValidatorSignature(alice_key.value().getId(), "alice", alice_sig.value());

        // Bob signs
        auto bob_sig = bob_key.value().sign(hash_bytes);
        REQUIRE(bob_sig.is_ok());

        auto add_bob = consensus.addSignature(proposal_id, bob_key.value().getId(), bob_sig.value());
        REQUIRE(add_bob.is_ok());
        CHECK(add_bob.value()); // Quorum reached!

        // Add signature to block
        block.addValidatorSignature(bob_key.value().getId(), "bob", bob_sig.value());

        // Verify block has required signatures
        CHECK(block.countValidSignatures() >= static_cast<size_t>(consensus.getRequiredSignatures()));
        CHECK(consensus.isProposalReady(proposal_id));
    }

    TEST_CASE("Chain with multiple PoA blocks") {
        PoAConfig config;
        config.initial_required_signatures = 2;
        PoAConsensus consensus(config);

        auto alice_key = Key::generate();
        auto bob_key = Key::generate();
        REQUIRE(alice_key.is_ok());
        REQUIRE(bob_key.is_ok());

        consensus.addValidator("alice", alice_key.value());
        consensus.addValidator("bob", bob_key.value());

        std::vector<Block<RobotState>> blocks;
        auto privateKey = std::make_shared<Crypto>("test_key");

        // Create multiple blocks
        for (int i = 0; i < 3; i++) {
            Transaction<RobotState> tx("tx_" + std::to_string(i),
                                       RobotState{"robot_1", static_cast<double>(i * 10), static_cast<double>(i * 20),
                                                  "active"},
                                       100);
            tx.signTransaction(privateKey);

            Block<RobotState> block({tx});
            block.setProposer("alice");

            // Sign with both validators
            auto hash = block.calculateHash();
            REQUIRE(hash.is_ok());
            std::vector<uint8_t> hash_bytes(hash.value().begin(), hash.value().end());

            auto alice_sig = alice_key.value().sign(hash_bytes);
            auto bob_sig = bob_key.value().sign(hash_bytes);
            REQUIRE(alice_sig.is_ok());
            REQUIRE(bob_sig.is_ok());

            block.addValidatorSignature(alice_key.value().getId(), "alice", alice_sig.value());
            block.addValidatorSignature(bob_key.value().getId(), "bob", bob_sig.value());

            blocks.push_back(block);
        }

        CHECK(blocks.size() == 3);
        for (const auto &block : blocks) {
            CHECK(block.countValidSignatures() == 2);
            CHECK(block.getProposer() == "alice");
        }
    }

    TEST_CASE("Offline validator reduces quorum requirement") {
        PoAConfig config;
        config.initial_required_signatures = 2;
        config.minimum_required_signatures = 1;
        PoAConsensus consensus(config);

        auto alice_key = Key::generate();
        auto bob_key = Key::generate();
        REQUIRE(alice_key.is_ok());
        REQUIRE(bob_key.is_ok());

        consensus.addValidator("alice", alice_key.value());
        consensus.addValidator("bob", bob_key.value());

        CHECK(consensus.getRequiredSignatures() == 2);

        // Mark Bob offline
        consensus.markOffline(bob_key.value().getId());

        // With only 1 active validator, requirement should reduce
        CHECK(consensus.getRequiredSignatures() == 1);

        // Single validator can now approve blocks
        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<RobotState> tx("tx_001", RobotState{"robot_1", 10.0, 20.0, "active"}, 100);
        tx.signTransaction(privateKey);

        Block<RobotState> block({tx});

        auto hash = block.calculateHash();
        REQUIRE(hash.is_ok());
        std::vector<uint8_t> hash_bytes(hash.value().begin(), hash.value().end());

        auto proposal_id = consensus.createProposal(hash.value(), alice_key.value().getId());

        auto alice_sig = alice_key.value().sign(hash_bytes);
        REQUIRE(alice_sig.is_ok());

        auto result = consensus.addSignature(proposal_id, alice_key.value().getId(), alice_sig.value());
        REQUIRE(result.is_ok());
        CHECK(result.value()); // Quorum reached with 1 signature
    }

    TEST_CASE("Revoked validator cannot sign") {
        PoAConfig config;
        PoAConsensus consensus(config);

        auto alice_key = Key::generate();
        REQUIRE(alice_key.is_ok());

        consensus.addValidator("alice", alice_key.value());
        CHECK(consensus.getActiveValidatorCount() == 1);

        // Revoke validator
        consensus.revokeValidator(alice_key.value().getId());

        // Now no active validators
        CHECK(consensus.getActiveValidatorCount() == 0);

        // Get validator and check status
        auto validator = consensus.getValidator(alice_key.value().getId());
        REQUIRE(validator.is_ok());
        CHECK(validator.value()->getStatus() == ValidatorStatus::REVOKED);
        CHECK_FALSE(validator.value()->canSign());
    }
}
