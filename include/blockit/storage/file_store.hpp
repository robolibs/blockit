#pragma once

#include <datapod/datapod.hpp>
#include <filesystem>
#include <fstream>
#include <keylock/keylock.hpp>
#include <map>
#include <unordered_map>

namespace blockit::storage {

    using namespace datapod;

    // ===========================================
    // Utility functions
    // ===========================================

    inline i64 currentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    inline Vector<u8> computeSHA256(const Vector<u8> &data) {
        keylock::keylock crypto(keylock::Algorithm::XChaCha20_Poly1305, keylock::HashAlgorithm::SHA256);
        std::vector<uint8_t> input(data.begin(), data.end());
        auto result = crypto.hash(input);
        if (!result.success) {
            return Vector<u8>{};
        }
        return Vector<u8>(result.data.begin(), result.data.end());
    }

    inline String hashToHex(const Vector<u8> &hash) {
        std::vector<uint8_t> input(hash.begin(), hash.end());
        return String(keylock::keylock::to_hex(input).c_str());
    }

    inline Vector<u8> hexToHash(const String &hex) {
        auto result = keylock::keylock::from_hex(std::string(hex.c_str()));
        return Vector<u8>(result.begin(), result.end());
    }

    // ===========================================
    // Core Types - POD structs with members()
    // ===========================================

    /// Transaction reference for anchoring
    struct TxRef {
        String tx_id;
        i64 block_height = 0;
        Array<u8, 32> merkle_root = {};

        auto members() { return std::tie(tx_id, block_height, merkle_root); }
        auto members() const { return std::tie(tx_id, block_height, merkle_root); }
    };

    /// Anchor linking external data to on-chain transaction
    struct Anchor {
        String content_id;
        Vector<u8> content_hash;
        String tx_id;
        i64 block_height = 0;
        Array<u8, 32> merkle_root = {};
        i64 anchored_at = 0;

        auto members() { return std::tie(content_id, content_hash, tx_id, block_height, merkle_root, anchored_at); }
        auto members() const {
            return std::tie(content_id, content_hash, tx_id, block_height, merkle_root, anchored_at);
        }
    };

    /// Block record for storage
    struct BlockRecord {
        i64 height = 0;
        String hash;
        String previous_hash;
        String merkle_root;
        i64 timestamp = 0;
        i64 nonce = 0;
        i64 created_at = 0;

        auto members() { return std::tie(height, hash, previous_hash, merkle_root, timestamp, nonce, created_at); }
        auto members() const {
            return std::tie(height, hash, previous_hash, merkle_root, timestamp, nonce, created_at);
        }

        String toJson() const {
            std::ostringstream oss;
            oss << "{\"height\":" << height << ",\"hash\":\"" << hash.c_str() << "\",\"previous_hash\":\""
                << previous_hash.c_str() << "\",\"merkle_root\":\"" << merkle_root.c_str()
                << "\",\"timestamp\":" << timestamp << ",\"nonce\":" << nonce << "}";
            return String(oss.str().c_str());
        }
    };

    /// Transaction record for storage
    struct TransactionRecord {
        String tx_id;
        i64 block_height = 0;
        i64 timestamp = 0;
        i16 priority = 0;
        Vector<u8> payload;
        i64 created_at = 0;

        auto members() { return std::tie(tx_id, block_height, timestamp, priority, payload, created_at); }
        auto members() const { return std::tie(tx_id, block_height, timestamp, priority, payload, created_at); }
    };

    /// Anchor record for storage
    struct AnchorRecord {
        String content_id;
        Vector<u8> content_hash;
        String tx_id;
        i64 block_height = 0;
        Array<u8, 32> merkle_root = {};
        i64 anchored_at = 0;

        auto members() { return std::tie(content_id, content_hash, tx_id, block_height, merkle_root, anchored_at); }
        auto members() const {
            return std::tie(content_id, content_hash, tx_id, block_height, merkle_root, anchored_at);
        }

        Anchor toAnchor() const {
            Anchor a;
            a.content_id = content_id;
            a.content_hash = content_hash;
            a.tx_id = tx_id;
            a.block_height = block_height;
            a.merkle_root = merkle_root;
            a.anchored_at = anchored_at;
            return a;
        }
    };

    /// Query filter for ledger queries
    struct LedgerQuery {
        Optional<i64> block_height_min;
        Optional<i64> block_height_max;
        Optional<i64> timestamp_min;
        Optional<i64> timestamp_max;
        Optional<String> tx_id;
        Optional<String> content_id;
        i32 limit = 100;
        i32 offset = 0;

        auto members() {
            return std::tie(block_height_min, block_height_max, timestamp_min, timestamp_max, tx_id, content_id, limit,
                            offset);
        }
        auto members() const {
            return std::tie(block_height_min, block_height_max, timestamp_min, timestamp_max, tx_id, content_id, limit,
                            offset);
        }
    };

    /// Storage configuration options
    struct OpenOptions {
        bool enable_wal = true;          // Unused (API compat)
        bool enable_foreign_keys = true; // Unused (API compat)
        i32 busy_timeout_ms = 5000;      // Unused (API compat)
        i32 cache_size_kb = 20000;       // Unused (API compat)
        enum class Synchronous { OFF = 0, NORMAL = 1, FULL = 2 };
        Synchronous sync_mode = Synchronous::NORMAL;

        auto members() { return std::tie(enable_wal, enable_foreign_keys, busy_timeout_ms, cache_size_kb, sync_mode); }
        auto members() const {
            return std::tie(enable_wal, enable_foreign_keys, busy_timeout_ms, cache_size_kb, sync_mode);
        }
    };

    // Schema extensions not supported in file store (API kept for compatibility)
    class ISchemaExtension {
      public:
        virtual ~ISchemaExtension() = default;
        virtual Vector<String> getCreateTableStatements() const = 0;
        virtual Vector<String> getCreateIndexStatements() const = 0;
        virtual Vector<Pair<i32, Vector<String>>> getMigrations() const { return {}; }
    };

    // ===========================================
    // FileStore - File-based ledger storage
    // ===========================================

    class FileStore {
      public:
        inline FileStore() : is_open_(false), sync_mode_(OpenOptions::Synchronous::NORMAL) {}

        inline ~FileStore() { close(); }

        // Non-copyable, movable
        FileStore(const FileStore &) = delete;
        FileStore &operator=(const FileStore &) = delete;

        inline FileStore(FileStore &&other) noexcept
            : base_path_(std::move(other.base_path_)), is_open_(other.is_open_), sync_mode_(other.sync_mode_),
              block_index_(std::move(other.block_index_)), tx_index_(std::move(other.tx_index_)),
              anchor_index_(std::move(other.anchor_index_)), hash_to_height_(std::move(other.hash_to_height_)) {
            other.is_open_ = false;
        }

        inline FileStore &operator=(FileStore &&other) noexcept {
            if (this != &other) {
                close();
                base_path_ = std::move(other.base_path_);
                is_open_ = other.is_open_;
                sync_mode_ = other.sync_mode_;
                block_index_ = std::move(other.block_index_);
                tx_index_ = std::move(other.tx_index_);
                anchor_index_ = std::move(other.anchor_index_);
                hash_to_height_ = std::move(other.hash_to_height_);
                other.is_open_ = false;
            }
            return *this;
        }

        /// Open or create storage at given path (directory)
        inline Result<void, Error> open(const String &path, const OpenOptions &opts = OpenOptions{}) {
            try {
                base_path_ = std::string(path.c_str());
                sync_mode_ = opts.sync_mode;

                std::filesystem::create_directories(base_path_);
                loadIndexes();

                is_open_ = true;
                return Result<void, Error>::ok();
            } catch (const std::exception &e) {
                is_open_ = false;
                return Result<void, Error>::err(Error::io_error(String(e.what())));
            }
        }

        /// Close storage
        inline void close() {
            if (is_open_) {
                flushIndexes();
                is_open_ = false;
            }
        }

        /// Check if storage is open
        inline bool isOpen() const { return is_open_; }

        /// Initialize core schema (creates necessary files)
        inline Result<void, Error> initializeCoreSchema() {
            if (!is_open_)
                return Result<void, Error>::err(Error::invalid_argument("Store not open"));

            try {
                auto blocks_path = base_path_ / "blocks.dat";
                auto tx_path = base_path_ / "transactions.dat";
                auto anchors_path = base_path_ / "anchors.dat";

                if (!std::filesystem::exists(blocks_path)) {
                    std::ofstream(blocks_path, std::ios::binary).close();
                }
                if (!std::filesystem::exists(tx_path)) {
                    std::ofstream(tx_path, std::ios::binary).close();
                }
                if (!std::filesystem::exists(anchors_path)) {
                    std::ofstream(anchors_path, std::ios::binary).close();
                }

                return Result<void, Error>::ok();
            } catch (const std::exception &e) {
                return Result<void, Error>::err(Error::io_error(String(e.what())));
            }
        }

        /// Register schema extension (no-op for file store)
        inline Result<void, Error> registerExtension(const ISchemaExtension &) {
            if (!is_open_)
                return Result<void, Error>::err(Error::invalid_argument("Store not open"));
            return Result<void, Error>::ok();
        }

        // ===========================================
        // Transaction management (RAII)
        // ===========================================

        class TxGuard {
          public:
            inline explicit TxGuard(FileStore &store) : store_(store), committed_(false) {
                store_.pending_blocks_.clear();
                store_.pending_transactions_.clear();
                store_.pending_anchors_.clear();
            }

            inline ~TxGuard() {
                if (!committed_) {
                    store_.pending_blocks_.clear();
                    store_.pending_transactions_.clear();
                    store_.pending_anchors_.clear();
                }
            }

            TxGuard(const TxGuard &) = delete;
            TxGuard &operator=(const TxGuard &) = delete;

            inline void commit() {
                if (!committed_) {
                    store_.flushPending();
                    committed_ = true;
                }
            }

            inline void rollback() {
                if (!committed_) {
                    store_.pending_blocks_.clear();
                    store_.pending_transactions_.clear();
                    store_.pending_anchors_.clear();
                    committed_ = true;
                }
            }

          private:
            FileStore &store_;
            bool committed_;
        };

        inline std::unique_ptr<TxGuard> beginTransaction() { return std::make_unique<TxGuard>(*this); }

        // ===========================================
        // Core Ledger Operations
        // ===========================================

        /// Store a block in the ledger
        inline Result<void, Error> storeBlock(i64 index, const String &hash, const String &previous_hash,
                                              const String &merkle_root, i64 timestamp, i64 nonce) {
            if (!is_open_)
                return Result<void, Error>::err(Error::invalid_argument("Store not open"));

            BlockRecord record;
            record.height = index;
            record.hash = hash;
            record.previous_hash = previous_hash;
            record.merkle_root = merkle_root;
            record.timestamp = timestamp;
            record.nonce = nonce;
            record.created_at = currentTimestamp();

            pending_blocks_.push_back(record);
            return Result<void, Error>::ok();
        }

        /// Store a transaction in the ledger
        inline Result<void, Error> storeTransaction(const String &tx_id, i64 block_height, i64 timestamp, i16 priority,
                                                    const Vector<u8> &payload) {
            if (!is_open_)
                return Result<void, Error>::err(Error::invalid_argument("Store not open"));

            TransactionRecord record;
            record.tx_id = tx_id;
            record.block_height = block_height;
            record.timestamp = timestamp;
            record.priority = priority;
            record.payload = payload;
            record.created_at = currentTimestamp();

            pending_transactions_.push_back(record);
            return Result<void, Error>::ok();
        }

        /// Create an anchor linking external content to on-chain transaction
        inline Result<void, Error> createAnchor(const String &content_id, const Vector<u8> &content_hash,
                                                const TxRef &tx_ref) {
            if (!is_open_)
                return Result<void, Error>::err(Error::invalid_argument("Store not open"));

            AnchorRecord record;
            record.content_id = content_id;
            record.content_hash = content_hash;
            record.tx_id = tx_ref.tx_id;
            record.block_height = tx_ref.block_height;
            record.merkle_root = tx_ref.merkle_root;
            record.anchored_at = currentTimestamp();

            pending_anchors_.push_back(record);
            return Result<void, Error>::ok();
        }

        /// Get anchor by content ID
        inline Optional<Anchor> getAnchor(const String &content_id) {
            if (!is_open_)
                return Optional<Anchor>();

            // Check pending first
            for (const auto &record : pending_anchors_) {
                if (std::string(record.content_id.c_str()) == std::string(content_id.c_str())) {
                    return Optional<Anchor>(record.toAnchor());
                }
            }

            // Check index
            std::string key(content_id.c_str());
            auto it = anchor_index_.find(key);
            if (it == anchor_index_.end())
                return Optional<Anchor>();

            return readAnchorAt(it->second);
        }

        /// Get all anchors for a specific block height
        inline Vector<Anchor> getAnchorsByBlock(i64 block_height) {
            Vector<Anchor> anchors;
            if (!is_open_)
                return anchors;

            // Check pending
            for (const auto &record : pending_anchors_) {
                if (record.block_height == block_height) {
                    anchors.push_back(record.toAnchor());
                }
            }

            // Read all anchors and filter
            auto all_anchors = readAllAnchors();
            for (auto &anchor : all_anchors) {
                if (anchor.block_height == block_height) {
                    anchors.push_back(std::move(anchor));
                }
            }

            return anchors;
        }

        /// Get all anchors within a height range
        inline Vector<Anchor> getAnchorsInRange(i64 min_height, i64 max_height) {
            Vector<Anchor> anchors;
            if (!is_open_)
                return anchors;

            // Check pending
            for (const auto &record : pending_anchors_) {
                if (record.block_height >= min_height && record.block_height <= max_height) {
                    anchors.push_back(record.toAnchor());
                }
            }

            // Read all anchors and filter
            auto all_anchors = readAllAnchors();
            for (auto &anchor : all_anchors) {
                if (anchor.block_height >= min_height && anchor.block_height <= max_height) {
                    anchors.push_back(std::move(anchor));
                }
            }

            return anchors;
        }

        /// Query transactions with filters
        inline Vector<String> queryTransactions(const LedgerQuery &query) {
            Vector<String> tx_ids;
            if (!is_open_)
                return tx_ids;

            auto all_tx = readAllTransactions();
            Vector<TransactionRecord> filtered;

            auto matchesQuery = [&query](const TransactionRecord &tx) {
                if (query.block_height_min.has_value() && tx.block_height < *query.block_height_min)
                    return false;
                if (query.block_height_max.has_value() && tx.block_height > *query.block_height_max)
                    return false;
                if (query.timestamp_min.has_value() && tx.timestamp < *query.timestamp_min)
                    return false;
                if (query.timestamp_max.has_value() && tx.timestamp > *query.timestamp_max)
                    return false;
                if (query.tx_id.has_value() && std::string(tx.tx_id.c_str()) != std::string(query.tx_id->c_str()))
                    return false;
                return true;
            };

            for (auto &tx : all_tx) {
                if (matchesQuery(tx))
                    filtered.push_back(std::move(tx));
            }

            for (const auto &tx : pending_transactions_) {
                if (matchesQuery(tx))
                    filtered.push_back(tx);
            }

            // Sort by block_height, timestamp
            std::sort(filtered.begin(), filtered.end(), [](const TransactionRecord &a, const TransactionRecord &b) {
                if (a.block_height != b.block_height)
                    return a.block_height < b.block_height;
                return a.timestamp < b.timestamp;
            });

            // Apply offset and limit
            i32 offset = query.offset;
            i32 limit = query.limit;
            for (usize i = offset; i < filtered.size() && static_cast<i32>(tx_ids.size()) < limit; ++i) {
                tx_ids.push_back(filtered[i].tx_id);
            }

            return tx_ids;
        }

        /// Get block by height
        inline Optional<String> getBlockByHeight(i64 height) {
            if (!is_open_)
                return Optional<String>();

            // Check pending first
            for (const auto &record : pending_blocks_) {
                if (record.height == height) {
                    return Optional<String>(record.toJson());
                }
            }

            // Check index
            auto it = block_index_.find(height);
            if (it == block_index_.end())
                return Optional<String>();

            auto record = readBlockAt(it->second);
            if (!record.has_value())
                return Optional<String>();

            return Optional<String>(record->toJson());
        }

        /// Get block by hash
        inline Optional<String> getBlockByHash(const String &hash) {
            if (!is_open_)
                return Optional<String>();

            // Check pending first
            for (const auto &record : pending_blocks_) {
                if (std::string(record.hash.c_str()) == std::string(hash.c_str())) {
                    return Optional<String>(record.toJson());
                }
            }

            // Check hash index
            std::string key(hash.c_str());
            auto it = hash_to_height_.find(key);
            if (it == hash_to_height_.end())
                return Optional<String>();

            return getBlockByHeight(it->second);
        }

        /// Get transaction by ID
        inline Optional<Vector<u8>> getTransaction(const String &tx_id) {
            if (!is_open_)
                return Optional<Vector<u8>>();

            // Check pending first
            for (const auto &record : pending_transactions_) {
                if (std::string(record.tx_id.c_str()) == std::string(tx_id.c_str())) {
                    return Optional<Vector<u8>>(record.payload);
                }
            }

            // Check index
            std::string key(tx_id.c_str());
            auto it = tx_index_.find(key);
            if (it == tx_index_.end())
                return Optional<Vector<u8>>();

            auto record = readTransactionAt(it->second);
            if (!record.has_value())
                return Optional<Vector<u8>>();

            return Optional<Vector<u8>>(record->payload);
        }

        // ===========================================
        // Verification & Integrity
        // ===========================================

        /// Verify anchor integrity by recomputing hash
        inline Result<bool, Error> verifyAnchor(const String &content_id, const Vector<u8> &current_content) {
            auto anchor = getAnchor(content_id);
            if (!anchor.has_value())
                return Result<bool, Error>::ok(false);

            auto computed_hash = computeSHA256(current_content);
            if (computed_hash.empty())
                return Result<bool, Error>::err(Error::io_error("Hash computation failed"));

            bool matches = (computed_hash.size() == anchor->content_hash.size());
            if (matches) {
                for (usize i = 0; i < computed_hash.size(); ++i) {
                    if (computed_hash[i] != anchor->content_hash[i]) {
                        matches = false;
                        break;
                    }
                }
            }
            return Result<bool, Error>::ok(matches);
        }

        /// Check if chain is continuous (no gaps in block heights)
        inline Result<bool, Error> verifyChainContinuity() {
            if (!is_open_)
                return Result<bool, Error>::err(Error::invalid_argument("Store not open"));

            if (block_index_.empty() && pending_blocks_.empty())
                return Result<bool, Error>::ok(true);

            Vector<i64> heights;
            for (const auto &[height, _] : block_index_) {
                heights.push_back(height);
            }
            for (const auto &block : pending_blocks_) {
                heights.push_back(block.height);
            }

            if (heights.empty())
                return Result<bool, Error>::ok(true);

            std::sort(heights.begin(), heights.end());
            heights.erase(std::unique(heights.begin(), heights.end()), heights.end());

            i64 expected_count = heights.back() - heights.front() + 1;
            return Result<bool, Error>::ok(static_cast<i64>(heights.size()) == expected_count);
        }

        // ===========================================
        // Statistics & Diagnostics
        // ===========================================

        inline i64 getBlockCount() {
            if (!is_open_)
                return 0;
            return static_cast<i64>(block_index_.size() + pending_blocks_.size());
        }

        inline i64 getTransactionCount() {
            if (!is_open_)
                return 0;
            return static_cast<i64>(tx_index_.size() + pending_transactions_.size());
        }

        inline i64 getAnchorCount() {
            if (!is_open_)
                return 0;
            return static_cast<i64>(anchor_index_.size() + pending_anchors_.size());
        }

        inline i64 getLatestBlockHeight() {
            if (!is_open_)
                return -1;

            i64 max_height = -1;
            for (const auto &[height, _] : block_index_) {
                max_height = std::max(max_height, height);
            }
            for (const auto &block : pending_blocks_) {
                max_height = std::max(max_height, block.height);
            }
            return max_height;
        }

        inline Result<bool, Error> quickCheck() {
            if (!is_open_)
                return Result<bool, Error>::err(Error::invalid_argument("Store not open"));

            try {
                bool exists = std::filesystem::exists(base_path_ / "blocks.dat") &&
                              std::filesystem::exists(base_path_ / "transactions.dat") &&
                              std::filesystem::exists(base_path_ / "anchors.dat");
                return Result<bool, Error>::ok(exists);
            } catch (const std::exception &e) {
                return Result<bool, Error>::err(Error::io_error(String(e.what())));
            }
        }

        // ===========================================
        // Raw access (limited support for file store)
        // ===========================================

        inline Result<void, Error> executeSql(const String &) {
            return Result<void, Error>::err(Error::not_found("SQL not supported in file store"));
        }

        inline Result<i64, Error> executeUpdate(const String &, const Vector<String> &) {
            return Result<i64, Error>::err(Error::not_found("SQL not supported in file store"));
        }

        inline Result<void, Error> executeQuery(const String &, std::function<void(const Vector<String> &)>) {
            return Result<void, Error>::err(Error::not_found("SQL not supported in file store"));
        }

      private:
        // ===========================================
        // File I/O with datapod serialization
        // ===========================================

        template <typename T>
        inline void appendRecord(const std::filesystem::path &file, const T &record, u64 &offset) {
            std::ofstream out(file, std::ios::binary | std::ios::app);
            if (!out)
                throw std::runtime_error("Failed to open file for writing");

            offset = out.tellp();

            // Serialize using datapod (need mutable copy)
            T mutable_record = record;
            auto buffer = datapod::serialize(mutable_record);
            u32 len = static_cast<u32>(buffer.size());

            out.write(reinterpret_cast<const char *>(&len), sizeof(len));
            out.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());

            if (sync_mode_ == OpenOptions::Synchronous::FULL) {
                out.flush();
            }
        }

        template <typename T> inline Optional<T> readRecordAt(const std::filesystem::path &file, u64 offset) {
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return Optional<T>();

            in.seekg(offset);

            u32 len;
            in.read(reinterpret_cast<char *>(&len), sizeof(len));
            if (!in)
                return Optional<T>();

            ByteBuf data(len);
            in.read(reinterpret_cast<char *>(data.data()), len);
            if (!in)
                return Optional<T>();

            return Optional<T>(datapod::deserialize<Mode::NONE, T>(data));
        }

        inline Optional<BlockRecord> readBlockAt(u64 offset) {
            return readRecordAt<BlockRecord>(base_path_ / "blocks.dat", offset);
        }

        inline Optional<TransactionRecord> readTransactionAt(u64 offset) {
            return readRecordAt<TransactionRecord>(base_path_ / "transactions.dat", offset);
        }

        inline Optional<Anchor> readAnchorAt(u64 offset) {
            auto record = readRecordAt<AnchorRecord>(base_path_ / "anchors.dat", offset);
            if (!record.has_value())
                return Optional<Anchor>();
            return Optional<Anchor>(record->toAnchor());
        }

        template <typename T> inline Vector<T> readAllRecords(const std::filesystem::path &file) {
            Vector<T> records;
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return records;

            while (in) {
                u32 len;
                in.read(reinterpret_cast<char *>(&len), sizeof(len));
                if (!in)
                    break;

                ByteBuf data(len);
                in.read(reinterpret_cast<char *>(data.data()), len);
                if (!in)
                    break;

                records.push_back(datapod::deserialize<Mode::NONE, T>(data));
            }

            return records;
        }

        inline Vector<TransactionRecord> readAllTransactions() {
            return readAllRecords<TransactionRecord>(base_path_ / "transactions.dat");
        }

        inline Vector<Anchor> readAllAnchors() {
            auto records = readAllRecords<AnchorRecord>(base_path_ / "anchors.dat");
            Vector<Anchor> anchors;
            for (const auto &r : records) {
                anchors.push_back(r.toAnchor());
            }
            return anchors;
        }

        // ===========================================
        // Index management
        // ===========================================

        inline void loadIndexes() {
            block_index_.clear();
            tx_index_.clear();
            anchor_index_.clear();
            hash_to_height_.clear();

            loadBlockIndex();
            loadTransactionIndex();
            loadAnchorIndex();
        }

        inline void loadBlockIndex() {
            auto file = base_path_ / "blocks.dat";
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return;

            while (in) {
                u64 record_offset = in.tellg();

                u32 len;
                in.read(reinterpret_cast<char *>(&len), sizeof(len));
                if (!in)
                    break;

                ByteBuf data(len);
                in.read(reinterpret_cast<char *>(data.data()), len);
                if (!in)
                    break;

                auto record = datapod::deserialize<Mode::NONE, BlockRecord>(data);
                block_index_[record.height] = record_offset;
                hash_to_height_[std::string(record.hash.c_str())] = record.height;
            }
        }

        inline void loadTransactionIndex() {
            auto file = base_path_ / "transactions.dat";
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return;

            while (in) {
                u64 record_offset = in.tellg();

                u32 len;
                in.read(reinterpret_cast<char *>(&len), sizeof(len));
                if (!in)
                    break;

                ByteBuf data(len);
                in.read(reinterpret_cast<char *>(data.data()), len);
                if (!in)
                    break;

                auto record = datapod::deserialize<Mode::NONE, TransactionRecord>(data);
                tx_index_[std::string(record.tx_id.c_str())] = record_offset;
            }
        }

        inline void loadAnchorIndex() {
            auto file = base_path_ / "anchors.dat";
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return;

            while (in) {
                u64 record_offset = in.tellg();

                u32 len;
                in.read(reinterpret_cast<char *>(&len), sizeof(len));
                if (!in)
                    break;

                ByteBuf data(len);
                in.read(reinterpret_cast<char *>(data.data()), len);
                if (!in)
                    break;

                auto record = datapod::deserialize<Mode::NONE, AnchorRecord>(data);
                anchor_index_[std::string(record.content_id.c_str())] = record_offset;
            }
        }

        inline void flushPending() {
            // Flush blocks
            for (const auto &record : pending_blocks_) {
                u64 offset;
                appendRecord(base_path_ / "blocks.dat", record, offset);
                block_index_[record.height] = offset;
                hash_to_height_[std::string(record.hash.c_str())] = record.height;
            }
            pending_blocks_.clear();

            // Flush transactions
            for (const auto &record : pending_transactions_) {
                u64 offset;
                appendRecord(base_path_ / "transactions.dat", record, offset);
                tx_index_[std::string(record.tx_id.c_str())] = offset;
            }
            pending_transactions_.clear();

            // Flush anchors
            for (const auto &record : pending_anchors_) {
                u64 offset;
                appendRecord(base_path_ / "anchors.dat", record, offset);
                anchor_index_[std::string(record.content_id.c_str())] = offset;
            }
            pending_anchors_.clear();
        }

        inline void flushIndexes() {
            // Indexes are maintained in memory and rebuilt on load
        }

        // ===========================================
        // Member variables
        // ===========================================

        std::filesystem::path base_path_;
        bool is_open_;
        OpenOptions::Synchronous sync_mode_;

        // In-memory indexes
        std::map<i64, u64> block_index_;
        std::unordered_map<std::string, u64> tx_index_;
        std::unordered_map<std::string, u64> anchor_index_;
        std::unordered_map<std::string, i64> hash_to_height_;

        // Pending writes
        Vector<BlockRecord> pending_blocks_;
        Vector<TransactionRecord> pending_transactions_;
        Vector<AnchorRecord> pending_anchors_;
    };

} // namespace blockit::storage
