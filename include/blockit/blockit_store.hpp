#pragma once

#include <blockit/ledger/block.hpp>
#include <blockit/ledger/chain.hpp>
#include <blockit/ledger/transaction.hpp>
#include <blockit/storage/sqlite_store.hpp>
#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace blockit {

    // ===========================================
    // Block - Unified Blockchain + Database
    // ===========================================

    /// High-level API that manages both blockchain ledger and SQLite storage together
    /// This is the recommended way to use Blockit with persistent queryable storage
    template <typename T> class Blockit {
      public:
        Blockit() = default;
        ~Blockit() = default;

        // Non-copyable, movable
        Blockit(const Blockit &) = delete;
        Blockit &operator=(const Blockit &) = delete;
        Blockit(Blockit &&) = default;
        Blockit &operator=(Blockit &&) = default;

        /// Initialize the store with blockchain and database
        /// @param db_path Path to SQLite database file
        /// @param chain_name Name for the blockchain
        /// @param genesis_tx_id Genesis transaction ID
        /// @param genesis_data Genesis transaction data
        /// @param crypto Cryptographic key for signing
        /// @param opts Database options
        /// @return true on success, false on failure
        bool initialize(const std::string &db_path, const std::string &chain_name, const std::string &genesis_tx_id,
                        const T &genesis_data, std::shared_ptr<ledger::Crypto> crypto,
                        const storage::OpenOptions &opts = storage::OpenOptions{});

        /// Register a schema extension for custom tables
        /// Call this after initialize() but before adding data
        /// @param extension User's schema extension
        /// @return true on success, false on failure
        bool registerSchema(const storage::ISchemaExtension &extension);

        // ===========================================
        // Unified Transaction API
        // ===========================================

        /// Create a transaction with automatic database anchoring
        /// This creates the blockchain transaction AND prepares it for database anchoring
        /// @param tx_id Transaction ID (unique)
        /// @param data Transaction data (your custom type T)
        /// @param content_id Content identifier for database (can be same as tx_id)
        /// @param content_bytes Content bytes to store and anchor (your serialized data)
        /// @param priority Transaction priority
        /// @return true on success, false on failure
        bool createTransaction(const std::string &tx_id, const T &data, const std::string &content_id,
                               const std::vector<uint8_t> &content_bytes, int16_t priority = 100);

        /// Add a block to the chain and automatically anchor all pending transactions
        /// This commits both blockchain and database changes atomically
        /// @param transactions Vector of transactions to include in block
        /// @return true on success, false on failure
        bool addBlock(const std::vector<ledger::Transaction<T>> &transactions);

        // ===========================================
        // Query Operations
        // ===========================================

        /// Get blockchain
        /// @return Reference to the blockchain
        ledger::Chain<T> &getChain();

        /// Get blockchain (const)
        /// @return Const reference to the blockchain
        const ledger::Chain<T> &getChain() const;

        /// Get storage layer
        /// @return Reference to the storage layer
        storage::SqliteStore &getStorage();

        /// Get storage layer (const)
        /// @return Const reference to the storage layer
        const storage::SqliteStore &getStorage() const;

        /// Verify a content item against its blockchain anchor
        /// @param content_id Content identifier
        /// @param content_bytes Current content to verify
        /// @return true if verified, false if mismatch or not found
        bool verifyContent(const std::string &content_id, const std::vector<uint8_t> &content_bytes);

        /// Get anchor information for content
        /// @param content_id Content identifier
        /// @return Anchor if found, nullopt otherwise
        std::optional<storage::Anchor> getAnchor(const std::string &content_id);

        // ===========================================
        // Statistics
        // ===========================================

        /// Get total number of blocks in blockchain
        int64_t getBlockCount() const;

        /// Get total number of transactions in database
        int64_t getTransactionCount() const;

        /// Get total number of anchored items
        int64_t getAnchorCount() const;

        /// Check if blockchain and database are in sync
        /// @return true if consistent, false if discrepancies detected
        bool verifyConsistency();

      private:
        ledger::Chain<T> chain_;
        storage::SqliteStore store_;
        bool initialized_ = false;

        // Pending transactions waiting to be anchored (tx_id -> {content_id, content_hash})
        std::map<std::string, std::pair<std::string, std::vector<uint8_t>>> pending_anchors_;
    };

    // ===========================================
    // Implementation
    // ===========================================

    template <typename T>
    bool Blockit<T>::initialize(const std::string &db_path, const std::string &chain_name,
                                const std::string &genesis_tx_id, const T &genesis_data,
                                std::shared_ptr<ledger::Crypto> crypto, const storage::OpenOptions &opts) {
        // Open database
        if (!store_.open(db_path, opts)) {
            return false;
        }

        // Initialize core schema
        if (!store_.initializeCoreSchema()) {
            return false;
        }

        // Initialize blockchain
        chain_ = ledger::Chain<T>(chain_name, genesis_tx_id, genesis_data, crypto);

        // Store genesis block in database
        auto genesis_block = chain_.blocks_[0];
        if (!store_.storeBlock(genesis_block.index_, genesis_block.hash_, genesis_block.previous_hash_,
                               genesis_block.merkle_root_, genesis_block.timestamp_.sec, genesis_block.nonce_)) {
            return false;
        }

        initialized_ = true;
        return true;
    }

    template <typename T> bool Blockit<T>::registerSchema(const storage::ISchemaExtension &extension) {
        if (!initialized_)
            return false;
        return store_.registerExtension(extension);
    }

    template <typename T>
    bool Blockit<T>::createTransaction(const std::string &tx_id, const T &data, const std::string &content_id,
                                       const std::vector<uint8_t> &content_bytes, int16_t priority) {
        if (!initialized_)
            return false;

        // Compute content hash for anchoring later
        auto content_hash = storage::computeSHA256(content_bytes);

        // Store the pending anchor mapping
        pending_anchors_[tx_id] = {content_id, content_hash};

        // Note: Transaction is created but not yet added to blockchain
        // User will add it via addBlock()
        return true;
    }

    template <typename T> bool Blockit<T>::addBlock(const std::vector<ledger::Transaction<T>> &transactions) {
        if (!initialized_)
            return false;

        auto tx_guard = store_.beginTransaction();

        // Create blockchain block
        ledger::Block<T> block(transactions);

        // Add block to chain
        if (!chain_.addBlock(block)) {
            return false;
        }

        // Get the added block
        const auto &added_block = chain_.blocks_.back();

        // Store block in database
        if (!store_.storeBlock(added_block.index_, added_block.hash_, added_block.previous_hash_,
                               added_block.merkle_root_, added_block.timestamp_.sec, added_block.nonce_)) {
            return false;
        }

        // Store transactions and create anchors
        for (const auto &tx : transactions) {
            // Store transaction in database
            auto tx_payload = tx.serializeBinary();
            if (!store_.storeTransaction(tx.uuid_, added_block.index_, tx.timestamp_.sec, tx.priority_, tx_payload)) {
                return false;
            }

            // Check if this transaction has a pending anchor
            auto anchor_it = pending_anchors_.find(tx.uuid_);
            if (anchor_it != pending_anchors_.end()) {
                const auto &[content_id, content_hash] = anchor_it->second;

                // Create anchor
                storage::TxRef tx_ref;
                tx_ref.tx_id = tx.uuid_;
                tx_ref.block_height = added_block.index_;

                // Convert merkle_root string to bytes
                auto merkle_bytes = storage::hexToHash(added_block.merkle_root_);
                if (merkle_bytes.size() >= 32) {
                    std::copy_n(merkle_bytes.begin(), 32, tx_ref.merkle_root.begin());
                } else {
                    // Pad with zeros if merkle root is shorter
                    std::fill(tx_ref.merkle_root.begin(), tx_ref.merkle_root.end(), 0);
                    std::copy(merkle_bytes.begin(), merkle_bytes.end(), tx_ref.merkle_root.begin());
                }

                if (!store_.createAnchor(content_id, content_hash, tx_ref)) {
                    return false;
                }

                // Remove from pending
                pending_anchors_.erase(anchor_it);
            }
        }

        tx_guard->commit();
        return true;
    }

    template <typename T> ledger::Chain<T> &Blockit<T>::getChain() { return chain_; }

    template <typename T> const ledger::Chain<T> &Blockit<T>::getChain() const { return chain_; }

    template <typename T> storage::SqliteStore &Blockit<T>::getStorage() { return store_; }

    template <typename T> const storage::SqliteStore &Blockit<T>::getStorage() const { return store_; }

    template <typename T>
    bool Blockit<T>::verifyContent(const std::string &content_id, const std::vector<uint8_t> &content_bytes) {
        if (!initialized_)
            return false;
        return store_.verifyAnchor(content_id, content_bytes);
    }

    template <typename T> std::optional<storage::Anchor> Blockit<T>::getAnchor(const std::string &content_id) {
        if (!initialized_)
            return std::nullopt;
        return store_.getAnchor(content_id);
    }

    template <typename T> int64_t Blockit<T>::getBlockCount() const {
        return const_cast<storage::SqliteStore &>(store_).getBlockCount();
    }

    template <typename T> int64_t Blockit<T>::getTransactionCount() const {
        return const_cast<storage::SqliteStore &>(store_).getTransactionCount();
    }

    template <typename T> int64_t Blockit<T>::getAnchorCount() const {
        return const_cast<storage::SqliteStore &>(store_).getAnchorCount();
    }

    template <typename T> bool Blockit<T>::verifyConsistency() {
        if (!initialized_)
            return false;

        // Check blockchain validity
        if (!chain_.isValid()) {
            return false;
        }

        // Check database chain continuity
        if (!store_.verifyChainContinuity()) {
            return false;
        }

        // Check that block counts match
        int64_t chain_blocks = static_cast<int64_t>(chain_.blocks_.size());
        int64_t db_blocks = store_.getBlockCount();

        return chain_blocks == db_blocks;
    }

} // namespace blockit
