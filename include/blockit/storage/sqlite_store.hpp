#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <keylock/keylock.hpp>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace blockit::storage {

    // ===========================================
    // Utility functions
    // ===========================================

    /// Get current Unix timestamp in seconds
    inline int64_t currentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    /// Compute SHA256 hash using keylock
    inline std::vector<uint8_t> computeSHA256(const std::vector<uint8_t> &data) {
        keylock::keylock crypto(keylock::Algorithm::XChaCha20_Poly1305, keylock::HashAlgorithm::SHA256);
        auto result = crypto.hash(data);
        if (!result.success) {
            throw std::runtime_error("SHA256 hashing failed");
        }
        return result.data;
    }

    /// Convert hash bytes to hex string
    inline std::string hashToHex(const std::vector<uint8_t> &hash) { return keylock::keylock::to_hex(hash); }

    /// Convert hex string to hash bytes
    inline std::vector<uint8_t> hexToHash(const std::string &hex) { return keylock::keylock::from_hex(hex); }

    // ===========================================
    // Core Types
    // ===========================================

    /// Transaction reference for anchoring
    struct TxRef {
        std::string tx_id;
        int64_t block_height;
        std::array<uint8_t, 32> merkle_root;

        TxRef() : block_height(0), merkle_root{} {}
        TxRef(std::string id, int64_t height, std::array<uint8_t, 32> root)
            : tx_id(std::move(id)), block_height(height), merkle_root(root) {}
    };

    /// Anchor linking external data to on-chain transaction
    struct Anchor {
        std::string content_id;              // User's identifier (e.g., document ID, user ID, etc.)
        std::vector<uint8_t> content_hash;   // SHA256 of user's data
        std::string tx_id;                   // On-chain transaction ID
        int64_t block_height;                // Block height where tx was finalized
        std::array<uint8_t, 32> merkle_root; // Merkle root of the block
        int64_t anchored_at;                 // Timestamp when anchor was created

        Anchor() : block_height(0), merkle_root{}, anchored_at(0) {}
    };

    /// Query filter for ledger queries
    struct LedgerQuery {
        std::optional<int64_t> block_height_min;
        std::optional<int64_t> block_height_max;
        std::optional<int64_t> timestamp_min;
        std::optional<int64_t> timestamp_max;
        std::optional<std::string> tx_id;
        std::optional<std::string> content_id;
        int32_t limit = 100;
        int32_t offset = 0;

        LedgerQuery() = default;
    };

    /// SQLite database configuration
    struct OpenOptions {
        bool enable_wal = true;
        bool enable_foreign_keys = true;
        int32_t busy_timeout_ms = 5000;
        int32_t cache_size_kb = 20000;
        enum class Synchronous { OFF = 0, NORMAL = 1, FULL = 2 };
        Synchronous sync_mode = Synchronous::NORMAL;

        OpenOptions() = default;
    };

    // ===========================================
    // Extension Interface - Users implement this
    // ===========================================

    /// Interface for user-defined schema extensions
    /// Users implement this to create their own tables that reference the core ledger
    class ISchemaExtension {
      public:
        virtual ~ISchemaExtension() = default;

        /// Return CREATE TABLE statements for custom tables
        /// Should use "IF NOT EXISTS" for idempotency
        /// Can reference core ledger tables via foreign keys
        virtual std::vector<std::string> getCreateTableStatements() const = 0;

        /// Return CREATE INDEX statements for custom indexes
        virtual std::vector<std::string> getCreateIndexStatements() const = 0;

        /// Optional: return migration statements for schema version upgrades
        /// Key = version number, Value = SQL statements to execute
        virtual std::vector<std::pair<int32_t, std::vector<std::string>>> getMigrations() const { return {}; }
    };

    // ===========================================
    // SqliteStore - Core ledger storage
    // ===========================================

    class SqliteStore {
      public:
        inline SqliteStore() : db_(nullptr), is_open_(false) {}

        inline ~SqliteStore() { close(); }

        // Non-copyable, movable
        SqliteStore(const SqliteStore &) = delete;
        SqliteStore &operator=(const SqliteStore &) = delete;

        inline SqliteStore(SqliteStore &&other) noexcept
            : db_(other.db_), db_path_(std::move(other.db_path_)), is_open_(other.is_open_) {
            other.db_ = nullptr;
            other.is_open_ = false;
        }

        inline SqliteStore &operator=(SqliteStore &&other) noexcept {
            if (this != &other) {
                close();
                db_ = other.db_;
                db_path_ = std::move(other.db_path_);
                is_open_ = other.is_open_;
                other.db_ = nullptr;
                other.is_open_ = false;
            }
            return *this;
        }

        /// Open or create database at given path
        inline bool open(const std::string &path, const OpenOptions &opts = OpenOptions{}) {
            int rc = sqlite3_open(path.c_str(), &db_);
            if (rc != SQLITE_OK) {
                if (db_) {
                    sqlite3_close(db_);
                    db_ = nullptr;
                }
                is_open_ = false;
                return false;
            }

            db_path_ = path;
            is_open_ = true;
            applyPragmas(opts);
            return true;
        }

        /// Close database connection
        inline void close() {
            if (db_) {
                sqlite3_close(db_);
                db_ = nullptr;
                is_open_ = false;
            }
        }

        /// Check if database is open
        inline bool isOpen() const { return is_open_; }

        /// Initialize core ledger schema and run migrations
        inline bool initializeCoreSchema() {
            if (!db_ || !is_open_)
                return false;

            auto tx = beginTransaction();

            // Create schema migrations table first
            if (!executeSql(SCHEMA_MIGRATIONS_TABLE)) {
                return false;
            }

            int32_t current_version = getCurrentSchemaVersion();

            // Apply migrations if needed
            if (current_version < 1) {
                if (!createCoreSchemaV1())
                    return false;
                if (!setSchemaVersion(1))
                    return false;
            }

            tx->commit();
            return true;
        }

        /// Register and initialize user's schema extension
        inline bool registerExtension(const ISchemaExtension &extension) {
            if (!db_ || !is_open_)
                return false;

            auto tx = beginTransaction();

            // Execute CREATE TABLE statements
            for (const auto &sql : extension.getCreateTableStatements()) {
                if (!executeSql(sql)) {
                    return false;
                }
            }

            // Execute CREATE INDEX statements
            for (const auto &sql : extension.getCreateIndexStatements()) {
                if (!executeSql(sql)) {
                    return false;
                }
            }

            // Apply migrations
            for (const auto &[version, statements] : extension.getMigrations()) {
                for (const auto &sql : statements) {
                    if (!executeSql(sql)) {
                        return false;
                    }
                }
            }

            tx->commit();
            return true;
        }

        // ===========================================
        // Transaction management (RAII)
        // ===========================================

        class TxGuard {
          public:
            inline explicit TxGuard(SqliteStore &store) : store_(store), active_(false), committed_(false) {
                if (store_.db_) {
                    active_ = (sqlite3_exec(store_.db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr) == SQLITE_OK);
                }
            }

            inline ~TxGuard() {
                if (active_ && !committed_) {
                    sqlite3_exec(store_.db_, "ROLLBACK", nullptr, nullptr, nullptr);
                }
            }

            TxGuard(const TxGuard &) = delete;
            TxGuard &operator=(const TxGuard &) = delete;

            inline void commit() {
                if (active_ && !committed_) {
                    sqlite3_exec(store_.db_, "COMMIT", nullptr, nullptr, nullptr);
                    committed_ = true;
                    active_ = false;
                }
            }

            inline void rollback() {
                if (active_ && !committed_) {
                    sqlite3_exec(store_.db_, "ROLLBACK", nullptr, nullptr, nullptr);
                    committed_ = true;
                    active_ = false;
                }
            }

          private:
            SqliteStore &store_;
            bool active_;
            bool committed_;
        };

        inline std::unique_ptr<TxGuard> beginTransaction() { return std::make_unique<TxGuard>(*this); }

        // ===========================================
        // Core Ledger Operations
        // ===========================================

        /// Store a block in the ledger
        inline bool storeBlock(int64_t index, const std::string &hash, const std::string &previous_hash,
                               const std::string &merkle_root, int64_t timestamp, int64_t nonce) {
            if (!db_ || !is_open_)
                return false;

            sqlite3_stmt *stmt;
            const char *sql = "INSERT OR REPLACE INTO blocks (height, hash, previous_hash, merkle_root, timestamp, "
                              "nonce, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            sqlite3_bind_int64(stmt, 1, index);
            sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, previous_hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, merkle_root.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 5, timestamp);
            sqlite3_bind_int64(stmt, 6, nonce);
            sqlite3_bind_int64(stmt, 7, currentTimestamp());

            bool success = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);

            return success;
        }

        /// Store a transaction in the ledger
        inline bool storeTransaction(const std::string &tx_id, int64_t block_height, int64_t timestamp,
                                     int16_t priority, const std::vector<uint8_t> &payload) {
            if (!db_ || !is_open_)
                return false;

            sqlite3_stmt *stmt;
            const char *sql = "INSERT OR REPLACE INTO transactions (tx_id, block_height, timestamp, priority, payload, "
                              "created_at) VALUES (?, ?, ?, ?, ?, ?)";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            sqlite3_bind_text(stmt, 1, tx_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, block_height);
            sqlite3_bind_int64(stmt, 3, timestamp);
            sqlite3_bind_int(stmt, 4, priority);
            sqlite3_bind_blob(stmt, 5, payload.data(), static_cast<int>(payload.size()), SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 6, currentTimestamp());

            bool success = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);

            return success;
        }

        /// Create an anchor linking external content to on-chain transaction
        inline bool createAnchor(const std::string &content_id, const std::vector<uint8_t> &content_hash,
                                 const TxRef &tx_ref) {
            if (!db_ || !is_open_)
                return false;

            sqlite3_stmt *stmt;
            const char *sql = "INSERT OR REPLACE INTO anchors (content_id, content_hash, tx_id, "
                              "block_height, merkle_root, anchored_at) VALUES (?, ?, ?, ?, ?, ?)";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            sqlite3_bind_text(stmt, 1, content_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_blob(stmt, 2, content_hash.data(), static_cast<int>(content_hash.size()), SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, tx_ref.tx_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 4, tx_ref.block_height);
            sqlite3_bind_blob(stmt, 5, tx_ref.merkle_root.data(), static_cast<int>(tx_ref.merkle_root.size()),
                              SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 6, currentTimestamp());

            bool success = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);

            return success;
        }

        /// Get anchor by content ID
        inline std::optional<Anchor> getAnchor(const std::string &content_id) {
            if (!db_ || !is_open_)
                return std::nullopt;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT content_id, content_hash, tx_id, block_height, merkle_root, "
                              "anchored_at FROM anchors WHERE content_id = ?";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return std::nullopt;
            }

            sqlite3_bind_text(stmt, 1, content_id.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                Anchor anchor;
                anchor.content_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));

                const void *hash_blob = sqlite3_column_blob(stmt, 1);
                int hash_size = sqlite3_column_bytes(stmt, 1);
                anchor.content_hash.assign(static_cast<const uint8_t *>(hash_blob),
                                           static_cast<const uint8_t *>(hash_blob) + hash_size);

                anchor.tx_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
                anchor.block_height = sqlite3_column_int64(stmt, 3);

                const void *merkle_blob = sqlite3_column_blob(stmt, 4);
                std::memcpy(anchor.merkle_root.data(), merkle_blob, 32);

                anchor.anchored_at = sqlite3_column_int64(stmt, 5);

                sqlite3_finalize(stmt);
                return anchor;
            }

            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        /// Get all anchors for a specific block height
        inline std::vector<Anchor> getAnchorsByBlock(int64_t block_height) {
            std::vector<Anchor> anchors;
            if (!db_ || !is_open_)
                return anchors;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT content_id, content_hash, tx_id, block_height, merkle_root, "
                              "anchored_at FROM anchors WHERE block_height = ? ORDER BY anchored_at";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return anchors;
            }

            sqlite3_bind_int64(stmt, 1, block_height);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Anchor anchor;
                anchor.content_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));

                const void *hash_blob = sqlite3_column_blob(stmt, 1);
                int hash_size = sqlite3_column_bytes(stmt, 1);
                anchor.content_hash.assign(static_cast<const uint8_t *>(hash_blob),
                                           static_cast<const uint8_t *>(hash_blob) + hash_size);

                anchor.tx_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
                anchor.block_height = sqlite3_column_int64(stmt, 3);

                const void *merkle_blob = sqlite3_column_blob(stmt, 4);
                std::memcpy(anchor.merkle_root.data(), merkle_blob, 32);

                anchor.anchored_at = sqlite3_column_int64(stmt, 5);

                anchors.push_back(std::move(anchor));
            }

            sqlite3_finalize(stmt);
            return anchors;
        }

        /// Get all anchors within a height range
        inline std::vector<Anchor> getAnchorsInRange(int64_t min_height, int64_t max_height) {
            std::vector<Anchor> anchors;
            if (!db_ || !is_open_)
                return anchors;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT content_id, content_hash, tx_id, block_height, merkle_root, anchored_at "
                              "FROM anchors WHERE block_height >= ? AND block_height <= ? ORDER BY block_height";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return anchors;
            }

            sqlite3_bind_int64(stmt, 1, min_height);
            sqlite3_bind_int64(stmt, 2, max_height);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Anchor anchor;
                anchor.content_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));

                const void *hash_blob = sqlite3_column_blob(stmt, 1);
                int hash_size = sqlite3_column_bytes(stmt, 1);
                anchor.content_hash.assign(static_cast<const uint8_t *>(hash_blob),
                                           static_cast<const uint8_t *>(hash_blob) + hash_size);

                anchor.tx_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
                anchor.block_height = sqlite3_column_int64(stmt, 3);

                const void *merkle_blob = sqlite3_column_blob(stmt, 4);
                std::memcpy(anchor.merkle_root.data(), merkle_blob, 32);

                anchor.anchored_at = sqlite3_column_int64(stmt, 5);

                anchors.push_back(std::move(anchor));
            }

            sqlite3_finalize(stmt);
            return anchors;
        }

        /// Query transactions with filters
        inline std::vector<std::string> queryTransactions(const LedgerQuery &query) {
            std::vector<std::string> tx_ids;
            if (!db_ || !is_open_)
                return tx_ids;

            std::string sql = "SELECT tx_id FROM transactions WHERE 1=1";

            if (query.block_height_min) {
                sql += " AND block_height >= " + std::to_string(*query.block_height_min);
            }
            if (query.block_height_max) {
                sql += " AND block_height <= " + std::to_string(*query.block_height_max);
            }
            if (query.timestamp_min) {
                sql += " AND timestamp >= " + std::to_string(*query.timestamp_min);
            }
            if (query.timestamp_max) {
                sql += " AND timestamp <= " + std::to_string(*query.timestamp_max);
            }
            if (query.tx_id) {
                sql += " AND tx_id = '" + *query.tx_id + "'";
            }

            sql += " ORDER BY block_height, timestamp LIMIT " + std::to_string(query.limit) + " OFFSET " +
                   std::to_string(query.offset);

            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                return tx_ids;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                tx_ids.push_back(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
            }

            sqlite3_finalize(stmt);
            return tx_ids;
        }

        /// Get block by height
        inline std::optional<std::string> getBlockByHeight(int64_t height) {
            if (!db_ || !is_open_)
                return std::nullopt;

            sqlite3_stmt *stmt;
            const char *sql =
                "SELECT height, hash, previous_hash, merkle_root, timestamp, nonce FROM blocks WHERE height = ?";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return std::nullopt;
            }

            sqlite3_bind_int64(stmt, 1, height);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string json = "{";
                json += "\"height\":" + std::to_string(sqlite3_column_int64(stmt, 0)) + ",";
                json +=
                    "\"hash\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))) + "\",";
                json += "\"previous_hash\":\"" +
                        std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))) + "\",";
                json += "\"merkle_root\":\"" +
                        std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3))) + "\",";
                json += "\"timestamp\":" + std::to_string(sqlite3_column_int64(stmt, 4)) + ",";
                json += "\"nonce\":" + std::to_string(sqlite3_column_int64(stmt, 5));
                json += "}";

                sqlite3_finalize(stmt);
                return json;
            }

            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        /// Get block by hash
        inline std::optional<std::string> getBlockByHash(const std::string &hash) {
            if (!db_ || !is_open_)
                return std::nullopt;

            sqlite3_stmt *stmt;
            const char *sql =
                "SELECT height, hash, previous_hash, merkle_root, timestamp, nonce FROM blocks WHERE hash = ?";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return std::nullopt;
            }

            sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string json = "{";
                json += "\"height\":" + std::to_string(sqlite3_column_int64(stmt, 0)) + ",";
                json +=
                    "\"hash\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))) + "\",";
                json += "\"previous_hash\":\"" +
                        std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))) + "\",";
                json += "\"merkle_root\":\"" +
                        std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3))) + "\",";
                json += "\"timestamp\":" + std::to_string(sqlite3_column_int64(stmt, 4)) + ",";
                json += "\"nonce\":" + std::to_string(sqlite3_column_int64(stmt, 5));
                json += "}";

                sqlite3_finalize(stmt);
                return json;
            }

            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        /// Get transaction by ID
        inline std::optional<std::vector<uint8_t>> getTransaction(const std::string &tx_id) {
            if (!db_ || !is_open_)
                return std::nullopt;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT payload FROM transactions WHERE tx_id = ?";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return std::nullopt;
            }

            sqlite3_bind_text(stmt, 1, tx_id.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const void *blob = sqlite3_column_blob(stmt, 0);
                int size = sqlite3_column_bytes(stmt, 0);
                std::vector<uint8_t> payload(static_cast<const uint8_t *>(blob),
                                             static_cast<const uint8_t *>(blob) + size);

                sqlite3_finalize(stmt);
                return payload;
            }

            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        // ===========================================
        // Verification & Integrity
        // ===========================================

        /// Verify anchor integrity by recomputing hash
        inline bool verifyAnchor(const std::string &content_id, const std::vector<uint8_t> &current_content) {
            auto anchor = getAnchor(content_id);
            if (!anchor)
                return false;

            try {
                auto computed_hash = computeSHA256(current_content);
                return computed_hash == anchor->content_hash;
            } catch (const std::exception &) {
                return false;
            }
        }

        /// Check if chain is continuous (no gaps in block heights)
        inline bool verifyChainContinuity() {
            if (!db_ || !is_open_)
                return false;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT MIN(height), MAX(height), COUNT(*) FROM blocks";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int64_t min_height = sqlite3_column_int64(stmt, 0);
                int64_t max_height = sqlite3_column_int64(stmt, 1);
                int64_t count = sqlite3_column_int64(stmt, 2);

                sqlite3_finalize(stmt);

                // Check if count matches expected range
                return (max_height - min_height + 1) == count;
            }

            sqlite3_finalize(stmt);
            return false;
        }

        // ===========================================
        // Statistics & Diagnostics
        // ===========================================

        /// Get total number of blocks stored
        inline int64_t getBlockCount() {
            if (!db_ || !is_open_)
                return 0;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT COUNT(*) FROM blocks";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return 0;
            }

            int64_t count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int64(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return count;
        }

        /// Get total number of transactions stored
        inline int64_t getTransactionCount() {
            if (!db_ || !is_open_)
                return 0;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT COUNT(*) FROM transactions";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return 0;
            }

            int64_t count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int64(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return count;
        }

        /// Get total number of anchors
        inline int64_t getAnchorCount() {
            if (!db_ || !is_open_)
                return 0;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT COUNT(*) FROM anchors";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return 0;
            }

            int64_t count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int64(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return count;
        }

        /// Get latest block height
        inline int64_t getLatestBlockHeight() {
            if (!db_ || !is_open_)
                return -1;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT MAX(height) FROM blocks";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return -1;
            }

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) {
                    sqlite3_finalize(stmt);
                    return -1; // No blocks in database
                }
                int64_t height = sqlite3_column_int64(stmt, 0);
                sqlite3_finalize(stmt);
                return height;
            }

            sqlite3_finalize(stmt);
            return -1;
        }

        /// Run SQLite integrity check
        inline bool quickCheck() {
            if (!db_ || !is_open_)
                return false;

            sqlite3_stmt *stmt;
            const char *sql = "PRAGMA quick_check";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            bool ok = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string result = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                ok = (result == "ok");
            }

            sqlite3_finalize(stmt);
            return ok;
        }

        // ===========================================
        // Raw SQL access for extensions
        // ===========================================

        /// Execute raw SQL statement (for user extensions)
        inline bool executeSql(const std::string &sql) {
            if (!db_ || !is_open_)
                return false;

            char *errmsg = nullptr;
            int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);

            if (rc != SQLITE_OK) {
                if (errmsg) {
                    sqlite3_free(errmsg);
                }
                return false;
            }

            return true;
        }

        /// Execute prepared statement with parameters
        inline int64_t executeUpdate(const std::string &sql, const std::vector<std::string> &params) {
            if (!db_ || !is_open_)
                return -1;

            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                return -1;
            }

            for (size_t i = 0; i < params.size(); ++i) {
                sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
            }

            int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            return (rc == SQLITE_DONE) ? sqlite3_changes(db_) : -1;
        }

        /// Query with result callback
        inline bool executeQuery(const std::string &sql,
                                 std::function<void(const std::vector<std::string> &row)> callback) {
            if (!db_ || !is_open_ || !callback)
                return false;

            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::vector<std::string> row;
                int col_count = sqlite3_column_count(stmt);
                for (int i = 0; i < col_count; ++i) {
                    const unsigned char *text = sqlite3_column_text(stmt, i);
                    row.push_back(text ? reinterpret_cast<const char *>(text) : "");
                }
                callback(row);
            }

            sqlite3_finalize(stmt);
            return true;
        }

      private:
        sqlite3 *db_;
        std::string db_path_;
        bool is_open_;

        // Core schema creation
        inline void applyPragmas(const OpenOptions &opts) {
            if (!db_)
                return;

            if (opts.enable_wal) {
                sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
            }

            if (opts.enable_foreign_keys) {
                sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
            }

            std::string busy_timeout = "PRAGMA busy_timeout=" + std::to_string(opts.busy_timeout_ms) + ";";
            sqlite3_exec(db_, busy_timeout.c_str(), nullptr, nullptr, nullptr);

            std::string cache_size = "PRAGMA cache_size=-" + std::to_string(opts.cache_size_kb) + ";";
            sqlite3_exec(db_, cache_size.c_str(), nullptr, nullptr, nullptr);

            std::string sync_mode;
            switch (opts.sync_mode) {
            case OpenOptions::Synchronous::OFF:
                sync_mode = "PRAGMA synchronous=OFF;";
                break;
            case OpenOptions::Synchronous::NORMAL:
                sync_mode = "PRAGMA synchronous=NORMAL;";
                break;
            case OpenOptions::Synchronous::FULL:
                sync_mode = "PRAGMA synchronous=FULL;";
                break;
            }
            sqlite3_exec(db_, sync_mode.c_str(), nullptr, nullptr, nullptr);
        }

        inline bool createCoreSchemaV1() {
            return executeSql(BLOCKS_TABLE) && executeSql(TRANSACTIONS_TABLE) && executeSql(ANCHORS_TABLE) &&
                   executeSql(IDX_BLOCKS_TIMESTAMP) && executeSql(IDX_BLOCKS_HASH) && executeSql(IDX_TX_BLOCK_HEIGHT) &&
                   executeSql(IDX_TX_TIMESTAMP) && executeSql(IDX_ANCHORS_BLOCK) && executeSql(IDX_ANCHORS_TX);
        }

        inline bool tableExists(const std::string &table_name) {
            if (!db_)
                return false;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_TRANSIENT);

            bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
            sqlite3_finalize(stmt);

            return exists;
        }

        inline int32_t getCurrentSchemaVersion() {
            if (!tableExists("schema_migrations"))
                return 0;

            sqlite3_stmt *stmt;
            const char *sql = "SELECT MAX(version) FROM schema_migrations";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return 0;
            }

            int32_t version = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                version = sqlite3_column_int(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return version;
        }

        inline bool setSchemaVersion(int32_t version) {
            sqlite3_stmt *stmt;
            const char *sql = "INSERT OR REPLACE INTO schema_migrations (version, applied_at) VALUES (?, ?)";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                return false;
            }

            sqlite3_bind_int(stmt, 1, version);
            sqlite3_bind_int64(stmt, 2, currentTimestamp());

            bool success = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);

            return success;
        }

        // Core schema SQL definitions (inline, no separate files)
        static constexpr const char *SCHEMA_MIGRATIONS_TABLE = R"(
            CREATE TABLE IF NOT EXISTS schema_migrations (
                version INTEGER PRIMARY KEY,
                applied_at INTEGER NOT NULL
            )
        )";

        static constexpr const char *BLOCKS_TABLE = R"(
            CREATE TABLE IF NOT EXISTS blocks (
                height INTEGER PRIMARY KEY,
                hash TEXT NOT NULL UNIQUE,
                previous_hash TEXT NOT NULL,
                merkle_root TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                nonce INTEGER NOT NULL,
                created_at INTEGER NOT NULL
            )
        )";

        static constexpr const char *TRANSACTIONS_TABLE = R"(
            CREATE TABLE IF NOT EXISTS transactions (
                tx_id TEXT PRIMARY KEY,
                block_height INTEGER NOT NULL,
                timestamp INTEGER NOT NULL,
                priority INTEGER NOT NULL,
                payload BLOB NOT NULL,
                created_at INTEGER NOT NULL,
                FOREIGN KEY(block_height) REFERENCES blocks(height) ON DELETE CASCADE
            )
        )";

        static constexpr const char *ANCHORS_TABLE = R"(
            CREATE TABLE IF NOT EXISTS anchors (
                content_id TEXT PRIMARY KEY,
                content_hash BLOB NOT NULL,
                tx_id TEXT NOT NULL,
                block_height INTEGER NOT NULL,
                merkle_root BLOB NOT NULL,
                anchored_at INTEGER NOT NULL,
                FOREIGN KEY(tx_id) REFERENCES transactions(tx_id) ON DELETE CASCADE,
                FOREIGN KEY(block_height) REFERENCES blocks(height) ON DELETE CASCADE
            )
        )";

        static constexpr const char *IDX_BLOCKS_TIMESTAMP =
            "CREATE INDEX IF NOT EXISTS idx_blocks_timestamp ON blocks(timestamp)";
        static constexpr const char *IDX_BLOCKS_HASH = "CREATE INDEX IF NOT EXISTS idx_blocks_hash ON blocks(hash)";
        static constexpr const char *IDX_TX_BLOCK_HEIGHT =
            "CREATE INDEX IF NOT EXISTS idx_tx_block_height ON transactions(block_height)";
        static constexpr const char *IDX_TX_TIMESTAMP =
            "CREATE INDEX IF NOT EXISTS idx_tx_timestamp ON transactions(timestamp)";
        static constexpr const char *IDX_ANCHORS_BLOCK =
            "CREATE INDEX IF NOT EXISTS idx_anchors_block ON anchors(block_height)";
        static constexpr const char *IDX_ANCHORS_TX = "CREATE INDEX IF NOT EXISTS idx_anchors_tx ON anchors(tx_id)";
    };

} // namespace blockit::storage
