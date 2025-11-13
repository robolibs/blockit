#include <blockit/ledger/merkle.hpp>
#include <lockey/lockey.hpp>

namespace blockit::ledger {

    std::string MerkleTree::hashData(const std::string &data) const {
        lockey::Lockey crypto(lockey::Lockey::Algorithm::XChaCha20_Poly1305, lockey::Lockey::HashAlgorithm::SHA256);
        std::vector<uint8_t> data_vec(data.begin(), data.end());
        auto hash_result = crypto.hash(data_vec);
        if (hash_result.success)
            return lockey::Lockey::to_hex(hash_result.data);
        return "";
    }

    std::string MerkleTree::combineHashes(const std::string &left, const std::string &right) const {
        return hashData(left + right);
    }

    MerkleTree::MerkleTree(const std::vector<std::string> &transaction_strings) {
        if (transaction_strings.empty()) {
            root_hash_ = "";
            return;
        }
        leaves_ = transaction_strings;
        buildTree();
    }

    void MerkleTree::buildTree() {
        if (leaves_.empty()) {
            root_hash_ = "";
            return;
        }
        tree_levels_.clear();
        std::vector<std::string> current_level;
        for (const auto &leaf : leaves_)
            current_level.push_back(hashData(leaf));
        tree_levels_.push_back(current_level);
        while (current_level.size() > 1) {
            std::vector<std::string> next_level;
            for (size_t i = 0; i < current_level.size(); i += 2) {
                if (i + 1 < current_level.size())
                    next_level.push_back(combineHashes(current_level[i], current_level[i + 1]));
                else
                    next_level.push_back(combineHashes(current_level[i], current_level[i]));
            }
            tree_levels_.push_back(next_level);
            current_level = next_level;
        }
        root_hash_ = current_level.empty() ? "" : current_level[0];
    }

    std::string MerkleTree::getRoot() const { return root_hash_; }

    bool MerkleTree::isEmpty() const { return leaves_.empty(); }

    std::vector<std::string> MerkleTree::getProof(size_t transaction_index) const {
        std::vector<std::string> proof;
        if (transaction_index >= leaves_.size() || tree_levels_.empty())
            return proof;
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
        return proof;
    }

    bool MerkleTree::verifyProof(const std::string &transaction_data, size_t transaction_index,
                                 const std::vector<std::string> &proof) const {
        if (proof.empty() && leaves_.size() == 1)
            return hashData(transaction_data) == root_hash_;
        std::string current_hash = hashData(transaction_data);
        size_t current_index = transaction_index;
        for (const auto &proof_hash : proof) {
            if (current_index % 2 == 0)
                current_hash = combineHashes(current_hash, proof_hash);
            else
                current_hash = combineHashes(proof_hash, current_hash);
            current_index = current_index / 2;
        }
        return current_hash == root_hash_;
    }

    bool MerkleTree::verifyProof(const std::string &transaction_data, const std::vector<std::string> &proof,
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
        // This handles the case where we're verifying external data
        if (!found && !proof.empty()) {
            // Reconstruct root from proof assuming index 0
            std::string current_hash = hashData(transaction_data);
            size_t current_index = 0;
            for (const auto &proof_hash : proof) {
                if (current_index % 2 == 0)
                    current_hash = combineHashes(current_hash, proof_hash);
                else
                    current_hash = combineHashes(proof_hash, current_hash);
                current_index = current_index / 2;
            }
            return current_hash == expected_root;
        }

        // If found, verify using the index
        if (found) {
            return verifyProof(transaction_data, index, proof) && root_hash_ == expected_root;
        }

        // Single element tree case
        if (proof.empty() && leaves_.size() == 1) {
            return hashData(transaction_data) == expected_root;
        }

        return false;
    }

    size_t MerkleTree::getTransactionCount() const { return leaves_.size(); }

    std::vector<std::string> MerkleTree::generateProof(const std::string &transaction_data) const {
        // Find the index of the transaction
        for (size_t i = 0; i < leaves_.size(); ++i) {
            if (leaves_[i] == transaction_data) {
                return getProof(i);
            }
        }
        // Return empty proof if transaction not found
        return std::vector<std::string>();
    }

} // namespace blockit::ledger
