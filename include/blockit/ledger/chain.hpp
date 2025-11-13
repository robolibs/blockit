#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "auth.hpp"
#include "block.hpp"

namespace blockit::ledger {

    using namespace std::chrono;

    template <typename T> class Chain {
      public:
        std::string uuid_;
        Timestamp timestamp_;
        std::vector<Block<T>> blocks_;
        Authenticator entity_manager_;

        Chain() = default;
        inline Chain(std::string s_uuid, std::string t_uuid, T function, std::shared_ptr<Crypto> privateKey_,
                     int16_t priority = 100) {
            Transaction<T> genesisTransaction(t_uuid, function, priority);
            genesisTransaction.signTransaction(privateKey_);
            Block<T> genesisBlock({genesisTransaction});
            blocks_.push_back(genesisBlock);
            uuid_ = std::move(s_uuid);
        }
        inline Chain(std::string s_uuid, std::string t_uuid, T function, int16_t priority = 100) {
            Transaction<T> genesisTransaction(t_uuid, function, priority);
            Block<T> genesisBlock({genesisTransaction});
            blocks_.push_back(genesisBlock);
            uuid_ = std::move(s_uuid);
        }

        inline bool addBlock(const Block<T> &newBlock) {
            Block<T> blockToAdd = newBlock;
            blockToAdd.previous_hash_ = blocks_.back().hash_;
            blockToAdd.index_ = blocks_.back().index_ + 1;
            for (const auto &txn : blockToAdd.transactions_) {
                if (entity_manager_.isTransactionUsed(txn.uuid_)) {
                    std::cout << "Duplicate transaction detected: " << txn.uuid_ << std::endl;
                    return false;
                }
            }
            blockToAdd.buildMerkleTree();
            blockToAdd.hash_ = blockToAdd.calculateHash();
            if (!blockToAdd.isValid()) {
                std::cout << "Invalid block attempted to be added to the blockchain" << std::endl;
                return false;
            }
            for (const auto &txn : blockToAdd.transactions_)
                entity_manager_.markTransactionUsed(txn.uuid_);
            blocks_.push_back(blockToAdd);
            return true;
        }

        inline bool addBlock(std::string uuid, T function, std::shared_ptr<Crypto> privateKey_,
                             int16_t priority = 100) {
            Transaction<T> genesisTransaction(uuid, function, priority);
            genesisTransaction.signTransaction(privateKey_);
            Block<T> genesisBlock({genesisTransaction});
            return addBlock(genesisBlock);
        }

        inline bool isValid() const {
            if (blocks_.empty())
                return false;
            if (blocks_.size() == 1)
                return blocks_[0].isValid();
            for (size_t i = 1; i < blocks_.size(); i++) {
                const Block<T> &currentBlock_ = blocks_[i];
                const Block<T> &previousBlock = blocks_[i - 1];
                if (!currentBlock_.isValid())
                    return false;
                if (currentBlock_.previous_hash_ != previousBlock.hash_)
                    return false;
            }
            return true;
        }

        inline void registerEntity(const std::string &entity_id, const std::string &initial_state = "inactive") {
            entity_manager_.registerParticipant(entity_id, initial_state);
        }
        inline void registerParticipant(const std::string &participant_id,
                                        const std::string &initial_state = "inactive",
                                        const std::unordered_map<std::string, std::string> &metadata = {}) {
            entity_manager_.registerParticipant(participant_id, initial_state, metadata);
        }
        inline bool isEntityAuthorized(const std::string &entity_id) const {
            return entity_manager_.isParticipantAuthorized(entity_id);
        }
        inline bool isParticipantAuthorized(const std::string &participant_id) const {
            return entity_manager_.isParticipantAuthorized(participant_id);
        }
        inline bool updateEntityState(const std::string &entity_id, const std::string &new_state) {
            return entity_manager_.updateParticipantState(entity_id, new_state);
        }
        inline bool updateParticipantState(const std::string &participant_id, const std::string &new_state) {
            return entity_manager_.updateParticipantState(participant_id, new_state);
        }
        inline void grantPermission(const std::string &entity_id, const std::string &permission) {
            entity_manager_.grantCapability(entity_id, permission);
        }
        inline void grantCapability(const std::string &participant_id, const std::string &capability) {
            entity_manager_.grantCapability(participant_id, capability);
        }
        inline std::string getParticipantMetadata(const std::string &participant_id, const std::string &key) const {
            return entity_manager_.getParticipantMetadata(participant_id, key);
        }
        inline void setParticipantMetadata(const std::string &participant_id, const std::string &key,
                                           const std::string &value) {
            entity_manager_.setParticipantMetadata(participant_id, key, value);
        }
        inline bool executeCommand(const std::string &issuer_entity, const std::string &command,
                                   const std::string &tx_id, const std::string &required_permission = "") {
            return entity_manager_.validateAndRecordAction(issuer_entity, command, tx_id, required_permission);
        }
        inline bool validateAndRecordAction(const std::string &issuer_participant,
                                            const std::string &action_description, const std::string &tx_id,
                                            const std::string &required_capability = "") {
            return entity_manager_.validateAndRecordAction(issuer_participant, action_description, tx_id,
                                                           required_capability);
        }
        inline void printChainSummary() const {
            std::cout << "=== Blockchain Summary ===\n";
            std::cout << "Chain UUID: " << uuid_ << "\n";
            std::cout << "Total Blocks: " << blocks_.size() << "\n";
            std::cout << "Chain Valid: " << (isValid() ? "YES" : "NO") << "\n";
            size_t total_transactions = 0;
            for (const auto &block : blocks_)
                total_transactions += block.transactions_.size();
            std::cout << "Total Transactions: " << total_transactions << "\n";
            if (!blocks_.empty()) {
                std::cout << "Genesis Block Hash: " << blocks_[0].hash_.substr(0, 16) << "...\n";
                std::cout << "Latest Block Hash: " << blocks_.back().hash_.substr(0, 16) << "...\n";
            }
            std::cout << "\nAuthenticator:\n";
            entity_manager_.printSystemSummary();
        }
        inline bool isParticipantRegistered(const std::string &participant_id) const {
            return entity_manager_.isParticipantAuthorized(participant_id);
        }
        inline bool canParticipantPerform(const std::string &participant_id, const std::string &capability) const {
            return entity_manager_.hasCapability(participant_id, capability);
        }
        inline void revokeCapability(const std::string &participant_id, const std::string &capability) {
            entity_manager_.revokeCapability(participant_id, capability);
        }
        inline bool isChainValid() const { return isValid(); }
        inline size_t getChainLength() const { return blocks_.size(); }
        inline const Block<T> &getLastBlock() const {
            if (blocks_.empty())
                throw std::runtime_error("Chain is empty");
            return blocks_.back();
        }
        inline bool isTransactionUsed(const std::string &tx_id) const {
            return entity_manager_.isTransactionUsed(tx_id);
        }

        inline std::string serialize() const {
            std::stringstream ss;
            ss << R"({)";
            ss << R"("uuid": ")" << uuid_ << R"(",)";
            ss << R"("timestamp": )" << timestamp_.serialize() << R"(,)";
            ss << R"("blocks": [)";
            for (size_t i = 0; i < blocks_.size(); ++i) {
                ss << blocks_[i].serialize();
                if (i < blocks_.size() - 1)
                    ss << ",";
            }
            ss << R"(],)";
            ss << R"("entity_manager": )" << entity_manager_.serialize();
            ss << R"(})";
            return ss.str();
        }
        inline static Chain<T> deserialize(const std::string &data) {
            Chain<T> result;
            size_t uuid_start = data.find("\"uuid\": \"") + 9;
            size_t uuid_end = data.find("\"", uuid_start);
            result.uuid_ = data.substr(uuid_start, uuid_end - uuid_start);
            size_t ts_start = data.find("\"timestamp\": ") + 13;
            size_t ts_end = data.find("},", ts_start) + 1;
            result.timestamp_ = Timestamp::deserialize(data.substr(ts_start, ts_end - ts_start));
            size_t blocks_array_start = data.find("\"blocks\": [") + 11;
            size_t blocks_array_end = data.find("],", blocks_array_start);
            std::string blocks_array_data = data.substr(blocks_array_start, blocks_array_end - blocks_array_start);
            int brace_count = 0;
            size_t block_start = 0;
            for (size_t i = 0; i < blocks_array_data.length(); ++i) {
                if (blocks_array_data[i] == '{') {
                    if (brace_count == 0)
                        block_start = i;
                    brace_count++;
                } else if (blocks_array_data[i] == '}') {
                    brace_count--;
                    if (brace_count == 0) {
                        std::string block_data = blocks_array_data.substr(block_start, i - block_start + 1);
                        result.blocks_.push_back(Block<T>::deserialize(block_data));
                    }
                }
            }
            size_t entity_start = data.find("\"entity_manager\": ") + 18;
            size_t entity_end = data.rfind("}");
            std::string entity_data = data.substr(entity_start, entity_end - entity_start);
            result.entity_manager_ = Authenticator::deserialize(entity_data);
            return result;
        }

        inline bool saveToFile(const std::string &filename) const {
            try {
                std::ofstream file(filename);
                if (!file.is_open())
                    return false;
                file << serialize();
                file.close();
                return true;
            } catch (...) {
                return false;
            }
        }
        inline bool loadFromFile(const std::string &filename) {
            try {
                std::ifstream file(filename);
                if (!file.is_open())
                    return false;
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();
                *this = Chain<T>::deserialize(buffer.str());
                return true;
            } catch (...) {
                return false;
            }
        }
    };

} // namespace blockit::ledger
