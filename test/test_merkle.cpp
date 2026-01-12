#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <string>
#include <vector>

TEST_SUITE("Merkle Tree Tests") {
    TEST_CASE("Empty Merkle Tree") {
        std::vector<std::string> empty_transactions;
        chain::MerkleTree tree(empty_transactions);

        CHECK(tree.isEmpty());
        CHECK(tree.getTransactionCount() == 0);
        CHECK(tree.getRoot().empty());
    }

    TEST_CASE("Single transaction Merkle Tree") {
        std::vector<std::string> single_tx = {"transaction_1"};
        chain::MerkleTree tree(single_tx);

        CHECK_FALSE(tree.isEmpty());
        CHECK(tree.getTransactionCount() == 1);
        CHECK_FALSE(tree.getRoot().empty());

        // Verify the transaction
        auto proof = tree.getProof(0);
        bool verified = tree.verifyProof("transaction_1", 0, proof);
        CHECK(verified);
    }

    TEST_CASE("Multiple transactions Merkle Tree") {
        std::vector<std::string> transactions = {"tx_1", "tx_2", "tx_3", "tx_4"};
        chain::MerkleTree tree(transactions);

        CHECK(tree.getTransactionCount() == 4);
        CHECK_FALSE(tree.getRoot().empty());

        // Verify each transaction
        for (size_t i = 0; i < transactions.size(); i++) {
            auto proof = tree.getProof(i);
            bool verified = tree.verifyProof(transactions[i], i, proof);
            CHECK(verified);
        }
    }

    TEST_CASE("Odd number of transactions") {
        std::vector<std::string> transactions = {"tx_1", "tx_2", "tx_3"};
        chain::MerkleTree tree(transactions);

        CHECK(tree.getTransactionCount() == 3);
        CHECK_FALSE(tree.getRoot().empty());

        // Should handle odd number correctly
        for (size_t i = 0; i < transactions.size(); i++) {
            auto proof = tree.getProof(i);
            bool verified = tree.verifyProof(transactions[i], i, proof);
            CHECK(verified);
        }
    }

    TEST_CASE("Large transaction set efficiency") {
        std::vector<std::string> large_set;
        for (int i = 0; i < 1000; i++) {
            large_set.push_back("transaction_" + std::to_string(i));
        }

        chain::MerkleTree tree(large_set);
        CHECK(tree.getTransactionCount() == 1000);

        // Test random transactions
        std::vector<size_t> test_indices = {0, 100, 500, 999};
        for (size_t idx : test_indices) {
            auto proof = tree.getProof(idx);
            bool verified = tree.verifyProof(large_set[idx], idx, proof);
            CHECK(verified);

            // Proof size should be logarithmic (approximately log2(1000) ≈ 10)
            CHECK(proof.size() <= 15); // Allow some margin
        }
    }

    TEST_CASE("Invalid proof should fail verification") {
        std::vector<std::string> transactions = {"tx_1", "tx_2", "tx_3", "tx_4"};
        chain::MerkleTree tree(transactions);

        // Get valid proof for transaction 0
        auto proof = tree.getProof(0);

        // Try to verify wrong transaction with this proof
        bool verified = tree.verifyProof("wrong_transaction", 0, proof);
        CHECK_FALSE(verified);

        // Try to verify correct transaction with wrong index
        verified = tree.verifyProof("tx_1", 1, proof);
        CHECK_FALSE(verified);
    }

    TEST_CASE("Merkle root changes with different transaction sets") {
        std::vector<std::string> set1 = {"tx_1", "tx_2"};
        std::vector<std::string> set2 = {"tx_1", "tx_3"}; // Different second transaction

        chain::MerkleTree tree1(set1);
        chain::MerkleTree tree2(set2);

        CHECK(tree1.getRoot() != tree2.getRoot());
    }

    TEST_CASE("Same transaction set produces same root") {
        std::vector<std::string> transactions = {"tx_a", "tx_b", "tx_c"};

        chain::MerkleTree tree1(transactions);
        chain::MerkleTree tree2(transactions);

        CHECK(tree1.getRoot() == tree2.getRoot());
    }
}
