#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <memory>

struct ChainTestData {
    std::string action;
    std::string issuer;

    std::string to_string() const { return "ChainTestData{" + action + "," + issuer + "}"; }
};

TEST_SUITE("Chain Tests") {
    TEST_CASE("Chain creation with genesis block") {
        auto privateKey = std::make_shared<chain::Crypto>("chain_test_key");

        chain::Chain<ChainTestData> blockchain("test-chain", "genesis-tx", ChainTestData{"init", "system"}, privateKey);

        CHECK(std::string(blockchain.uuid_.c_str()) == "test-chain");
        CHECK(blockchain.blocks_.size() == 1);
        CHECK(blockchain.blocks_[0].index_ == 0);
        CHECK(std::string(blockchain.blocks_[0].previous_hash_.c_str()) == "GENESIS");
        CHECK_FALSE(blockchain.blocks_[0].hash_.empty());
    }

    TEST_CASE("Adding blocks to chain") {
        auto privateKey = std::make_shared<chain::Crypto>("add_block_key");

        chain::Chain<ChainTestData> blockchain("test-chain", "genesis", ChainTestData{"start", "system"}, privateKey);

        // Create and add a new block
        chain::Transaction<ChainTestData> tx("tx-001", ChainTestData{"action1", "user1"}, 100);
        tx.signTransaction(privateKey);

        chain::Block<ChainTestData> newBlock({tx});
        blockchain.addBlock(newBlock);

        CHECK(blockchain.blocks_.size() == 2);
        CHECK(blockchain.blocks_[1].index_ == 1);
        CHECK(std::string(blockchain.blocks_[1].previous_hash_.c_str()) == std::string(blockchain.blocks_[0].hash_.c_str()));
    }

    TEST_CASE("Chain validation") {
        auto privateKey = std::make_shared<chain::Crypto>("validation_chain_key");

        chain::Chain<ChainTestData> blockchain("valid-chain", "genesis", ChainTestData{"init", "sys"}, privateKey);

        // Add multiple blocks
        for (int i = 1; i <= 3; i++) {
            chain::Transaction<ChainTestData> tx("tx-" + std::to_string(i),
                                                 ChainTestData{"action" + std::to_string(i), "user"}, 100);
            tx.signTransaction(privateKey);
            chain::Block<ChainTestData> block({tx});
            blockchain.addBlock(block);
        }

        auto valid_result = blockchain.isValid();
        CHECK(valid_result.is_ok());
        CHECK(valid_result.value());
        CHECK(blockchain.blocks_.size() == 4); // Genesis + 3 added blocks
    }

    TEST_CASE("Participant management in chain") {
        auto privateKey = std::make_shared<chain::Crypto>("participant_key");

        chain::Chain<ChainTestData> blockchain("participant-chain", "genesis", ChainTestData{"init", "system"},
                                               privateKey);

        // Register participants
        blockchain.registerParticipant("robot-001", "active", {{"type", "industrial"}});
        blockchain.registerParticipant("sensor-001", "monitoring");

        // Check authorization
        CHECK(blockchain.isParticipantAuthorized("robot-001"));
        CHECK(blockchain.isParticipantAuthorized("sensor-001"));
        CHECK_FALSE(blockchain.isParticipantAuthorized("unknown-device"));

        // Grant capabilities
        blockchain.grantCapability("robot-001", "MOVE");
        blockchain.grantCapability("robot-001", "PICK");
        blockchain.grantCapability("sensor-001", "READ_DATA");

        // Update states
        CHECK(blockchain.updateParticipantState("robot-001", "working").is_ok());
        CHECK(blockchain.updateParticipantState("sensor-001", "active").is_ok());

        // Check metadata
        auto meta_result = blockchain.getParticipantMetadata("robot-001", "type");
        CHECK(meta_result.is_ok());
        CHECK(meta_result.value() == "industrial");
    }

    TEST_CASE("Double-spend prevention in chain") {
        auto privateKey = std::make_shared<chain::Crypto>("double_spend_key");

        chain::Chain<ChainTestData> blockchain("anti-double-spend", "genesis", ChainTestData{"init", "system"},
                                               privateKey);

        blockchain.registerParticipant("user-001", "active");

        // First transaction should succeed
        chain::Transaction<ChainTestData> tx1("tx-unique", ChainTestData{"transfer", "user-001"}, 100);
        tx1.signTransaction(privateKey);
        chain::Block<ChainTestData> block1({tx1});
        blockchain.addBlock(block1);

        CHECK(blockchain.blocks_.size() == 2); // Genesis + new block

        // Second transaction with same ID should be rejected
        chain::Transaction<ChainTestData> tx2("tx-unique", ChainTestData{"another_transfer", "user-001"}, 100);
        tx2.signTransaction(privateKey);
        chain::Block<ChainTestData> block2({tx2});

        size_t blocks_before = blockchain.blocks_.size();
        blockchain.addBlock(block2); // Should fail

        CHECK(blockchain.blocks_.size() == blocks_before); // No new block added
    }

    TEST_CASE("Chain integrity with hash linking") {
        auto privateKey = std::make_shared<chain::Crypto>("integrity_key");

        chain::Chain<ChainTestData> blockchain("integrity-chain", "genesis", ChainTestData{"start", "system"},
                                               privateKey);

        std::string previousHash(blockchain.blocks_[0].hash_.c_str());

        // Add blocks and verify hash linking
        for (int i = 1; i <= 5; i++) {
            chain::Transaction<ChainTestData> tx("tx-" + std::to_string(i),
                                                 ChainTestData{"action" + std::to_string(i), "user"}, 100);
            tx.signTransaction(privateKey);
            chain::Block<ChainTestData> block({tx});
            blockchain.addBlock(block);

            // Verify current block links to previous
            CHECK(std::string(blockchain.blocks_[i].previous_hash_.c_str()) == previousHash);
            CHECK(blockchain.blocks_[i].index_ == i);

            previousHash = std::string(blockchain.blocks_[i].hash_.c_str());
        }

        auto valid_result = blockchain.isValid();
        CHECK(valid_result.is_ok());
        CHECK(valid_result.value());
    }

    TEST_CASE("Chain with unauthorized participant actions") {
        auto privateKey = std::make_shared<chain::Crypto>("unauthorized_key");

        chain::Chain<ChainTestData> blockchain("auth-chain", "genesis", ChainTestData{"init", "system"}, privateKey);

        // Don't register any participants

        // Try to add transaction from unauthorized participant
        chain::Transaction<ChainTestData> tx("tx-unauthorized", ChainTestData{"illegal_action", "hacker"}, 100);
        tx.signTransaction(privateKey);
        chain::Block<ChainTestData> block({tx});

        // Block should still be added (authorization check is separate from blockchain mechanics)
        blockchain.addBlock(block);
        CHECK(blockchain.blocks_.size() == 2);

        // But the action validation should fail
        CHECK_FALSE(blockchain.validateAndRecordAction("hacker", "some_action", "action-001").is_ok());
    }

    TEST_CASE("Chain action validation with capabilities") {
        auto privateKey = std::make_shared<chain::Crypto>("capability_key");

        chain::Chain<ChainTestData> blockchain("capability-chain", "genesis", ChainTestData{"init", "system"},
                                               privateKey);

        blockchain.registerParticipant("worker-001", "ready");
        blockchain.grantCapability("worker-001", "OPERATE_MACHINE");
        blockchain.grantCapability("worker-001", "QUALITY_CHECK");

        // Valid action with required capability
        CHECK(blockchain.validateAndRecordAction("worker-001", "operate drilling machine", "action-001",
                                                 "OPERATE_MACHINE").is_ok());

        // Valid action without capability requirement
        CHECK(blockchain.validateAndRecordAction("worker-001", "status update", "action-002").is_ok());

        // Invalid action - missing capability
        CHECK_FALSE(
            blockchain.validateAndRecordAction("worker-001", "emergency shutdown", "action-003", "EMERGENCY_STOP").is_ok());

        // Invalid action - duplicate ID
        CHECK_FALSE(
            blockchain.validateAndRecordAction("worker-001", "another operation", "action-001", "OPERATE_MACHINE").is_ok());
    }
}
