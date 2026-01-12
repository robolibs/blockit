#pragma once

#include <chrono>
#include <keylock/keylock.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "merkle.hpp"
#include "transaction.hpp"

namespace blockit::ledger {

    using namespace std::chrono;

    template <typename T> class Block {
      public:
        int64_t index_{};
        std::string previous_hash_{};
        std::string hash_{};
        std::vector<Transaction<T>> transactions_{};
        int64_t nonce_{};
        Timestamp timestamp_{};
        std::string merkle_root_{};

        Block() = default;
        inline Block(std::vector<Transaction<T>> txns) {
            index_ = 0;
            previous_hash_ = "GENESIS";
            transactions_ = std::move(txns);
            nonce_ = 0;
            timestamp_.sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
            timestamp_.nanosec =
                duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count() % 1000000000;
            buildMerkleTree();
            hash_ = calculateHash();
        }

        inline void buildMerkleTree() {
            std::vector<std::string> tx_strings;
            tx_strings.reserve(transactions_.size());
            for (const auto &txn : transactions_)
                tx_strings.push_back(txn.toString());
            MerkleTree temp_tree(tx_strings);
            merkle_root_ = temp_tree.getRoot();
        }

        inline std::string calculateHash() const {
            std::stringstream ss;
            ss << index_ << timestamp_.sec << timestamp_.nanosec << previous_hash_ << nonce_;
            ss << merkle_root_;
            keylock::keylock crypto(keylock::Algorithm::XChaCha20_Poly1305, keylock::HashAlgorithm::SHA256);
            std::string data = ss.str();
            std::vector<uint8_t> data_vec(data.begin(), data.end());
            auto hash_result = crypto.hash(data_vec);
            if (hash_result.success)
                return keylock::keylock::to_hex(hash_result.data);
            else
                return "";
        }

        inline bool isValid() const {
            if (index_ < 0 || previous_hash_.empty() || hash_.empty())
                return false;
            if (hash_ != calculateHash())
                return false;
            std::vector<std::string> tx_strings;
            for (const auto &txn : transactions_)
                tx_strings.push_back(txn.toString());
            MerkleTree verification_tree(tx_strings);
            if (merkle_root_ != verification_tree.getRoot())
                return false;
            for (const auto &txn : transactions_) {
                if (!txn.isValid())
                    return false;
            }
            return true;
        }

        inline bool verifyTransaction(size_t transaction_index) const {
            if (transaction_index >= transactions_.size())
                return false;
            std::vector<std::string> tx_strings;
            for (const auto &txn : transactions_)
                tx_strings.push_back(txn.toString());
            MerkleTree verification_tree(tx_strings);
            auto proof = verification_tree.getProof(transaction_index);
            return verification_tree.verifyProof(transactions_[transaction_index].toString(), transaction_index, proof);
        }

        // Serialize block to string (alias for serializeJson)
        inline std::string serialize() const { return serializeJson(); }

        // Serialize block to JSON string
        inline std::string serializeJson() const {
            std::stringstream ss;
            ss << R"({)";
            ss << R"("index": )" << index_ << R"(,)";
            ss << R"("previous_hash": ")" << previous_hash_ << R"(",)";
            ss << R"("hash": ")" << hash_ << R"(",)";
            ss << R"("nonce": )" << nonce_ << R"(,)";
            ss << R"("timestamp": {"sec": )" << timestamp_.sec << R"(, "nanosec": )" << timestamp_.nanosec << R"(},)";
            ss << R"("merkle_root": ")" << merkle_root_ << R"(",)";
            ss << R"("transactions": [)";
            for (size_t i = 0; i < transactions_.size(); ++i) {
                ss << transactions_[i].serialize();
                if (i < transactions_.size() - 1)
                    ss << ",";
            }
            ss << R"(])";
            ss << R"(})";
            return ss.str();
        }

        // Serialize block to binary format
        inline std::vector<uint8_t> serializeBinary() const {
            std::vector<uint8_t> buffer;

            // Write block header (int64_t as two uint32)
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(index_ & 0xFFFFFFFF));
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>((index_ >> 32) & 0xFFFFFFFF));

            BinarySerializer::writeString(buffer, previous_hash_);
            BinarySerializer::writeString(buffer, hash_);

            // Write nonce (int64_t as two uint32)
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(nonce_ & 0xFFFFFFFF));
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>((nonce_ >> 32) & 0xFFFFFFFF));

            // Write timestamp (already has correct methods)
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(timestamp_.sec));
            BinarySerializer::writeUint32(buffer, timestamp_.nanosec);

            // Write merkle root
            BinarySerializer::writeString(buffer, merkle_root_);

            // Write transaction count and transactions
            BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(transactions_.size()));
            for (const auto &tx : transactions_) {
                auto txData = tx.serializeBinary();
                BinarySerializer::writeUint32(buffer, static_cast<uint32_t>(txData.size()));
                buffer.insert(buffer.end(), txData.begin(), txData.end());
            }

            return buffer;
        }

        // Deserialize block from binary format
        inline static Block<T> deserializeBinary(const std::vector<uint8_t> &data) {
            Block<T> result;
            size_t offset = 0;

            // Read block header (int64_t from two uint32)
            uint32_t index_low = BinarySerializer::readUint32(data, offset);
            uint32_t index_high = BinarySerializer::readUint32(data, offset);
            result.index_ = static_cast<int64_t>(index_low) | (static_cast<int64_t>(index_high) << 32);

            result.previous_hash_ = BinarySerializer::readString(data, offset);
            result.hash_ = BinarySerializer::readString(data, offset);

            // Read nonce (int64_t from two uint32)
            uint32_t nonce_low = BinarySerializer::readUint32(data, offset);
            uint32_t nonce_high = BinarySerializer::readUint32(data, offset);
            result.nonce_ = static_cast<int64_t>(nonce_low) | (static_cast<int64_t>(nonce_high) << 32);

            // Read timestamp
            result.timestamp_.sec = static_cast<int32_t>(BinarySerializer::readUint32(data, offset));
            result.timestamp_.nanosec = BinarySerializer::readUint32(data, offset);

            // Read merkle root
            result.merkle_root_ = BinarySerializer::readString(data, offset);

            // Read transaction count and transactions
            uint32_t tx_count = BinarySerializer::readUint32(data, offset);
            for (uint32_t i = 0; i < tx_count; ++i) {
                uint32_t tx_size = BinarySerializer::readUint32(data, offset);
                std::vector<uint8_t> tx_data(data.begin() + offset, data.begin() + offset + tx_size);
                offset += tx_size;
                result.transactions_.push_back(Transaction<T>::deserializeBinary(tx_data));
            }

            return result;
        }

        // Deserialize block from string
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
            size_t ts_sec_start = data.find("\"sec\": ") + 7;
            size_t ts_sec_end = data.find(",", ts_sec_start);
            result.timestamp_.sec = std::stoll(data.substr(ts_sec_start, ts_sec_end - ts_sec_start));

            size_t ts_nano_start = data.find("\"nanosec\": ") + 11;
            size_t ts_nano_end = data.find("}", ts_nano_start);
            result.timestamp_.nanosec = std::stoll(data.substr(ts_nano_start, ts_nano_end - ts_nano_start));

            // Parse merkle_root
            size_t merkle_start = data.find("\"merkle_root\": \"") + 16;
            size_t merkle_end = data.find("\"", merkle_start);
            result.merkle_root_ = data.substr(merkle_start, merkle_end - merkle_start);

            // Parse transactions array
            size_t tx_array_start = data.find("\"transactions\": [") + 17;
            size_t tx_array_end = data.find("]", tx_array_start);
            std::string tx_array_data = data.substr(tx_array_start, tx_array_end - tx_array_start);

            if (!tx_array_data.empty()) {
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
            }

            return result;
        }
    };

} // namespace blockit::ledger
