#pragma once

#include <chrono>
#include <datapod/datapod.hpp>
#include <keylock/keylock.hpp>
#include <sstream>

#include "merkle.hpp"
#include "poa.hpp"
#include "transaction.hpp"

namespace blockit {
    namespace ledger {

        using namespace std::chrono;

        template <typename T> class Block {
          public:
            dp::i64 index_{0};
            dp::String previous_hash_{};
            dp::String hash_{};
            dp::Vector<Transaction<T>> transactions_{};
            dp::i64 nonce_{0};
            Timestamp timestamp_{};
            dp::String merkle_root_{};

            // PoA consensus fields
            dp::String proposer_id_{};
            dp::Vector<BlockSignature> validator_signatures_{};

            Block() = default;

            inline Block(const std::vector<Transaction<T>> &txns) {
                index_ = 0;
                previous_hash_ = "GENESIS";
                transactions_ = dp::Vector<Transaction<T>>(txns.begin(), txns.end());
                nonce_ = 0;
                timestamp_.sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
                timestamp_.nanosec =
                    duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count() % 1000000000;
                buildMerkleTree();
                auto hash_result = calculateHash();
                if (hash_result.is_ok()) {
                    hash_ = dp::String(hash_result.value().c_str());
                }
            }

            auto members() {
                return std::tie(index_, previous_hash_, hash_, transactions_, nonce_, timestamp_, merkle_root_,
                                proposer_id_, validator_signatures_);
            }
            auto members() const {
                return std::tie(index_, previous_hash_, hash_, transactions_, nonce_, timestamp_, merkle_root_,
                                proposer_id_, validator_signatures_);
            }

            // PoA methods
            inline dp::Result<void, dp::Error> addValidatorSignature(const std::string &validator_id,
                                                                     const std::string &participant_id,
                                                                     const std::vector<uint8_t> &signature) {
                // Check if validator already signed
                for (const auto &sig : validator_signatures_) {
                    if (std::string(sig.validator_id.c_str()) == validator_id) {
                        return dp::Result<void, dp::Error>::err(
                            dp::Error::invalid_argument("Validator already signed this block"));
                    }
                }

                BlockSignature block_sig;
                block_sig.validator_id = dp::String(validator_id.c_str());
                block_sig.participant_id = dp::String(participant_id.c_str());
                block_sig.signature = dp::Vector<dp::u8>(signature.begin(), signature.end());
                block_sig.signed_at = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

                validator_signatures_.push_back(block_sig);
                return dp::Result<void, dp::Error>::ok();
            }

            inline bool hasSigned(const std::string &validator_id) const {
                for (const auto &sig : validator_signatures_) {
                    if (std::string(sig.validator_id.c_str()) == validator_id) {
                        return true;
                    }
                }
                return false;
            }

            inline size_t countValidSignatures() const { return validator_signatures_.size(); }

            inline void setProposer(const std::string &proposer_id) { proposer_id_ = dp::String(proposer_id.c_str()); }

            inline std::string getProposer() const { return std::string(proposer_id_.c_str()); }

            inline dp::Result<void, dp::Error> buildMerkleTree() {
                std::vector<std::string> tx_strings;
                tx_strings.reserve(transactions_.size());
                for (const auto &txn : transactions_)
                    tx_strings.push_back(txn.toString());
                MerkleTree temp_tree(tx_strings);
                merkle_root_ = dp::String(temp_tree.getRootUnsafe().c_str());
                return dp::Result<void, dp::Error>::ok();
            }

            inline dp::Result<std::string, dp::Error> calculateHash() const {
                std::stringstream ss;
                ss << index_ << timestamp_.sec << timestamp_.nanosec << std::string(previous_hash_.c_str()) << nonce_;
                ss << std::string(merkle_root_.c_str());
                keylock::keylock crypto(keylock::Algorithm::XChaCha20_Poly1305, keylock::HashAlgorithm::SHA256);
                std::string data = ss.str();
                std::vector<dp::u8> data_vec(data.begin(), data.end());
                auto hash_result = crypto.hash(data_vec);
                if (hash_result.success)
                    return dp::Result<std::string, dp::Error>::ok(keylock::keylock::to_hex(hash_result.data));
                else
                    return dp::Result<std::string, dp::Error>::err(
                        dp::Error::io_error("Failed to calculate block hash"));
            }

            inline dp::Result<bool, dp::Error> isValid() const {
                if (index_ < 0) {
                    return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Block index is negative"));
                }
                if (previous_hash_.empty()) {
                    return dp::Result<bool, dp::Error>::err(
                        dp::Error::invalid_argument("Block previous hash is empty"));
                }
                if (hash_.empty()) {
                    return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Block hash is empty"));
                }

                auto calc_hash = calculateHash();
                if (!calc_hash.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(calc_hash.error());
                }
                if (std::string(hash_.c_str()) != calc_hash.value()) {
                    return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Block hash mismatch"));
                }

                std::vector<std::string> tx_strings;
                for (const auto &txn : transactions_)
                    tx_strings.push_back(txn.toString());
                MerkleTree verification_tree(tx_strings);
                if (std::string(merkle_root_.c_str()) != verification_tree.getRootUnsafe()) {
                    return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Merkle root mismatch"));
                }

                for (const auto &txn : transactions_) {
                    auto valid = txn.isValid();
                    if (!valid.is_ok()) {
                        return dp::Result<bool, dp::Error>::err(valid.error());
                    }
                    if (!valid.value()) {
                        return dp::Result<bool, dp::Error>::err(
                            dp::Error::invalid_argument("Block contains invalid transaction"));
                    }
                }
                return dp::Result<bool, dp::Error>::ok(true);
            }

            inline dp::Result<bool, dp::Error> verifyTransaction(size_t transaction_index) const {
                if (transaction_index >= transactions_.size()) {
                    return dp::Result<bool, dp::Error>::err(dp::Error::out_of_range("Transaction index out of range"));
                }
                std::vector<std::string> tx_strings;
                for (const auto &txn : transactions_)
                    tx_strings.push_back(txn.toString());
                MerkleTree verification_tree(tx_strings);
                auto proof_result = verification_tree.getProof(transaction_index);
                if (!proof_result.is_ok()) {
                    return dp::Result<bool, dp::Error>::err(proof_result.error());
                }
                return verification_tree.verifyProof(transactions_[transaction_index].toString(), transaction_index,
                                                     proof_result.value());
            }

            // Serialize to binary using datapod
            inline dp::ByteBuf serialize() const {
                auto &self = const_cast<Block<T> &>(*this);
                return dp::serialize<dp::Mode::WITH_VERSION>(self);
            }

            // Deserialize from binary using datapod
            static dp::Result<Block<T>, dp::Error> deserialize(const dp::ByteBuf &data) {
                try {
                    auto result = dp::deserialize<dp::Mode::WITH_VERSION, Block<T>>(data);
                    return dp::Result<Block<T>, dp::Error>::ok(std::move(result));
                } catch (const std::exception &e) {
                    return dp::Result<Block<T>, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
                }
            }

            static dp::Result<Block<T>, dp::Error> deserialize(const dp::u8 *data, dp::usize size) {
                try {
                    auto result = dp::deserialize<dp::Mode::WITH_VERSION, Block<T>>(data, size);
                    return dp::Result<Block<T>, dp::Error>::ok(std::move(result));
                } catch (const std::exception &e) {
                    return dp::Result<Block<T>, dp::Error>::err(dp::Error::io_error(dp::String(e.what())));
                }
            }
        };

    } // namespace ledger

    // Backward compatibility
    using ledger::Block;

} // namespace blockit
