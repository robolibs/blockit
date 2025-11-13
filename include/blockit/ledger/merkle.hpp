#pragma once

#include <lockey/lockey.hpp>
#include <string>
#include <vector>

namespace blockit::ledger {

    class MerkleTree {
      private:
        std::vector<std::string> leaves_;
        std::vector<std::vector<std::string>> tree_levels_;
        std::string root_hash_;

        std::string hashData(const std::string &data) const;
        std::string combineHashes(const std::string &left, const std::string &right) const;

      public:
        MerkleTree() = default;
        explicit MerkleTree(const std::vector<std::string> &transaction_strings);

        void buildTree();
        std::string getRoot() const;
        bool isEmpty() const;
        std::vector<std::string> getProof(size_t transaction_index) const;
        bool verifyProof(const std::string &transaction_data, size_t transaction_index,
                         const std::vector<std::string> &proof) const;

        // Overloaded version that takes proof and expected root (finds index automatically)
        bool verifyProof(const std::string &transaction_data, const std::vector<std::string> &proof,
                         const std::string &expected_root) const;

        // Add method to get transaction count
        size_t getTransactionCount() const;

        // Add method to generate proof by transaction data (finds index first)
        std::vector<std::string> generateProof(const std::string &transaction_data) const;
    };

} // namespace blockit::ledger
