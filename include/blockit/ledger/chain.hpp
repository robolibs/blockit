#pragma once

#include <chrono>
#include <datapod/datapod.hpp>
#include <fstream>
#include <iostream>
#include <shared_mutex>

#include "auth.hpp"
#include "block.hpp"
#include "poa.hpp"

namespace blockit {

    using namespace std::chrono;

    template <typename T> class Chain {
      public:
        dp::String uuid_{};
        Timestamp timestamp_{};
        dp::Vector<Block<T>> blocks_{};
        Authenticator entity_manager_{};

      private:
        mutable std::shared_mutex mutex_;
        std::unique_ptr<PoAConsensus> poa_; // Optional PoA consensus

      public:
        Chain() = default;

        // Copy constructor
        Chain(const Chain &other) {
            std::shared_lock lock(other.mutex_);
            uuid_ = other.uuid_;
            timestamp_ = other.timestamp_;
            blocks_ = other.blocks_;
            entity_manager_ = other.entity_manager_;
        }

        // Copy assignment
        Chain &operator=(const Chain &other) {
            if (this != &other) {
                std::unique_lock lock1(mutex_, std::defer_lock);
                std::shared_lock lock2(other.mutex_, std::defer_lock);
                std::lock(lock1, lock2);
                uuid_ = other.uuid_;
                timestamp_ = other.timestamp_;
                blocks_ = other.blocks_;
                entity_manager_ = other.entity_manager_;
            }
            return *this;
        }

        // Move constructor
        Chain(Chain &&other) noexcept {
            std::unique_lock lock(other.mutex_);
            uuid_ = std::move(other.uuid_);
            timestamp_ = other.timestamp_;
            blocks_ = std::move(other.blocks_);
            entity_manager_ = std::move(other.entity_manager_);
        }

        // Move assignment
        Chain &operator=(Chain &&other) noexcept {
            if (this != &other) {
                std::unique_lock lock1(mutex_, std::defer_lock);
                std::unique_lock lock2(other.mutex_, std::defer_lock);
                std::lock(lock1, lock2);
                uuid_ = std::move(other.uuid_);
                timestamp_ = other.timestamp_;
                blocks_ = std::move(other.blocks_);
                entity_manager_ = std::move(other.entity_manager_);
            }
            return *this;
        }

        inline Chain(const std::string &s_uuid, const std::string &t_uuid, T function,
                     std::shared_ptr<Crypto> privateKey_, dp::i16 priority = 100) {
            Transaction<T> genesisTransaction(t_uuid, function, priority);
            auto sign_result = genesisTransaction.signTransaction(privateKey_);
            (void)sign_result; // Ignore errors in constructor
            Block<T> genesisBlock({genesisTransaction});
            blocks_.push_back(genesisBlock);
            uuid_ = dp::String(s_uuid.c_str());
        }

        inline Chain(const std::string &s_uuid, const std::string &t_uuid, T function, dp::i16 priority = 100) {
            Transaction<T> genesisTransaction(t_uuid, function, priority);
            Block<T> genesisBlock({genesisTransaction});
            blocks_.push_back(genesisBlock);
            uuid_ = dp::String(s_uuid.c_str());
        }

        auto members() { return std::tie(uuid_, timestamp_, blocks_, entity_manager_); }
        auto members() const { return std::tie(uuid_, timestamp_, blocks_, entity_manager_); }

        inline dp::Result<void, dp::Error> addBlock(const Block<T> &newBlock) {
            std::unique_lock lock(mutex_);

            if (blocks_.empty()) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("Cannot add block to empty chain"));
            }

            Block<T> blockToAdd = newBlock;
            blockToAdd.previous_hash_ = blocks_.back().hash_;
            blockToAdd.index_ = blocks_.back().index_ + 1;

            // Check for duplicate transactions
            for (const auto &txn : blockToAdd.transactions_) {
                if (entity_manager_.isTransactionUsed(std::string(txn.uuid_.c_str()))) {
                    std::string msg = "Duplicate transaction detected: " + std::string(txn.uuid_.c_str());
                    return dp::Result<void, dp::Error>::err(dp::Error::already_exists(dp::String(msg.c_str())));
                }
            }

            blockToAdd.buildMerkleTree();
            auto hash_result = blockToAdd.calculateHash();
            if (!hash_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(hash_result.error());
            }
            blockToAdd.hash_ = dp::String(hash_result.value().c_str());

            auto valid_result = blockToAdd.isValid();
            if (!valid_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(valid_result.error());
            }
            if (!valid_result.value()) {
                return dp::Result<void, dp::Error>::err(
                    dp::Error::invalid_argument("Invalid block attempted to be added"));
            }

            // Mark all transactions as used
            for (const auto &txn : blockToAdd.transactions_) {
                auto mark_result = entity_manager_.markTransactionUsed(std::string(txn.uuid_.c_str()));
                if (!mark_result.is_ok()) {
                    return dp::Result<void, dp::Error>::err(mark_result.error());
                }
            }

            blocks_.push_back(blockToAdd);
            return dp::Result<void, dp::Error>::ok();
        }

        inline dp::Result<void, dp::Error> addBlock(const std::string &uuid, T function,
                                                    std::shared_ptr<Crypto> privateKey_, dp::i16 priority = 100) {
            Transaction<T> genesisTransaction(uuid, function, priority);
            auto sign_result = genesisTransaction.signTransaction(privateKey_);
            if (!sign_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(sign_result.error());
            }
            Block<T> genesisBlock({genesisTransaction});
            return addBlock(genesisBlock);
        }

        inline dp::Result<bool, dp::Error> isValid() const {
            std::shared_lock lock(mutex_);

            if (blocks_.empty()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Chain is empty"));
            }

            if (blocks_.size() == 1) {
                return blocks_[0].isValid();
            }

            for (size_t i = 1; i < blocks_.size(); i++) {
                const Block<T> &currentBlock_ = blocks_[i];
                const Block<T> &previousBlock = blocks_[i - 1];

                auto valid_result = currentBlock_.isValid();
                if (!valid_result.is_ok()) {
                    return valid_result;
                }
                if (!valid_result.value()) {
                    std::string msg = "Block at index " + std::to_string(i) + " is invalid";
                    return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument(dp::String(msg.c_str())));
                }
                if (std::string(currentBlock_.previous_hash_.c_str()) != std::string(previousBlock.hash_.c_str())) {
                    std::string msg = "Block hash chain broken at index " + std::to_string(i);
                    return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument(dp::String(msg.c_str())));
                }
            }
            return dp::Result<bool, dp::Error>::ok(true);
        }

        inline dp::Result<void, dp::Error> registerEntity(const std::string &entity_id,
                                                          const std::string &initial_state = "inactive") {
            return entity_manager_.registerParticipant(entity_id, initial_state);
        }

        inline dp::Result<void, dp::Error>
        registerParticipant(const std::string &participant_id, const std::string &initial_state = "inactive",
                            const std::unordered_map<std::string, std::string> &metadata = {}) {
            return entity_manager_.registerParticipant(participant_id, initial_state, metadata);
        }

        inline bool isEntityAuthorized(const std::string &entity_id) const {
            return entity_manager_.isParticipantAuthorized(entity_id);
        }

        inline bool isParticipantAuthorized(const std::string &participant_id) const {
            return entity_manager_.isParticipantAuthorized(participant_id);
        }

        inline dp::Result<void, dp::Error> updateEntityState(const std::string &entity_id,
                                                             const std::string &new_state) {
            return entity_manager_.updateParticipantState(entity_id, new_state);
        }

        inline dp::Result<void, dp::Error> updateParticipantState(const std::string &participant_id,
                                                                  const std::string &new_state) {
            return entity_manager_.updateParticipantState(participant_id, new_state);
        }

        inline dp::Result<void, dp::Error> grantPermission(const std::string &entity_id,
                                                           const std::string &permission) {
            return entity_manager_.grantCapability(entity_id, permission);
        }

        inline dp::Result<void, dp::Error> grantCapability(const std::string &participant_id,
                                                           const std::string &capability) {
            return entity_manager_.grantCapability(participant_id, capability);
        }

        inline dp::Result<std::string, dp::Error> getParticipantMetadata(const std::string &participant_id,
                                                                         const std::string &key) const {
            return entity_manager_.getParticipantMetadata(participant_id, key);
        }

        inline dp::Result<void, dp::Error> setParticipantMetadata(const std::string &participant_id,
                                                                  const std::string &key, const std::string &value) {
            return entity_manager_.setParticipantMetadata(participant_id, key, value);
        }

        inline dp::Result<void, dp::Error> executeCommand(const std::string &issuer_entity, const std::string &command,
                                                          const std::string &tx_id,
                                                          const std::string &required_permission = "") {
            return entity_manager_.validateAndRecordAction(issuer_entity, command, tx_id, required_permission);
        }

        inline dp::Result<void, dp::Error> validateAndRecordAction(const std::string &issuer_participant,
                                                                   const std::string &action_description,
                                                                   const std::string &tx_id,
                                                                   const std::string &required_capability = "") {
            return entity_manager_.validateAndRecordAction(issuer_participant, action_description, tx_id,
                                                           required_capability);
        }

        inline void printChainSummary() const {
            std::shared_lock lock(mutex_);
            std::cout << "=== Blockchain Summary ===\n";
            std::cout << "Chain UUID: " << uuid_.c_str() << "\n";
            std::cout << "Total Blocks: " << blocks_.size() << "\n";

            auto valid_result = isValidUnsafe();
            std::cout << "Chain Valid: " << (valid_result ? "YES" : "NO") << "\n";

            size_t total_transactions = 0;
            for (const auto &block : blocks_)
                total_transactions += block.transactions_.size();
            std::cout << "Total Transactions: " << total_transactions << "\n";
            if (!blocks_.empty()) {
                std::cout << "Genesis Block Hash: " << std::string(blocks_[0].hash_.c_str()).substr(0, 16) << "...\n";
                std::cout << "Latest Block Hash: " << std::string(blocks_.back().hash_.c_str()).substr(0, 16)
                          << "...\n";
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

        inline dp::Result<void, dp::Error> revokeCapability(const std::string &participant_id,
                                                            const std::string &capability) {
            return entity_manager_.revokeCapability(participant_id, capability);
        }

        inline dp::Result<bool, dp::Error> isChainValid() const { return isValid(); }

        inline size_t getChainLength() const {
            std::shared_lock lock(mutex_);
            return blocks_.size();
        }

        inline dp::Result<Block<T>, dp::Error> getLastBlock() const {
            std::shared_lock lock(mutex_);
            if (blocks_.empty()) {
                return dp::Result<Block<T>, dp::Error>::err(dp::Error::invalid_argument("Chain is empty"));
            }
            return dp::Result<Block<T>, dp::Error>::ok(blocks_.back());
        }

        inline bool isTransactionUsed(const std::string &tx_id) const {
            return entity_manager_.isTransactionUsed(tx_id);
        }

        // ===========================================
        // PoA Consensus Support
        // ===========================================

        /// Initialize PoA consensus for this chain
        inline void initializePoA(const PoAConfig &config = PoAConfig{}) {
            std::unique_lock lock(mutex_);
            poa_ = std::make_unique<PoAConsensus>(config);
        }

        /// Check if PoA is enabled
        inline bool hasPoA() const {
            std::shared_lock lock(mutex_);
            return poa_ != nullptr;
        }

        /// Get PoA consensus (for direct access)
        inline PoAConsensus *getPoA() {
            std::shared_lock lock(mutex_);
            return poa_.get();
        }

        /// Add a validator to the PoA consensus
        inline dp::Result<void, dp::Error> addValidator(const std::string &participant_id, const Key &identity,
                                                        int weight = 1) {
            std::unique_lock lock(mutex_);
            if (!poa_) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("PoA not initialized"));
            }
            poa_->addValidator(participant_id, identity, weight);
            return dp::Result<void, dp::Error>::ok();
        }

        /// Remove a validator from the PoA consensus
        inline dp::Result<void, dp::Error> revokeValidator(const std::string &validator_id) {
            std::unique_lock lock(mutex_);
            if (!poa_) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("PoA not initialized"));
            }
            poa_->revokeValidator(validator_id);
            return dp::Result<void, dp::Error>::ok();
        }

        /// Get required signature count
        inline int getRequiredSignatures() const {
            std::shared_lock lock(mutex_);
            if (!poa_)
                return 0;
            return poa_->getRequiredSignatures();
        }

        /// Get active validator count
        inline size_t getActiveValidatorCount() const {
            std::shared_lock lock(mutex_);
            if (!poa_)
                return 0;
            return poa_->getActiveValidatorCount();
        }

        /// Add a block with PoA consensus validation
        inline dp::Result<void, dp::Error> addBlockWithPoA(Block<T> &block, const std::string &proposer_id) {
            std::unique_lock lock(mutex_);

            if (!poa_) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("PoA not initialized"));
            }

            if (blocks_.empty()) {
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument("Cannot add block to empty chain"));
            }

            // Set block metadata
            block.previous_hash_ = blocks_.back().hash_;
            block.index_ = blocks_.back().index_ + 1;
            block.setProposer(proposer_id);

            // Check for duplicate transactions
            for (const auto &txn : block.transactions_) {
                if (entity_manager_.isTransactionUsed(std::string(txn.uuid_.c_str()))) {
                    std::string msg = "Duplicate transaction detected: " + std::string(txn.uuid_.c_str());
                    return dp::Result<void, dp::Error>::err(dp::Error::already_exists(dp::String(msg.c_str())));
                }
            }

            // Build merkle tree and calculate hash
            block.buildMerkleTree();
            auto hash_result = block.calculateHash();
            if (!hash_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(hash_result.error());
            }
            block.hash_ = dp::String(hash_result.value().c_str());

            // Validate block
            auto valid_result = block.isValid();
            if (!valid_result.is_ok()) {
                return dp::Result<void, dp::Error>::err(valid_result.error());
            }
            if (!valid_result.value()) {
                return dp::Result<void, dp::Error>::err(
                    dp::Error::invalid_argument("Invalid block attempted to be added"));
            }

            // Check PoA quorum
            size_t required = static_cast<size_t>(poa_->getRequiredSignatures());
            if (block.countValidSignatures() < required) {
                std::string msg = "Block requires " + std::to_string(required) + " signatures, has " +
                                  std::to_string(block.countValidSignatures());
                return dp::Result<void, dp::Error>::err(dp::Error::invalid_argument(dp::String(msg.c_str())));
            }

            // Mark all transactions as used
            for (const auto &txn : block.transactions_) {
                auto mark_result = entity_manager_.markTransactionUsed(std::string(txn.uuid_.c_str()));
                if (!mark_result.is_ok()) {
                    return dp::Result<void, dp::Error>::err(mark_result.error());
                }
            }

            blocks_.push_back(block);
            return dp::Result<void, dp::Error>::ok();
        }

        /// Create a proposal for a new block (returns proposal ID)
        inline dp::Result<std::string, dp::Error> createBlockProposal(Block<T> &block, const std::string &proposer_id) {
            std::unique_lock lock(mutex_);

            if (!poa_) {
                return dp::Result<std::string, dp::Error>::err(dp::Error::invalid_argument("PoA not initialized"));
            }

            // Prepare block
            if (!blocks_.empty()) {
                block.previous_hash_ = blocks_.back().hash_;
                block.index_ = blocks_.back().index_ + 1;
            }
            block.setProposer(proposer_id);
            block.buildMerkleTree();

            auto hash_result = block.calculateHash();
            if (!hash_result.is_ok()) {
                return dp::Result<std::string, dp::Error>::err(hash_result.error());
            }
            block.hash_ = dp::String(hash_result.value().c_str());

            // Get proposer's validator ID
            auto validator = poa_->getValidatorByParticipant(proposer_id);
            if (!validator.is_ok()) {
                return dp::Result<std::string, dp::Error>::err(validator.error());
            }

            // Create proposal
            auto proposal_id = poa_->createProposal(hash_result.value(), validator.value()->getId());
            return dp::Result<std::string, dp::Error>::ok(proposal_id);
        }

        /// Sign a block proposal
        inline dp::Result<bool, dp::Error> signBlockProposal(const std::string &proposal_id, Block<T> &block,
                                                             const Key &signer_key) {
            std::unique_lock lock(mutex_);

            if (!poa_) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("PoA not initialized"));
            }

            // Sign the block hash
            auto hash = block.calculateHash();
            if (!hash.is_ok()) {
                return dp::Result<bool, dp::Error>::err(hash.error());
            }

            std::vector<uint8_t> hash_bytes(hash.value().begin(), hash.value().end());
            auto sig_result = signer_key.sign(hash_bytes);
            if (!sig_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(sig_result.error());
            }

            // Add signature to consensus
            auto add_result = poa_->addSignature(proposal_id, signer_key.getId(), sig_result.value());
            if (!add_result.is_ok()) {
                return dp::Result<bool, dp::Error>::err(add_result.error());
            }

            // Get validator info for block signature
            auto validator = poa_->getValidator(signer_key.getId());
            if (validator.is_ok()) {
                block.addValidatorSignature(signer_key.getId(), validator.value()->getParticipantId(),
                                            sig_result.value());
            }

            return dp::Result<bool, dp::Error>::ok(add_result.value()); // Returns true if quorum reached
        }

        // Serialize to binary using datapod
        inline dp::ByteBuf serialize() const {
            std::shared_lock lock(mutex_);
            auto &self = const_cast<Chain<T> &>(*this);
            return dp::serialize<dp::Mode::WITH_VERSION>(self);
        }

        // Deserialize from binary using datapod
        static dp::Result<Chain<T>, dp::Error> deserialize(const dp::ByteBuf &data) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, Chain<T>>(data);
                return dp::Result<Chain<T>, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<Chain<T>, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        static dp::Result<Chain<T>, dp::Error> deserialize(const dp::u8 *data, dp::usize size) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, Chain<T>>(data, size);
                return dp::Result<Chain<T>, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<Chain<T>, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        inline dp::Result<void, dp::Error> saveToFile(const std::string &filename) const {
            std::shared_lock lock(mutex_);
            try {
                auto data = serializeUnsafe();
                std::ofstream file(filename, std::ios::binary);
                if (!file.is_open()) {
                    return dp::Result<void, dp::Error>::err(dp::Error::io_error("Failed to open file for writing"));
                }
                file.write(reinterpret_cast<const char *>(data.data()), data.size());
                file.close();
                return dp::Result<void, dp::Error>::ok();
            } catch (const std::exception &e) {
                return dp::Result<void, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

        inline dp::Result<void, dp::Error> loadFromFile(const std::string &filename) {
            try {
                std::ifstream file(filename, std::ios::binary | std::ios::ate);
                if (!file.is_open()) {
                    return dp::Result<void, dp::Error>::err(dp::Error::io_error("Failed to open file for reading"));
                }
                auto size = file.tellg();
                file.seekg(0, std::ios::beg);
                dp::ByteBuf buffer(size);
                file.read(reinterpret_cast<char *>(buffer.data()), size);
                file.close();

                auto result = Chain<T>::deserialize(buffer);
                if (!result.is_ok()) {
                    return dp::Result<void, dp::Error>::err(result.error());
                }
                std::unique_lock lock(mutex_);
                uuid_ = std::move(result.value().uuid_);
                timestamp_ = result.value().timestamp_;
                blocks_ = std::move(result.value().blocks_);
                entity_manager_ = std::move(result.value().entity_manager_);
                return dp::Result<void, dp::Error>::ok();
            } catch (const std::exception &e) {
                return dp::Result<void, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
            }
        }

      private:
        // Unsafe versions for internal use (caller must hold lock)
        inline bool isValidUnsafe() const {
            if (blocks_.empty())
                return false;
            if (blocks_.size() == 1) {
                auto result = blocks_[0].isValid();
                return result.is_ok() && result.value();
            }
            for (size_t i = 1; i < blocks_.size(); i++) {
                const Block<T> &currentBlock_ = blocks_[i];
                const Block<T> &previousBlock = blocks_[i - 1];
                auto result = currentBlock_.isValid();
                if (!result.is_ok() || !result.value())
                    return false;
                if (std::string(currentBlock_.previous_hash_.c_str()) != std::string(previousBlock.hash_.c_str()))
                    return false;
            }
            return true;
        }

        inline dp::ByteBuf serializeUnsafe() const {
            auto &self = const_cast<Chain<T> &>(*this);
            return dp::serialize<dp::Mode::WITH_VERSION>(self);
        }
    };

} // namespace blockit
