#pragma once

#include <blockit/identity/credential_issuer.hpp>
#include <blockit/identity/credential_status.hpp>
#include <blockit/identity/did_registry.hpp>
#include <blockit/identity/verifiable_credential.hpp>
#include <blockit/ledger/block.hpp>
#include <blockit/ledger/chain.hpp>
#include <blockit/ledger/poa.hpp>
#include <blockit/ledger/transaction.hpp>
#include <blockit/storage/file_store.hpp>
#include <datapod/datapod.hpp>
#include <map>
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
                                       const T &genesis_data, std::shared_ptr<Crypto> crypto,
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
        Result<void, Error> addBlock(const std::vector<Transaction<T>> &transactions);

        // ===========================================
        // Query Operations
        // ===========================================

        /// Get blockchain
        Chain<T> &getChain();
        const Chain<T> &getChain() const;

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

        // ===========================================
        // PoA Consensus Support
        // ===========================================

        /// Initialize PoA consensus for the chain
        /// @param config PoA configuration
        void initializePoA(const PoAConfig &config = PoAConfig{});

        /// Check if PoA is enabled
        bool hasPoA() const;

        /// Add a validator
        /// @param participant_id Participant identifier
        /// @param identity Validator's key identity
        /// @param weight Validator weight (default 1)
        /// @return Result indicating success or error
        Result<void, Error> addValidator(const std::string &participant_id, const Key &identity, int weight = 1);

        /// Add a block with PoA consensus validation
        /// @param transactions Transactions to include
        /// @param proposer_id Proposer's participant ID
        /// @param signatures Map of validator_id -> signature
        /// @return Result indicating success or error
        Result<void, Error> addBlockWithPoA(const std::vector<Transaction<T>> &transactions,
                                            const std::string &proposer_id,
                                            const std::map<std::string, std::vector<uint8_t>> &signatures);

        /// Get required signatures for PoA
        int getRequiredSignatures() const;

        /// Get active validator count
        size_t getActiveValidatorCount() const;

        // ===========================================
        // DID Support
        // ===========================================

        /// Initialize DID support for the chain
        void initializeDID();

        /// Check if DID support is enabled
        bool hasDID() const;

        /// Get DID registry
        DIDRegistry *getDIDRegistry();
        const DIDRegistry *getDIDRegistry() const;

        /// Get credential status list
        CredentialStatusList *getCredentialStatusList();
        const CredentialStatusList *getCredentialStatusList() const;

        /// Create a DID for a new identity
        /// @param key The key pair for the new identity
        /// @return Result with pair of (DIDDocument, DIDOperation) on success
        Result<std::pair<DIDDocument, DIDOperation>, Error> createDID(const Key &key);

        /// Resolve a DID to its document
        /// @param did The DID to resolve
        /// @return Result with DIDDocument on success
        Result<DIDDocument, Error> resolveDID(const DID &did) const;

        /// Resolve a DID string to its document
        Result<DIDDocument, Error> resolveDID(const std::string &did_string) const;

        /// Create a robot identity with DID and authorization credential
        /// @param robot_key The robot's key pair
        /// @param robot_id Unique robot identifier (e.g., "ROBOT_007")
        /// @param capabilities List of robot capabilities
        /// @param issuer_key Key of the credential issuer
        /// @return Result with pair of (DIDDocument, VerifiableCredential) on success
        Result<std::pair<DIDDocument, VerifiableCredential>, Error>
        createRobotIdentity(const Key &robot_key, const std::string &robot_id,
                            const std::vector<std::string> &capabilities, const Key &issuer_key);

        /// Issue a credential using the built-in credential issuer
        /// @param issuer_key Issuer's key pair
        /// @param subject_did Subject's DID
        /// @param type Credential type
        /// @param claims Map of claim key-value pairs
        /// @param validity_duration How long the credential is valid (0 = no expiration)
        /// @return Result with VerifiableCredential on success
        Result<VerifiableCredential, Error>
        issueCredential(const Key &issuer_key, const DID &subject_did, CredentialType type,
                        const std::map<std::string, std::string> &claims,
                        std::chrono::milliseconds validity_duration = std::chrono::milliseconds(0));

        /// Store a DID document in the file store
        /// @param doc The DID document to store
        /// @return Result indicating success or error
        Result<void, Error> storeDIDDocument(const DIDDocument &doc);

        /// Store a credential in the file store
        /// @param credential The credential to store
        /// @return Result indicating success or error
        Result<void, Error> storeCredential(const VerifiableCredential &credential);

        /// Load a credential from the file store
        /// @param credential_id The credential ID
        /// @return Optional with VerifiableCredential if found
        Optional<VerifiableCredential> loadCredential(const std::string &credential_id);

      private:
        Chain<T> chain_;
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
                                               std::shared_ptr<Crypto> crypto, const storage::OpenOptions &opts) {
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
        chain_ = Chain<T>(std::string(chain_name.c_str()), std::string(genesis_tx_id.c_str()), genesis_data, crypto);

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

    template <typename T> Result<void, Error> Blockit<T>::addBlock(const std::vector<Transaction<T>> &transactions) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));

        auto tx_guard = store_.beginTransaction();

        // Create blockchain block
        Block<T> block(transactions);

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

    template <typename T> Chain<T> &Blockit<T>::getChain() { return chain_; }

    template <typename T> const Chain<T> &Blockit<T>::getChain() const { return chain_; }

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

    // ===========================================
    // PoA Implementation
    // ===========================================

    template <typename T> void Blockit<T>::initializePoA(const PoAConfig &config) {
        std::unique_lock lock(mutex_);
        chain_.initializePoA(config);
    }

    template <typename T> bool Blockit<T>::hasPoA() const {
        std::shared_lock lock(mutex_);
        return chain_.hasPoA();
    }

    template <typename T>
    Result<void, Error> Blockit<T>::addValidator(const std::string &participant_id, const Key &identity, int weight) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));
        return chain_.addValidator(participant_id, identity, weight);
    }

    template <typename T>
    Result<void, Error> Blockit<T>::addBlockWithPoA(const std::vector<Transaction<T>> &transactions,
                                                    const std::string &proposer_id,
                                                    const std::map<std::string, std::vector<uint8_t>> &signatures) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));

        if (!chain_.hasPoA()) {
            return Result<void, Error>::err(Error::invalid_argument("PoA not initialized"));
        }

        auto tx_guard = store_.beginTransaction();

        // Create blockchain block
        Block<T> block(transactions);
        block.setProposer(proposer_id);

        // Add all validator signatures to the block
        for (const auto &[validator_id, signature] : signatures) {
            auto validator = chain_.getPoA()->getValidator(validator_id);
            if (validator.is_ok()) {
                block.addValidatorSignature(validator_id, validator.value()->getParticipantId(), signature);
            }
        }

        // Add block using PoA validation
        auto add_result = chain_.addBlockWithPoA(block, proposer_id);
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

        // Store transactions
        for (const auto &tx : transactions) {
            auto tx_payload = tx.serialize();
            Vector<u8> payload(tx_payload.begin(), tx_payload.end());

            auto tx_result = store_.storeTransaction(String(tx.uuid_.c_str()), added_block.index_, tx.timestamp_.sec,
                                                     tx.priority_, payload);

            if (!tx_result.is_ok()) {
                return tx_result;
            }

            // Handle pending anchors
            auto anchor_it = pending_anchors_.find(std::string(tx.uuid_.c_str()));
            if (anchor_it != pending_anchors_.end()) {
                const auto &[content_id, content_hash] = anchor_it->second;

                storage::TxRef tx_ref;
                tx_ref.tx_id = String(tx.uuid_.c_str());
                tx_ref.block_height = added_block.index_;

                auto merkle_bytes = storage::hexToHash(String(added_block.merkle_root_.c_str()));
                for (usize i = 0; i < 32; ++i) {
                    tx_ref.merkle_root[i] = (i < merkle_bytes.size()) ? merkle_bytes[i] : 0;
                }

                auto anchor_result = store_.createAnchor(content_id, content_hash, tx_ref);
                if (!anchor_result.is_ok()) {
                    return anchor_result;
                }

                pending_anchors_.erase(anchor_it);
            }
        }

        tx_guard->commit();
        return Result<void, Error>::ok();
    }

    template <typename T> int Blockit<T>::getRequiredSignatures() const {
        std::shared_lock lock(mutex_);
        return chain_.getRequiredSignatures();
    }

    template <typename T> size_t Blockit<T>::getActiveValidatorCount() const {
        std::shared_lock lock(mutex_);
        return chain_.getActiveValidatorCount();
    }

    // ===========================================
    // DID Implementation
    // ===========================================

    template <typename T> void Blockit<T>::initializeDID() {
        std::unique_lock lock(mutex_);
        chain_.initializeDID();
    }

    template <typename T> bool Blockit<T>::hasDID() const {
        std::shared_lock lock(mutex_);
        return chain_.hasDID();
    }

    template <typename T> DIDRegistry *Blockit<T>::getDIDRegistry() {
        std::shared_lock lock(mutex_);
        return chain_.getDIDRegistry();
    }

    template <typename T> const DIDRegistry *Blockit<T>::getDIDRegistry() const {
        std::shared_lock lock(mutex_);
        return chain_.getDIDRegistry();
    }

    template <typename T> CredentialStatusList *Blockit<T>::getCredentialStatusList() {
        std::shared_lock lock(mutex_);
        return chain_.getCredentialStatusList();
    }

    template <typename T> const CredentialStatusList *Blockit<T>::getCredentialStatusList() const {
        std::shared_lock lock(mutex_);
        return chain_.getCredentialStatusList();
    }

    template <typename T> Result<std::pair<DIDDocument, DIDOperation>, Error> Blockit<T>::createDID(const Key &key) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<std::pair<DIDDocument, DIDOperation>, Error>::err(
                Error::invalid_argument("Store not initialized"));

        if (!chain_.hasDID())
            return Result<std::pair<DIDDocument, DIDOperation>, Error>::err(
                Error::invalid_argument("DID support not initialized"));

        auto registry = chain_.getDIDRegistry();
        auto result = registry->create(key);
        if (!result.is_ok()) {
            return Result<std::pair<DIDDocument, DIDOperation>, Error>::err(result.error());
        }

        // Store the DID document in FileStore
        const auto &[doc, op] = result.value();
        auto doc_bytes = doc.serialize();

        storage::DIDRecord record;
        record.did = String(doc.getId().toString().c_str());
        record.document = Vector<u8>(doc_bytes.begin(), doc_bytes.end());
        record.version = 1;
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        record.created_at = now;
        record.updated_at = now;
        record.status = 0; // Active

        auto tx = store_.beginTransaction();
        auto store_result = store_.storeDID(record);
        if (!store_result.is_ok()) {
            return Result<std::pair<DIDDocument, DIDOperation>, Error>::err(store_result.error());
        }
        tx->commit();

        return Result<std::pair<DIDDocument, DIDOperation>, Error>::ok(result.value());
    }

    template <typename T> Result<DIDDocument, Error> Blockit<T>::resolveDID(const DID &did) const {
        std::shared_lock lock(mutex_);
        if (!initialized_)
            return Result<DIDDocument, Error>::err(Error::invalid_argument("Store not initialized"));

        if (!chain_.hasDID())
            return Result<DIDDocument, Error>::err(Error::invalid_argument("DID support not initialized"));

        return chain_.getDIDRegistry()->resolve(did);
    }

    template <typename T> Result<DIDDocument, Error> Blockit<T>::resolveDID(const std::string &did_string) const {
        std::shared_lock lock(mutex_);
        if (!initialized_)
            return Result<DIDDocument, Error>::err(Error::invalid_argument("Store not initialized"));

        if (!chain_.hasDID())
            return Result<DIDDocument, Error>::err(Error::invalid_argument("DID support not initialized"));

        return chain_.getDIDRegistry()->resolve(did_string);
    }

    template <typename T>
    Result<std::pair<DIDDocument, VerifiableCredential>, Error>
    Blockit<T>::createRobotIdentity(const Key &robot_key, const std::string &robot_id,
                                    const std::vector<std::string> &capabilities, const Key &issuer_key) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::err(
                Error::invalid_argument("Store not initialized"));

        if (!chain_.hasDID())
            return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::err(
                Error::invalid_argument("DID support not initialized"));

        auto registry = chain_.getDIDRegistry();
        auto status_list = chain_.getCredentialStatusList();

        // Create DID for the robot
        auto did_result = registry->create(robot_key);
        if (!did_result.is_ok()) {
            return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::err(did_result.error());
        }

        const auto &[robot_doc, did_op] = did_result.value();
        auto robot_did = robot_doc.getId();

        // Create issuer DID and CredentialIssuer
        auto issuer_did = DID::fromKey(issuer_key);

        // Ensure issuer DID exists in registry
        if (!registry->exists(issuer_did)) {
            auto issuer_did_result = registry->create(issuer_key);
            if (!issuer_did_result.is_ok()) {
                return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::err(issuer_did_result.error());
            }
        }

        CredentialIssuer issuer(issuer_did, issuer_key);

        // Issue robot authorization credential
        auto cred_result = issuer.issueRobotAuthorization(robot_did, robot_id, capabilities);
        if (!cred_result.is_ok()) {
            return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::err(cred_result.error());
        }

        auto credential = cred_result.value();

        // Record credential in status list
        status_list->recordIssue(credential.getId(), issuer_did.toString());

        // Store robot DID in FileStore
        auto robot_doc_bytes = robot_doc.serialize();
        storage::DIDRecord robot_record;
        robot_record.did = String(robot_did.toString().c_str());
        robot_record.document = Vector<u8>(robot_doc_bytes.begin(), robot_doc_bytes.end());
        robot_record.version = 1;
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        robot_record.created_at = now;
        robot_record.updated_at = now;
        robot_record.status = 0;

        auto tx = store_.beginTransaction();
        auto store_did_result = store_.storeDID(robot_record);
        if (!store_did_result.is_ok()) {
            return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::err(store_did_result.error());
        }

        // Store credential in FileStore
        auto cred_bytes = credential.serialize();
        storage::CredentialRecord cred_record;
        cred_record.credential_id = String(credential.getId().c_str());
        cred_record.issuer_did = String(issuer_did.toString().c_str());
        cred_record.subject_did = String(robot_did.toString().c_str());
        cred_record.credential = Vector<u8>(cred_bytes.begin(), cred_bytes.end());
        cred_record.status = 0;
        cred_record.issued_at = now;
        cred_record.expires_at = credential.getExpirationDate();

        auto store_cred_result = store_.storeCredential(cred_record);
        if (!store_cred_result.is_ok()) {
            return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::err(store_cred_result.error());
        }

        tx->commit();

        return Result<std::pair<DIDDocument, VerifiableCredential>, Error>::ok({robot_doc, credential});
    }

    template <typename T>
    Result<VerifiableCredential, Error> Blockit<T>::issueCredential(const Key &issuer_key, const DID &subject_did,
                                                                    CredentialType type,
                                                                    const std::map<std::string, std::string> &claims,
                                                                    std::chrono::milliseconds validity_duration) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<VerifiableCredential, Error>::err(Error::invalid_argument("Store not initialized"));

        if (!chain_.hasDID())
            return Result<VerifiableCredential, Error>::err(Error::invalid_argument("DID support not initialized"));

        auto status_list = chain_.getCredentialStatusList();
        auto issuer_did = DID::fromKey(issuer_key);

        CredentialIssuer issuer(issuer_did, issuer_key);

        auto cred_result = issuer.issueCustomCredential(subject_did, type, claims, validity_duration);
        if (!cred_result.is_ok()) {
            return Result<VerifiableCredential, Error>::err(cred_result.error());
        }

        auto credential = cred_result.value();

        // Record in status list
        status_list->recordIssue(credential.getId(), issuer_did.toString());

        // Store in FileStore
        auto cred_bytes = credential.serialize();
        storage::CredentialRecord cred_record;
        cred_record.credential_id = String(credential.getId().c_str());
        cred_record.issuer_did = String(issuer_did.toString().c_str());
        cred_record.subject_did = String(subject_did.toString().c_str());
        cred_record.credential = Vector<u8>(cred_bytes.begin(), cred_bytes.end());
        cred_record.status = 0;
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        cred_record.issued_at = now;
        cred_record.expires_at = credential.getExpirationDate();

        auto tx = store_.beginTransaction();
        auto store_result = store_.storeCredential(cred_record);
        if (!store_result.is_ok()) {
            return Result<VerifiableCredential, Error>::err(store_result.error());
        }
        tx->commit();

        return Result<VerifiableCredential, Error>::ok(credential);
    }

    template <typename T> Result<void, Error> Blockit<T>::storeDIDDocument(const DIDDocument &doc) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));

        auto doc_bytes = doc.serialize();
        storage::DIDRecord record;
        record.did = String(doc.getId().toString().c_str());
        record.document = Vector<u8>(doc_bytes.begin(), doc_bytes.end());
        record.version = 1;
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        record.created_at = now;
        record.updated_at = now;
        record.status = doc.isActive() ? static_cast<u8>(0) : static_cast<u8>(1);

        auto tx = store_.beginTransaction();
        auto result = store_.storeDID(record);
        if (!result.is_ok()) {
            return result;
        }
        tx->commit();
        return Result<void, Error>::ok();
    }

    template <typename T> Result<void, Error> Blockit<T>::storeCredential(const VerifiableCredential &credential) {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return Result<void, Error>::err(Error::invalid_argument("Store not initialized"));

        auto cred_bytes = credential.serialize();
        storage::CredentialRecord record;
        record.credential_id = String(credential.getId().c_str());
        record.issuer_did = String(credential.getIssuerString().c_str());
        record.subject_did = String(credential.getSubjectDID().toString().c_str());
        record.credential = Vector<u8>(cred_bytes.begin(), cred_bytes.end());
        record.status = 0; // Active
        auto now =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        record.issued_at = now;
        record.expires_at = credential.getExpirationDate();

        auto tx = store_.beginTransaction();
        auto result = store_.storeCredential(record);
        if (!result.is_ok()) {
            return result;
        }
        tx->commit();
        return Result<void, Error>::ok();
    }

    template <typename T> Optional<VerifiableCredential> Blockit<T>::loadCredential(const std::string &credential_id) {
        std::shared_lock lock(mutex_);
        if (!initialized_)
            return Optional<VerifiableCredential>();

        auto record = store_.loadCredential(String(credential_id.c_str()));
        if (!record.has_value()) {
            return Optional<VerifiableCredential>();
        }

        // Deserialize the credential
        std::vector<uint8_t> cred_data(record->credential.begin(), record->credential.end());
        ByteBuf buf(cred_data.begin(), cred_data.end());
        auto result = VerifiableCredential::deserialize(buf);
        if (!result.is_ok()) {
            return Optional<VerifiableCredential>();
        }

        return Optional<VerifiableCredential>(result.value());
    }

} // namespace blockit
