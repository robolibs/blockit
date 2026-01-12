#pragma once

#include <blockit/ledger/block.hpp>
#include <blockit/ledger/chain.hpp>
#include <blockit/ledger/transaction.hpp>
#include <blockit/storage/file_store.hpp>
#include <datapod/datapod.hpp>
#include <mutex>
#include <shared_mutex>

namespace blockit {

    using namespace datapod;

    // ===========================================
    // Blockit - Unified Blockchain + Storage
    // ===========================================

    /// High-level API that manages both blockchain ledger and file-based storage together
    /// This is the recommended way to use Blockit with persistent queryable storage
    template <typename T> class Blockit {
      public:
        Blockit() = default;
        ~Blockit() = default;

        // Non-copyable, movable
        Blockit(const Blockit &) = delete;
        Blockit &operator=(const Blockit &) = delete;

        Blockit(Blockit &&other) noexcept {
            std::unique_lock lock(other.mutex_);
            chain_ = std::move(other.chain_);
            store_ = std::move(other.store_);
            initialized_ = other.initialized_;
            pending_anchors_ = std::move(other.pending_anchors_);
            other.initialized_ = false;
        }

        Blockit &operator=(Blockit &&other) noexcept {
            if (this != &other) {
                std::unique_lock lock1(mutex_, std::defer_lock);
                std::unique_lock lock2(other.mutex_, std::defer_lock);
                std::lock(lock1, lock2);
                chain_ = std::move(other.chain_);
                store_ = std::move(other.store_);
                initialized_ = other.initialized_;
                pending_anchors_ = std::move(other.pending_anchors_);
                other.initialized_ = false;
            }
            return *this;
        }

        /// Initialize the store with blockchain and storage
        /// @param db_path Path to storage directory
        /// @param chain_name Name for the blockchain
        /// @param genesis_tx_id Genesis transaction ID
        /// @param genesis_data Genesis transaction data
        /// @param crypto Cryptographic key for signing
        /// @param opts Storage options
        /// @return Result indicating success or error
        Result<void, Error> initialize(const String &db_path, const String &chain_name, const String &genesis_tx_id,
                                       const T &genesis_data, std::shared_ptr<ledger::Crypto> crypto,
                                       const storage::OpenOptions &opts = storage::OpenOptions{});

        /// Register a schema extension
        /// Note: Schema extensions are not supported in file-based storage
        /// @param extension User's schema extension (ignored)
        /// @return Result indicating success
        Result<void, Error> registerSchema(const storage::ISchemaExtension &extension);

        // ===========================================
        // Unified Transaction API
        // ===========================================

        /// Create a transaction with automatic storage anchoring
        /// @param tx_id Transaction ID (unique)
        /// @param data Transaction data (your custom type T)
        /// @param content_id Content identifier for storage
        /// @param content_bytes Content bytes to store and anchor
        /// @param priority Transaction priority
        /// @return Result indicating success or error
        Result<void, Error> createTransaction(const String &tx_id, const T &data, const String &content_id,
                                              const Vector<u8> &content_bytes, i16 priority = 100);

        /// Add a block to the chain and automatically anchor all pending transactions
        /// @param transactions Vector of transactions to include in block
        /// @return Result indicating success or error
        Result<void, Error> addBlock(const std::vector<ledger::Transaction<T>> &transactions);

        // ===========================================
        // Query Operations
        // ===========================================

        /// Get blockchain
        ledger::Chain<T> &getChain();
        const ledger::Chain<T> &getChain() const;

        /// Get storage layer
        storage::FileStore &getStorage();
        const storage::FileStore &getStorage() const;

        /// Verify a content item against its blockchain anchor
        /// @param content_id Content identifier
        /// @param content_bytes Current content to verify
        /// @return Result with true if verified, false if mismatch
        Result<bool, Error> verifyContent(const String &content_id, const Vector<u8> &content_bytes);

        /// Get anchor information for content
        /// @param content_id Content identifier
        /// @return Optional anchor if found
        Optional<storage::Anchor> getAnchor(const String &content_id);

        // ===========================================
        // Statistics
        // ===========================================

        i64 getBlockCount() const;
        i64 getTransactionCount() const;
        i64 getAnchorCount() const;

        /// Check if blockchain and storage are in sync
        /// @return Result with true if consistent
        Result<bool, Error> verifyConsistency();

      private:
        ledger::Chain<T> chain_;
        storage::FileStore store_;
        bool initialized_ = false;

        // Thread safety
        mutable std::shared_mutex mutex_;

        // Pending transactions waiting to be anchored (tx_id -> {content_id, content_hash})
        std::map<std::string, std::pair<String, Vector<u8>>> pending_anchors_;
    };

    // ===========================================
    // Implementation
    // ===========================================

    template <typename T>
    Result<void, Error> Blockit<T>::initialize(const String &db_path, const String &chain_name,
                                               const String &genesis_tx_id, const T &genesis_data,
                                               std::shared_ptr<ledger::Crypto> crypto,
                                               const storage::OpenOptions &opts) {
        std::unique_lock lock(mutex_);

        // Open storage
        auto open_result = store_.open(db_path, opts);
        if (!open_result.is_ok()) {
            return open_result;
        }

        // Initialize core schema
        auto schema_result = store_.initializeCoreSchema();
        if (!schema_result.is_ok()) {
            return schema_result;
        }

        // Initialize blockchain
        chain_ =
            ledger::Chain<T>(std::string(chain_name.c_str()), std::string(genesis_tx_id.c_str()), genesis_data, crypto);

        // Store genesis block in storage
        auto genesis_block = chain_.blocks_[0];
        auto store_result = store_.storeBlock(
            genesis_block.index_, String(genesis_block.hash_.c_str()), String(genesis_block.previous_hash_.c_str()),
            String(genesis_block.merkle_root_.c_str()), genesis_block.timestamp_.sec, genesis_block.nonce_);

        if (!store_result.is_ok()) {
            return store_result;
        }

        // Commit the genesis block
        auto tx = store_.beginTransaction();
        tx->commit();

        initialized_ = true;
        return Result<void, Error>::ok();
    }

    template <typename T> Result<void, Error> Blockit<T>::registerSchema(const storage::ISchemaExtension &extension) {
        std::shared_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));
        return store_.registerExtension(extension);
    }

    template <typename T>
    Result<void, Error> Blockit<T>::createTransaction(const String &tx_id, const T &data, const String &content_id,
                                                      const Vector<u8> &content_bytes, i16 priority) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));

        // Compute content hash for anchoring later
        auto content_hash = storage::computeSHA256(content_bytes);
        if (content_hash.empty()) {
            return Result<void, Error>::err(Error::io_error("Hash computation failed"));
        }

        // Store the pending anchor mapping
        pending_anchors_[std::string(tx_id.c_str())] = {content_id, content_hash};

        (void)data;     // Data is used when creating the actual transaction in addBlock
        (void)priority; // Priority is used when creating the actual transaction in addBlock

        return Result<void, Error>::ok();
    }

    template <typename T>
    Result<void, Error> Blockit<T>::addBlock(const std::vector<ledger::Transaction<T>> &transactions) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));

        auto tx_guard = store_.beginTransaction();

        // Create blockchain block
        ledger::Block<T> block(transactions);

        // Add block to chain
        auto add_result = chain_.addBlock(block);
        if (!add_result.is_ok()) {
            return Result<void, Error>::err(add_result.error());
        }

        // Get the added block
        auto last_block_result = chain_.getLastBlock();
        if (!last_block_result.is_ok()) {
            return Result<void, Error>::err(last_block_result.error());
        }
        const auto &added_block = last_block_result.value();

        // Store block in storage
        auto store_result = store_.storeBlock(
            added_block.index_, String(added_block.hash_.c_str()), String(added_block.previous_hash_.c_str()),
            String(added_block.merkle_root_.c_str()), added_block.timestamp_.sec, added_block.nonce_);

        if (!store_result.is_ok()) {
            return store_result;
        }

        // Store transactions and create anchors
        for (const auto &tx : transactions) {
            // Store transaction in storage
            auto tx_payload = tx.serialize();
            Vector<u8> payload(tx_payload.begin(), tx_payload.end());

            auto tx_result = store_.storeTransaction(String(tx.uuid_.c_str()), added_block.index_, tx.timestamp_.sec,
                                                     tx.priority_, payload);

            if (!tx_result.is_ok()) {
                return tx_result;
            }

            // Check if this transaction has a pending anchor
            auto anchor_it = pending_anchors_.find(std::string(tx.uuid_.c_str()));
            if (anchor_it != pending_anchors_.end()) {
                const auto &[content_id, content_hash] = anchor_it->second;

                // Create anchor
                storage::TxRef tx_ref;
                tx_ref.tx_id = String(tx.uuid_.c_str());
                tx_ref.block_height = added_block.index_;

                // Convert merkle_root string to bytes
                auto merkle_bytes = storage::hexToHash(String(added_block.merkle_root_.c_str()));
                if (merkle_bytes.size() >= 32) {
                    for (usize i = 0; i < 32; ++i) {
                        tx_ref.merkle_root[i] = merkle_bytes[i];
                    }
                } else {
                    // Pad with zeros if merkle root is shorter
                    for (usize i = 0; i < 32; ++i) {
                        tx_ref.merkle_root[i] = (i < merkle_bytes.size()) ? merkle_bytes[i] : 0;
                    }
                }

                auto anchor_result = store_.createAnchor(content_id, content_hash, tx_ref);
                if (!anchor_result.is_ok()) {
                    return anchor_result;
                }

                // Remove from pending
                pending_anchors_.erase(anchor_it);
            }
        }

        tx_guard->commit();
        return Result<void, Error>::ok();
    }

    template <typename T> ledger::Chain<T> &Blockit<T>::getChain() { return chain_; }

    template <typename T> const ledger::Chain<T> &Blockit<T>::getChain() const { return chain_; }

    template <typename T> storage::FileStore &Blockit<T>::getStorage() { return store_; }

    template <typename T> const storage::FileStore &Blockit<T>::getStorage() const { return store_; }

    template <typename T>
    Result<bool, Error> Blockit<T>::verifyContent(const String &content_id, const Vector<u8> &content_bytes) {
        std::shared_lock lock(mutex_);
        if (!initialized_)
            return Result<bool, Error>::err(Error::invalid_argument("Store not initialized"));
        return store_.verifyAnchor(content_id, content_bytes);
    }

    template <typename T> Optional<storage::Anchor> Blockit<T>::getAnchor(const String &content_id) {
        std::shared_lock lock(mutex_);
        if (!initialized_)
            return Optional<storage::Anchor>();
        return store_.getAnchor(content_id);
    }

    template <typename T> i64 Blockit<T>::getBlockCount() const {
        std::shared_lock lock(mutex_);
        return const_cast<storage::FileStore &>(store_).getBlockCount();
    }

    template <typename T> i64 Blockit<T>::getTransactionCount() const {
        std::shared_lock lock(mutex_);
        return const_cast<storage::FileStore &>(store_).getTransactionCount();
    }

    template <typename T> i64 Blockit<T>::getAnchorCount() const {
        std::shared_lock lock(mutex_);
        return const_cast<storage::FileStore &>(store_).getAnchorCount();
    }

    template <typename T> Result<bool, Error> Blockit<T>::verifyConsistency() {
        std::shared_lock lock(mutex_);
        if (!initialized_)
            return Result<bool, Error>::err(Error::invalid_argument("Store not initialized"));

        // Check blockchain validity
        auto valid_result = chain_.isValid();
        if (!valid_result.is_ok()) {
            return Result<bool, Error>::err(valid_result.error());
        }
        if (!valid_result.value()) {
            return Result<bool, Error>::ok(false);
        }

        // Check storage chain continuity
        auto continuity_result = store_.verifyChainContinuity();
        if (!continuity_result.is_ok()) {
            return continuity_result;
        }
        if (!continuity_result.value()) {
            return Result<bool, Error>::ok(false);
        }

        // Check that block counts match
        i64 chain_blocks = static_cast<i64>(chain_.getChainLength());
        i64 db_blocks = const_cast<storage::FileStore &>(store_).getBlockCount();

        return Result<bool, Error>::ok(chain_blocks == db_blocks);
    }

} // namespace blockit
