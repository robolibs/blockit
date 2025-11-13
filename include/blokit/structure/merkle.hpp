#pragma once

#include <iostream>
#include <lockey/lockey.hpp>
#include <string>
#include <vector>

namespace chain {

    class MerkleTree {
      private:
        std::vector<std::string> leaves_;
        std::vector<std::vector<std::string>> tree_levels_;
        std::string root_hash_;

        // Hash function using Lockey
        inline std::string hashData(const std::string &data) const {
            lockey::Lockey crypto(lockey::Lockey::Algorithm::XChaCha20_Poly1305, lockey::Lockey::HashAlgorithm::SHA256);
            std::vector<uint8_t> data_vec(data.begin(), data.end());
            auto hash_result = crypto.hash(data_vec);

            if (hash_result.success) {
                return lockey::Lockey::to_hex(hash_result.data);
            } else {
                return "";
            }
        }

        // Combine two hashes
        inline std::string combineHashes(const std::string &left, const std::string &right) const {
            return hashData(left + right);
        }

      public:
        MerkleTree() = default;

        // Construct Merkle tree from transaction strings
        inline explicit MerkleTree(const std::vector<std::string> &transaction_strings) {
            if (transaction_strings.empty()) {
                root_hash_ = "";
                return;
            }

            leaves_ = transaction_strings;
            buildTree();
        }

        // Build the Merkle tree
        inline void buildTree() {
            if (leaves_.empty()) {
                root_hash_ = "";
                return;
            }

            tree_levels_.clear();

            // Start with leaf level (hash each transaction)
            std::vector<std::string> current_level;
            for (const auto &leaf : leaves_) {
                current_level.push_back(hashData(leaf));
            }
            tree_levels_.push_back(current_level);

            // Build tree levels up to root
            while (current_level.size() > 1) {
                std::vector<std::string> next_level;

                for (size_t i = 0; i < current_level.size(); i += 2) {
                    if (i + 1 < current_level.size()) {
                        // Pair exists
                        next_level.push_back(combineHashes(current_level[i], current_level[i + 1]));
                    } else {
                        // Odd number, hash with itself
                        next_level.push_back(combineHashes(current_level[i], current_level[i]));
                    }
                }

                tree_levels_.push_back(next_level);
                current_level = next_level;
            }

            // Root is the single element in the final level
            root_hash_ = current_level.empty() ? "" : current_level[0];
        }

        // Get the Merkle root
        inline std::string getRoot() const { return root_hash_; }

        // Get proof for a specific transaction (simplified version)
        inline std::vector<std::string> getProof(size_t transaction_index) const {
            std::vector<std::string> proof;

            if (transaction_index >= leaves_.size() || tree_levels_.empty()) {
                return proof;
            }

            size_t current_index = transaction_index;

            // Traverse up the tree levels
            for (size_t level = 0; level < tree_levels_.size() - 1; level++) {
                const auto &current_level_nodes = tree_levels_[level];

                // Find sibling
                size_t sibling_index;
                if (current_index % 2 == 0) {
                    // Current is left child, sibling is right
                    sibling_index = current_index + 1;
                } else {
                    // Current is right child, sibling is left
                    sibling_index = current_index - 1;
                }

                // Add sibling to proof if it exists
                if (sibling_index < current_level_nodes.size()) {
                    proof.push_back(current_level_nodes[sibling_index]);
                } else {
                    // If no sibling, use the same node (for odd numbers)
                    proof.push_back(current_level_nodes[current_index]);
                }

                // Move to parent index
                current_index = current_index / 2;
            }

            return proof;
        }

        // Get proof for a specific transaction by data
        inline std::vector<std::string> generateProof(const std::string &transaction_data) const {
            // Find the index of the transaction
            for (size_t i = 0; i < leaves_.size(); i++) {
                if (leaves_[i] == transaction_data) {
                    return getProof(i);
                }
            }
            return {}; // Return empty proof if transaction not found
        }

        // Verify a transaction is in the tree using proof
        inline bool verifyProof(const std::string &transaction_data, size_t transaction_index,
                                const std::vector<std::string> &proof) const {
            if (proof.empty() && leaves_.size() == 1) {
                // Single transaction case
                return hashData(transaction_data) == root_hash_;
            }

            std::string current_hash = hashData(transaction_data);
            size_t current_index = transaction_index;

            // Traverse proof
            for (const auto &proof_hash : proof) {
                if (current_index % 2 == 0) {
                    // Current is left child
                    current_hash = combineHashes(current_hash, proof_hash);
                } else {
                    // Current is right child
                    current_hash = combineHashes(proof_hash, current_hash);
                }
                current_index = current_index / 2;
            }

            return current_hash == root_hash_;
        }

        // Verify proof using transaction data and root
        inline bool verifyProof(const std::string &transaction_data, const std::vector<std::string> &proof,
                                const std::string &expected_root) const {
            // Find the index of the transaction
            for (size_t i = 0; i < leaves_.size(); i++) {
                if (leaves_[i] == transaction_data) {
                    return verifyProof(transaction_data, i, proof) && (root_hash_ == expected_root);
                }
            }
            return false;
        }

        // Print tree structure for debugging
        inline void printTree() const {
            std::cout << "=== Merkle Tree Structure ===" << std::endl;
            std::cout << "Leaves (" << leaves_.size() << "):" << std::endl;
            for (size_t i = 0; i < leaves_.size(); i++) {
                std::cout << "  [" << i << "] " << leaves_[i] << std::endl;
            }

            std::cout << "\nTree Levels:" << std::endl;
            for (size_t level = 0; level < tree_levels_.size(); level++) {
                std::cout << "Level " << level << " (" << tree_levels_[level].size() << " nodes):" << std::endl;
                for (size_t i = 0; i < tree_levels_[level].size(); i++) {
                    std::cout << "  " << tree_levels_[level][i].substr(0, 16) << "..." << std::endl;
                }
            }

            std::cout << "\nMerkle Root: " << root_hash_.substr(0, 32) << "..." << std::endl;
        }

        // Get number of transactions
        inline size_t getTransactionCount() const { return leaves_.size(); }

        // Check if tree is empty
        inline bool isEmpty() const { return leaves_.empty(); }
    };

} // namespace chain
