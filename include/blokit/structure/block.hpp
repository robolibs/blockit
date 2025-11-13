#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <lockey/lockey.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "merkle.hpp"
#include "transaction.hpp"

using namespace std::chrono;

namespace chain {
    template <typename T> class Block {
      public:
        int64_t index_;
        std::string previous_hash_;
        std::string hash_;
        std::vector<Transaction<T>> transactions_;
        int64_t nonce_;
        Timestamp timestamp_;
        std::string merkle_root_; // Merkle root for transaction integrity

        Block() = default;
        inline Block(std::vector<Transaction<T>> txns) {
            index_ = 0;
            previous_hash_ = "GENESIS";
            transactions_ = txns;
            nonce_ = 0;
            timestamp_.sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            timestamp_.nanosec =
                duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count() % 1000000000;

            // Build Merkle tree for transactions
            buildMerkleTree();
            hash_ = calculateHash();
        }

        // Build Merkle tree from transactions
        inline void buildMerkleTree() {
            std::vector<std::string> tx_strings;
            for (const auto &txn : transactions_) {
                tx_strings.push_back(txn.toString());
            }
            MerkleTree temp_tree(tx_strings);
            merkle_root_ = temp_tree.getRoot();
        }

        // Method to calculate the hash of the block
        inline std::string calculateHash() const {
            std::stringstream ss;
            ss << index_ << timestamp_.sec << timestamp_.nanosec << previous_hash_ << nonce_;
            // Use Merkle root instead of iterating through all transactions
            ss << merkle_root_;

            // Use lockey hash function and convert to hex
            lockey::Lockey crypto(lockey::Lockey::Algorithm::XChaCha20_Poly1305, lockey::Lockey::HashAlgorithm::SHA256);
            std::string data = ss.str();
            std::vector<uint8_t> data_vec(data.begin(), data.end());
            auto hash_result = crypto.hash(data_vec);

            if (hash_result.success) {
                return lockey::Lockey::to_hex(hash_result.data);
            } else {
                return ""; // Return empty string on hash failure
            }
        }

        inline bool isValid() const {
            // Basic field validation
            if (index_ < 0 || previous_hash_.empty() || hash_.empty()) {
                std::cout << "Basic validation failed - Index: " << index_ << " Hash: " << hash_
                          << " Previous hash: " << previous_hash_ << std::endl;
                return false;
            }

            // Verify block hash is correct
            if (hash_ != calculateHash()) {
                std::cout << "Block hash validation failed - stored vs calculated hash mismatch" << std::endl;
                return false;
            }

            // Verify Merkle root
            std::vector<std::string> tx_strings;
            for (const auto &txn : transactions_) {
                tx_strings.push_back(txn.toString());
            }
            MerkleTree verification_tree(tx_strings);
            if (merkle_root_ != verification_tree.getRoot()) {
                std::cout << "Merkle root validation failed" << std::endl;
                return false;
            }

            // Validate all transactions
            for (const auto &txn : transactions_) {
                if (!txn.isValid()) {
                    std::cout << "Transaction validation failed for: " << txn.uuid_ << std::endl;
                    return false;
                }
            }

            return true;
        }

        // Verify a specific transaction is in this block using Merkle proof
        inline bool verifyTransaction(size_t transaction_index) const {
            if (transaction_index >= transactions_.size()) {
                return false;
            }

            // Create temporary Merkle tree for verification
            std::vector<std::string> tx_strings;
            for (const auto &txn : transactions_) {
                tx_strings.push_back(txn.toString());
            }
            MerkleTree verification_tree(tx_strings);

            // Get proof and verify
            auto proof = verification_tree.getProof(transaction_index);
            return verification_tree.verifyProof(transactions_[transaction_index].toString(), transaction_index, proof);
        }

        // Get block summary for debugging
        inline void printBlockSummary() const {
            std::cout << "=== Block Summary ===" << std::endl;
            std::cout << "Index: " << index_ << std::endl;
            std::cout << "Transactions: " << transactions_.size() << std::endl;
            std::cout << "Merkle Root: " << merkle_root_.substr(0, 16) << "..." << std::endl;
            std::cout << "Block Hash: " << hash_.substr(0, 16) << "..." << std::endl;
            std::cout << "Previous Hash: " << previous_hash_.substr(0, 16) << "..." << std::endl;
            std::cout << "Timestamp: " << timestamp_.sec << "." << timestamp_.nanosec << std::endl;
            std::cout << "Is Valid: " << (isValid() ? "YES" : "NO") << std::endl;
        }

        // Binary serialization methods for unified system
        inline std::vector<uint8_t> serializeBinary() const {
            std::vector<uint8_t> buffer;

            // Write index
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(index_));

            // Write previous hash
            BinarySerializer::writeString(buffer, previous_hash_);

            // Write hash
            BinarySerializer::writeString(buffer, hash_);

            // Write nonce
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(nonce_));

            // Write timestamp
            auto timestampData = timestamp_.serializeBinary();
            BinarySerializer::writeBytes(buffer, timestampData);

            // Write merkle root
            BinarySerializer::writeString(buffer, merkle_root_);

            // Write transactions count and data
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(transactions_.size()));
            for (const auto &tx : transactions_) {
                auto txData = tx.serializeBinary();
                BinarySerializer::writeBytes(buffer, txData);
            }

            return buffer;
        }

        static Block<T> deserializeBinary(const std::vector<uint8_t> &data) {
            Block<T> result;
            size_t offset = 0;

            // Read index
            result.index_ = static_cast<int64_t>(BinarySerializer::readUint32(data, offset));

            // Read previous hash
            result.previous_hash_ = BinarySerializer::readString(data, offset);

            // Read hash
            result.hash_ = BinarySerializer::readString(data, offset);

            // Read nonce
            result.nonce_ = static_cast<int64_t>(BinarySerializer::readUint32(data, offset));

            // Read timestamp
            auto timestampData = BinarySerializer::readBytes(data, offset);
            result.timestamp_ = Timestamp::deserializeBinary(timestampData);

            // Read merkle root
            result.merkle_root_ = BinarySerializer::readString(data, offset);

            // Read transactions
            uint32_t txCount = BinarySerializer::readUint32(data, offset);
            for (uint32_t i = 0; i < txCount; i++) {
                auto txData = BinarySerializer::readBytes(data, offset);
                result.transactions_.push_back(Transaction<T>::deserializeBinary(txData));
            }

            return result;
        }

        // JSON serialization methods (maintain backward compatibility)
        inline std::string serializeJson() const {
            std::stringstream ss;
            ss << R"({)";
            ss << R"("index": )" << index_ << R"(,)";
            ss << R"("previous_hash": ")" << JsonSerializer::escapeJson(previous_hash_) << R"(",)";
            ss << R"("hash": ")" << JsonSerializer::escapeJson(hash_) << R"(",)";
            ss << R"("nonce": )" << nonce_ << R"(,)";
            ss << R"("timestamp": )" << timestamp_.serialize() << R"(,)";
            ss << R"("merkle_root": ")" << JsonSerializer::escapeJson(merkle_root_) << R"(",)";
            ss << R"("transactions": [)";

            for (size_t i = 0; i < transactions_.size(); ++i) {
                ss << transactions_[i].serialize();
                if (i < transactions_.size() - 1) {
                    ss << ",";
                }
            }

            ss << R"(])";
            ss << R"(})";

            return ss.str();
        }

        // Serialization methods
        inline std::string serialize() const { return serializeJson(); }

        inline static Block<T> deserialize(const std::string &data) {
            Block<T> result;

            // Parse index
            size_t index_start = data.find("\"index\": ") + 9;
            size_t index_end = data.find(",", index_start);
            result.index_ = std::stoll(data.substr(index_start, index_end - index_start));

            // Parse previous_hash
            size_t prev_hash_start = data.find("\"previous_hash\": \"") + 18;
            size_t prev_hash_end = data.find("\"", prev_hash_start);
            result.previous_hash_ = data.substr(prev_hash_start, prev_hash_end - prev_hash_start);

            // Parse hash
            size_t hash_start = data.find("\"hash\": \"") + 9;
            size_t hash_end = data.find("\"", hash_start);
            result.hash_ = data.substr(hash_start, hash_end - hash_start);

            // Parse nonce
            size_t nonce_start = data.find("\"nonce\": ") + 9;
            size_t nonce_end = data.find(",", nonce_start);
            result.nonce_ = std::stoll(data.substr(nonce_start, nonce_end - nonce_start));

            // Parse timestamp
            size_t ts_start = data.find("\"timestamp\": ") + 13;
            size_t ts_end = data.find("},", ts_start) + 1;
            result.timestamp_ = Timestamp::deserialize(data.substr(ts_start, ts_end - ts_start));

            // Parse merkle_root
            size_t merkle_start = data.find("\"merkle_root\": \"") + 16;
            size_t merkle_end = data.find("\"", merkle_start);
            result.merkle_root_ = data.substr(merkle_start, merkle_end - merkle_start);

            // Parse transactions array
            size_t tx_array_start = data.find("\"transactions\": [") + 17;
            size_t tx_array_end = data.rfind("]");
            std::string tx_array_data = data.substr(tx_array_start, tx_array_end - tx_array_start);

            // Parse individual transactions
            int brace_count = 0;
            size_t tx_start = 0;

            for (size_t i = 0; i < tx_array_data.length(); ++i) {
                if (tx_array_data[i] == '{') {
                    if (brace_count == 0)
                        tx_start = i;
                    brace_count++;
                } else if (tx_array_data[i] == '}') {
                    brace_count--;
                    if (brace_count == 0) {
                        std::string tx_data = tx_array_data.substr(tx_start, i - tx_start + 1);
                        result.transactions_.push_back(Transaction<T>::deserialize(tx_data));
                    }
                }
            }

            return result;
        }

      private:
        // Helper function to get the current timestamp
        inline std::string getCurrentTime() const {
            std::time_t now = std::time(nullptr);
            char buf[80];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            return std::string(buf);
        }
    };
} // namespace chain
