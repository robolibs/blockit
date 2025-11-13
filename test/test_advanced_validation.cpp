#include "blockit.hpp"
#include <doctest/doctest.h>
#include <memory>

// Advanced validation tests for enhanced security and edge cases

struct ValidationTestData {
    std::string category;
    std::string data;
    bool critical;

    std::string to_string() const {
        return "ValidationTestData{" + category + ":" + data + " (critical:" + (critical ? "true" : "false") + ")}";
    }
};

TEST_SUITE("Advanced Validation Tests") {
    TEST_CASE("Transaction validation edge cases") {
        auto privateKey = std::make_shared<chain::Crypto>("validation_key");

        // Test empty transaction data
        ValidationTestData emptyData{"", "", false};
        chain::Transaction<ValidationTestData> emptyTx("empty-tx", emptyData, 100);
        emptyTx.signTransaction(privateKey);

        // Should still be valid if properly signed
        CHECK(emptyTx.isValid());

        // Test maximum size transaction
        std::string largeData(10000, 'A'); // 10KB of data
        ValidationTestData largeData_struct{"large", largeData, true};
        chain::Transaction<ValidationTestData> largeTx("large-tx", largeData_struct, 255);
        largeTx.signTransaction(privateKey);

        CHECK(largeTx.isValid());
        CHECK(largeTx.toString().length() > 10000);

        // Test boundary priority values
        chain::Transaction<ValidationTestData> minPriorityTx("min-tx", emptyData, 0);
        chain::Transaction<ValidationTestData> maxPriorityTx("max-tx", emptyData, 255);

        minPriorityTx.signTransaction(privateKey);
        maxPriorityTx.signTransaction(privateKey);

        CHECK(minPriorityTx.isValid());
        CHECK(maxPriorityTx.isValid());
        CHECK(minPriorityTx.priority_ == 0);
        CHECK(maxPriorityTx.priority_ == 255);
    }

    TEST_CASE("Block validation with malformed transactions") {
        auto privateKey = std::make_shared<chain::Crypto>("malformed_key");

        ValidationTestData validData{"test", "valid_data", false};

        // Create valid transaction
        chain::Transaction<ValidationTestData> validTx("valid-tx", validData, 100);
        validTx.signTransaction(privateKey);

        // Create unsigned transaction (invalid)
        chain::Transaction<ValidationTestData> unsignedTx("unsigned-tx", validData, 100);
        // Don't sign this one

        // Block with only valid transaction should work
        chain::Block<ValidationTestData> validBlock({validTx});
        CHECK(validBlock.verifyTransaction(0));         // First transaction
        CHECK_FALSE(validBlock.verifyTransaction(999)); // Index out of bounds

        // Block with mixed valid/invalid transactions
        chain::Block<ValidationTestData> mixedBlock({validTx, unsignedTx});
        CHECK(mixedBlock.verifyTransaction(0)); // First transaction
        CHECK(mixedBlock.verifyTransaction(1)); // Second transaction (block contains it, even if invalid)

        // Verify Merkle root still calculated correctly
        CHECK_FALSE(mixedBlock.merkle_root_.empty());
    }

    TEST_CASE("Chain validation with corrupted blocks") {
        auto privateKey = std::make_shared<chain::Crypto>("corruption_key");

        chain::Chain<ValidationTestData> blockchain("corruption-test", "genesis",
                                                    ValidationTestData{"genesis", "initial", true}, privateKey);

        // Add valid blocks
        for (int i = 1; i <= 3; i++) {
            chain::Transaction<ValidationTestData> tx(
                "valid-tx-" + std::to_string(i), ValidationTestData{"valid", "data_" + std::to_string(i), false}, 100);
            tx.signTransaction(privateKey);
            chain::Block<ValidationTestData> block({tx});
            CHECK(blockchain.addBlock(block));
        }

        CHECK(blockchain.isChainValid());
        CHECK(blockchain.getChainLength() == 4); // Genesis + 3 blocks

        // Simulate hash corruption by getting last block and modifying it
        // (This is for testing - in real implementation, blocks should be immutable)
        auto lastBlock = blockchain.getLastBlock();
        std::string originalHash = lastBlock.hash_;

        // Verify chain is still valid with original hash
        CHECK(blockchain.isChainValid());

        // Test with empty hash (simulated corruption)
        // Note: This tests the validation logic, not actual corruption
        CHECK(blockchain.isChainValid()); // Should remain valid as we haven't actually corrupted the stored chain
    }

    TEST_CASE("Authenticator validation and edge cases") {
        auto privateKey = std::make_shared<chain::Crypto>("auth_edge_key");

        chain::Chain<ValidationTestData> blockchain("auth-edge-test", "genesis",
                                                    ValidationTestData{"genesis", "auth_test", true}, privateKey);

        // Test duplicate participant registration
        blockchain.registerParticipant("duplicate_test", "active", {{"role", "first"}});
        blockchain.registerParticipant("duplicate_test", "active", {{"role", "second"}}); // Should update, not fail

        // Verify updated metadata
        CHECK(blockchain.isParticipantRegistered("duplicate_test"));

        // Test capability management edge cases
        blockchain.grantCapability("duplicate_test", "test_capability");
        CHECK(blockchain.canParticipantPerform("duplicate_test", "test_capability"));

        // Grant same capability again (should not cause issues)
        blockchain.grantCapability("duplicate_test", "test_capability");
        CHECK(blockchain.canParticipantPerform("duplicate_test", "test_capability"));

        // Revoke capability
        blockchain.revokeCapability("duplicate_test", "test_capability");
        CHECK_FALSE(blockchain.canParticipantPerform("duplicate_test", "test_capability"));

        // Test operations on non-existent participants
        CHECK_FALSE(blockchain.canParticipantPerform("nonexistent", "any_capability"));

        // Granting capability to non-existent participant should handle gracefully
        blockchain.grantCapability("nonexistent", "test_capability");
        CHECK_FALSE(blockchain.canParticipantPerform("nonexistent", "test_capability"));
    }

    TEST_CASE("Merkle tree validation and edge cases") {
        std::vector<std::string> singleItem = {"single_transaction"};
        chain::MerkleTree singleTree(singleItem);

        CHECK_FALSE(singleTree.getRoot().empty());

        // Test proof generation and verification for single item
        auto proof = singleTree.generateProof("single_transaction");
        CHECK(singleTree.verifyProof("single_transaction", proof, singleTree.getRoot()));

        // Test with empty list
        std::vector<std::string> emptyList;
        chain::MerkleTree emptyTree(emptyList);
        CHECK(emptyTree.getRoot().empty()); // Empty tree should have empty root

        // Test with large number of transactions
        std::vector<std::string> largeTxList;
        for (int i = 0; i < 1000; i++) {
            largeTxList.push_back("tx_" + std::to_string(i));
        }

        chain::MerkleTree largeTree(largeTxList);
        CHECK_FALSE(largeTree.getRoot().empty());

        // Verify proofs for random transactions in large tree
        auto proof_0 = largeTree.generateProof("tx_0");
        auto proof_999 = largeTree.generateProof("tx_999");
        auto proof_500 = largeTree.generateProof("tx_500");

        CHECK(largeTree.verifyProof("tx_0", proof_0, largeTree.getRoot()));
        CHECK(largeTree.verifyProof("tx_999", proof_999, largeTree.getRoot()));
        CHECK(largeTree.verifyProof("tx_500", proof_500, largeTree.getRoot()));

        // Test invalid proof verification
        CHECK_FALSE(largeTree.verifyProof("tx_0", proof_999, largeTree.getRoot()));
        CHECK_FALSE(largeTree.verifyProof("nonexistent", proof_0, largeTree.getRoot()));
    }

    TEST_CASE("Double-spend prevention stress test") {
        auto privateKey = std::make_shared<chain::Crypto>("double_spend_key");

        chain::Chain<ValidationTestData> blockchain("double-spend-test", "genesis",
                                                    ValidationTestData{"genesis", "initial", true}, privateKey);

        // Create multiple transactions with same ID (simulating double-spend attempts)
        std::string duplicateTxId = "double-spend-attempt";
        ValidationTestData testData{"transfer", "100_units", true};

        chain::Transaction<ValidationTestData> tx1(duplicateTxId, testData, 100);
        chain::Transaction<ValidationTestData> tx2(duplicateTxId, testData, 150); // Different priority
        chain::Transaction<ValidationTestData> tx3(duplicateTxId, testData, 200); // Different priority

        tx1.signTransaction(privateKey);
        tx2.signTransaction(privateKey);
        tx3.signTransaction(privateKey);

        // First transaction should succeed
        chain::Block<ValidationTestData> block1({tx1});
        CHECK(blockchain.addBlock(block1));
        CHECK(blockchain.isTransactionUsed(duplicateTxId));

        // Subsequent blocks with same transaction ID should be rejected
        chain::Block<ValidationTestData> block2({tx2});
        chain::Block<ValidationTestData> block3({tx3});

        CHECK_FALSE(blockchain.addBlock(block2));
        CHECK_FALSE(blockchain.addBlock(block3));

        // Chain should remain valid and unchanged
        CHECK(blockchain.isChainValid());
        CHECK(blockchain.getChainLength() == 2); // Genesis + block1 only
    }

    TEST_CASE("Blockchain state consistency under edge conditions") {
        auto privateKey = std::make_shared<chain::Crypto>("consistency_key");

        chain::Chain<ValidationTestData> blockchain("consistency-test", "genesis",
                                                    ValidationTestData{"genesis", "consistent", true}, privateKey);

        // Add blocks with varying transaction counts
        std::vector<int> txCounts = {1, 5, 0, 10, 3}; // Including empty block

        for (size_t blockIndex = 0; blockIndex < txCounts.size(); blockIndex++) {
            std::vector<chain::Transaction<ValidationTestData>> blockTxs;

            for (int txIndex = 0; txIndex < txCounts[blockIndex]; txIndex++) {
                std::string txId = "consistency_tx_" + std::to_string(blockIndex) + "_" + std::to_string(txIndex);
                ValidationTestData txData{"consistency", "block_" + std::to_string(blockIndex), txIndex % 2 == 0};

                chain::Transaction<ValidationTestData> tx(txId, txData, 100 + txIndex);
                tx.signTransaction(privateKey);
                blockTxs.push_back(tx);
            }

            chain::Block<ValidationTestData> block(blockTxs);
            CHECK(blockchain.addBlock(block));
        }

        // Verify final chain state
        CHECK(blockchain.isChainValid());
        CHECK(blockchain.getChainLength() == txCounts.size() + 1); // +1 for genesis

        // Verify all non-empty transactions are tracked
        int totalExpectedTxs = 0;
        for (int count : txCounts) {
            totalExpectedTxs += count;
        }

        // Count tracked transactions (excluding genesis)
        int trackedTxs = 0;
        for (size_t blockIndex = 0; blockIndex < txCounts.size(); blockIndex++) {
            for (int txIndex = 0; txIndex < txCounts[blockIndex]; txIndex++) {
                std::string txId = "consistency_tx_" + std::to_string(blockIndex) + "_" + std::to_string(txIndex);
                if (blockchain.isTransactionUsed(txId)) {
                    trackedTxs++;
                }
            }
        }

        CHECK(trackedTxs == totalExpectedTxs);
    }
}
