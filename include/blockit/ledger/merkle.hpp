#pragma once

#include <keylock/keylock.hpp>
#include <string>
#include <vector>

#include <blockit/common/error.hpp>

namespace blockit::ledger {

    class MerkleTree {
      private:
        std::vector<std::string> leaves_;
        std::vector<std::vector<std::string>> tree_levels_;
        std::string root_hash_;

        inline dp::Result<std::string, dp::Error> hashData(const std::string &data) const {
            keylock::keylock crypto(keylock::Algorithm::XChaCha20_Poly1305, keylock::HashAlgorithm::SHA256);
            std::vector<dp::u8> data_vec(data.begin(), data.end());
            auto hash_result = crypto.hash(data_vec);
            if (hash_result.success)
                return dp::Result<std::string, dp::Error>::ok(keylock::keylock::to_hex(hash_result.data));
            return dp::Result<std::string, dp::Error>::err(hash_failed("Failed to hash data"));
        }

        inline dp::Result<std::string, dp::Error> combineHashes(const std::string &left,
                                                                const std::string &right) const {
            return hashData(left + right);
        }

      public:
        MerkleTree() = default;

        inline explicit MerkleTree(const std::vector<std::string> &transaction_strings) {
            if (transaction_strings.empty()) {
                root_hash_ = "";
                return;
            }
            leaves_ = transaction_strings;
            auto result = buildTree();
            // Ignore build errors in constructor, root_hash_ will be empty on failure
            (void)result;
        }

        inline dp::Result<void, dp::Error> buildTree() {
            if (leaves_.empty()) {
                root_hash_ = "";
                return dp::Result<void, dp::Error>::ok();
            }
            tree_levels_.clear();
            std::vector<std::string> current_level;
            for (const auto &leaf : leaves_) {
                auto hash_result = hashData(leaf);
                if (!hash_result.is_ok()) {
                    return dp::Result<void, dp::Error>::err(hash_result.error());
                }
                current_level.push_back(hash_result.value());
            }
            tree_levels_.push_back(current_level);
            while (current_level.size() > 1) {
                std::vector<std::string> next_level;
                for (size_t i = 0; i < current_level.size(); i += 2) {
                    dp::Result<std::string, dp::Error> combined;
                    if (i + 1 < current_level.size())
                        combined = combineHashes(current_level[i], current_level[i + 1]);
                    else
                        combined = combineHashes(current_level[i], current_level[i]);
                    if (!combined.is_ok()) {
                        return dp::Result<void, dp::Error>::err(combined.error());
                    }
                    next_level.push_back(combined.value());
                }
                tree_levels_.push_back(next_level);
                current_level = next_level;
            }
            root_hash_ = current_level.empty() ? "" : current_level[0];
            return dp::Result<void, dp::Error>::ok();
        }

        inline dp::Result<std::string, dp::Error> getRoot() const {
            if (leaves_.empty()) {
                return dp::Result<std::string, dp::Error>::err(merkle_empty("Merkle tree is empty"));
            }
            return dp::Result<std::string, dp::Error>::ok(root_hash_);
        }

        // Non-Result version for backward compatibility
        inline std::string getRootUnsafe() const { return root_hash_; }

        inline bool isEmpty() const { return leaves_.empty(); }

        inline dp::Result<std::vector<std::string>, dp::Error> getProof(size_t transaction_index) const {
            std::vector<std::string> proof;
            if (transaction_index >= leaves_.size()) {
                return dp::Result<std::vector<std::string>, dp::Error>::err(
                    dp::Error::out_of_range("Transaction index out of range"));
            }
            if (tree_levels_.empty()) {
                return dp::Result<std::vector<std::string>, dp::Error>::err(merkle_empty("Merkle tree not built"));
            }
            size_t current_index = transaction_index;
            for (size_t level = 0; level < tree_levels_.size() - 1; level++) {
                const auto &current_level_nodes = tree_levels_[level];
                size_t sibling_index = (current_index % 2 == 0) ? current_index + 1 : current_index - 1;
                if (sibling_index < current_level_nodes.size())
                    proof.push_back(current_level_nodes[sibling_index]);
                else
                    proof.push_back(current_level_nodes[current_index]);
                current_index = current_index / 2;
            }
            return dp::Result<std::vector<std::string>, dp::Error>::ok(std::move(proof));
        }

        inline dp::Result<bool, dp::Error> verifyProof(const std::string &transaction_data, size_t transaction_index,
                                                       const std::vector<std::string> &proof) const {
            if (proof.empty() && leaves_.size() == 1) {
                auto hash_result = hashData(transaction_data);
                if (!hash_result.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(hash_result.error());
                }
                return dp::Result<bool, dp::Error>::ok(hash_result.value() == root_hash_);
            }

            auto hash_result = hashData(transaction_data);
            if (!hash_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(hash_result.error());
            }
            std::string current_hash = hash_result.value();
            size_t current_index = transaction_index;

            for (const auto &proof_hash : proof) {
                dp::Result<std::string, dp::Error> combined;
                if (current_index % 2 == 0)
                    combined = combineHashes(current_hash, proof_hash);
                else
                    combined = combineHashes(proof_hash, current_hash);
                if (!combined.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(combined.error());
                }
                current_hash = combined.value();
                current_index = current_index / 2;
            }
            return dp::Result<bool, dp::Error>::ok(current_hash == root_hash_);
        }

        inline dp::Result<bool, dp::Error> verifyProof(const std::string &transaction_data,
                                                       const std::vector<std::string> &proof,
                                                       const std::string &expected_root) const {
            // Find the index of the transaction
            size_t index = 0;
            bool found = false;
            for (size_t i = 0; i < leaves_.size(); ++i) {
                if (leaves_[i] == transaction_data) {
                    index = i;
                    found = true;
                    break;
                }
            }

            // If not found in leaves, still try to verify with index 0
            if (!found && !proof.empty()) {
                auto hash_result = hashData(transaction_data);
                if (!hash_result.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(hash_result.error());
                }
                std::string current_hash = hash_result.value();
                size_t current_index = 0;
                for (const auto &proof_hash : proof) {
                    dp::Result<std::string, dp::Error> combined;
                    if (current_index % 2 == 0)
                        combined = combineHashes(current_hash, proof_hash);
                    else
                        combined = combineHashes(proof_hash, current_hash);
                    if (!combined.is_ok()) {
                        return dp::Result<bool, dp::Error>::err(combined.error());
                    }
                    current_hash = combined.value();
                    current_index = current_index / 2;
                }
                return dp::Result<bool, dp::Error>::ok(current_hash == expected_root);
            }

            // If found, verify using the index
            if (found) {
                auto verify_result = verifyProof(transaction_data, index, proof);
                if (!verify_result.is_ok()) {
                    return verify_result;
                }
                return dp::Result<bool, dp::Error>::ok(verify_result.value() && root_hash_ == expected_root);
            }

            // Single element tree case
            if (proof.empty() && leaves_.size() == 1) {
                auto hash_result = hashData(transaction_data);
                if (!hash_result.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(hash_result.error());
                }
                return dp::Result<bool, dp::Error>::ok(hash_result.value() == expected_root);
            }

            return dp::Result<bool, dp::Error>::ok(false);
        }

        inline size_t getTransactionCount() const { return leaves_.size(); }

        inline dp::Result<std::vector<std::string>, dp::Error>
        generateProof(const std::string &transaction_data) const {
            // Find the index of the transaction
            for (size_t i = 0; i < leaves_.size(); ++i) {
                if (leaves_[i] == transaction_data) {
                    return getProof(i);
                }
            }
            // Return error if transaction not found
            return dp::Result<std::vector<std::string>, dp::Error>::err(
                dp::Error::not_found("Transaction not found in tree"));
        }
    };

} // namespace blockit::ledger
