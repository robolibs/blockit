#pragma once

#include "sqlite_store.hpp"
#include <chrono>
#include <cstring>
#include <lockey/lockey.hpp>
#include <sqlite3.h>
#include <stdexcept>

namespace blockit::storage {

    // ===========================================
    // Utility functions implementation
    // ===========================================

    inline int64_t currentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    inline std::vector<uint8_t> computeSHA256(const std::vector<uint8_t> &data) {
        lockey::Lockey crypto(lockey::Lockey::Algorithm::XChaCha20_Poly1305, lockey::Lockey::HashAlgorithm::SHA256);
        auto result = crypto.hash(data);
        if (!result.success) {
            throw std::runtime_error("SHA256 hashing failed");
        }
        return result.data;
    }

    inline std::string hashToHex(const std::vector<uint8_t> &hash) { return lockey::Lockey::to_hex(hash); }

    inline std::vector<uint8_t> hexToHash(const std::string &hex) { return lockey::Lockey::from_hex(hex); }

    // ===========================================
    // SqliteStore implementation
    // ===========================================

    inline SqliteStore::SqliteStore() : db_(nullptr), is_open_(false) {}

    inline SqliteStore::~SqliteStore() { close(); }

    inline SqliteStore::SqliteStore(SqliteStore &&other) noexcept
        : db_(other.db_), db_path_(std::move(other.db_path_)), is_open_(other.is_open_) {
        other.db_ = nullptr;
        other.is_open_ = false;
    }

    inline SqliteStore &SqliteStore::operator=(SqliteStore &&other) noexcept {
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

    inline bool SqliteStore::open(const std::string &path, const OpenOptions &opts) {
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

    inline void SqliteStore::close() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
            is_open_ = false;
        }
    }

    inline bool SqliteStore::isOpen() const { return is_open_; }

    inline void SqliteStore::applyPragmas(const OpenOptions &opts) {
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

    inline bool SqliteStore::initializeCoreSchema() {
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

    inline bool SqliteStore::createCoreSchemaV1() {
        return executeSql(BLOCKS_TABLE) && executeSql(TRANSACTIONS_TABLE) && executeSql(ANCHORS_TABLE) &&
               executeSql(IDX_BLOCKS_TIMESTAMP) && executeSql(IDX_BLOCKS_HASH) && executeSql(IDX_TX_BLOCK_HEIGHT) &&
               executeSql(IDX_TX_TIMESTAMP) && executeSql(IDX_ANCHORS_BLOCK) && executeSql(IDX_ANCHORS_TX);
    }

    inline bool SqliteStore::registerExtension(const ISchemaExtension &extension) {
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

    inline bool SqliteStore::tableExists(const std::string &table_name) {
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

    inline int32_t SqliteStore::getCurrentSchemaVersion() {
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

    inline bool SqliteStore::setSchemaVersion(int32_t version) {
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

    // ===========================================
    // Transaction Guard
    // ===========================================

    inline SqliteStore::TxGuard::TxGuard(SqliteStore &store) : store_(store), active_(false), committed_(false) {
        if (store_.db_) {
            active_ = (sqlite3_exec(store_.db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr) == SQLITE_OK);
        }
    }

    inline SqliteStore::TxGuard::~TxGuard() {
        if (active_ && !committed_) {
            sqlite3_exec(store_.db_, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    inline void SqliteStore::TxGuard::commit() {
        if (active_ && !committed_) {
            sqlite3_exec(store_.db_, "COMMIT", nullptr, nullptr, nullptr);
            committed_ = true;
            active_ = false;
        }
    }

    inline void SqliteStore::TxGuard::rollback() {
        if (active_ && !committed_) {
            sqlite3_exec(store_.db_, "ROLLBACK", nullptr, nullptr, nullptr);
            committed_ = true;
            active_ = false;
        }
    }

    inline std::unique_ptr<SqliteStore::TxGuard> SqliteStore::beginTransaction() {
        return std::make_unique<TxGuard>(*this);
    }

    // ===========================================
    // Core Ledger Operations
    // ===========================================

    inline bool SqliteStore::storeBlock(int64_t index, const std::string &hash, const std::string &previous_hash,
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

    inline bool SqliteStore::storeTransaction(const std::string &tx_id, int64_t block_height, int64_t timestamp,
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

    inline bool SqliteStore::createAnchor(const std::string &content_id, const std::vector<uint8_t> &content_hash,
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

    inline std::optional<Anchor> SqliteStore::getAnchor(const std::string &content_id) {
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

    inline std::vector<Anchor> SqliteStore::getAnchorsByBlock(int64_t block_height) {
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

    inline std::vector<Anchor> SqliteStore::getAnchorsInRange(int64_t min_height, int64_t max_height) {
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

    inline std::vector<std::string> SqliteStore::queryTransactions(const LedgerQuery &query) {
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

    inline std::optional<std::string> SqliteStore::getBlockByHeight(int64_t height) {
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
            json += "\"hash\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))) + "\",";
            json += "\"previous_hash\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))) +
                    "\",";
            json += "\"merkle_root\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3))) +
                    "\",";
            json += "\"timestamp\":" + std::to_string(sqlite3_column_int64(stmt, 4)) + ",";
            json += "\"nonce\":" + std::to_string(sqlite3_column_int64(stmt, 5));
            json += "}";

            sqlite3_finalize(stmt);
            return json;
        }

        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    inline std::optional<std::string> SqliteStore::getBlockByHash(const std::string &hash) {
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
            json += "\"hash\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))) + "\",";
            json += "\"previous_hash\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))) +
                    "\",";
            json += "\"merkle_root\":\"" + std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3))) +
                    "\",";
            json += "\"timestamp\":" + std::to_string(sqlite3_column_int64(stmt, 4)) + ",";
            json += "\"nonce\":" + std::to_string(sqlite3_column_int64(stmt, 5));
            json += "}";

            sqlite3_finalize(stmt);
            return json;
        }

        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    inline std::optional<std::vector<uint8_t>> SqliteStore::getTransaction(const std::string &tx_id) {
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
            std::vector<uint8_t> payload(static_cast<const uint8_t *>(blob), static_cast<const uint8_t *>(blob) + size);

            sqlite3_finalize(stmt);
            return payload;
        }

        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    // ===========================================
    // Verification
    // ===========================================

    inline bool SqliteStore::verifyAnchor(const std::string &content_id, const std::vector<uint8_t> &current_content) {
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

    inline bool SqliteStore::verifyChainContinuity() {
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
    // Statistics
    // ===========================================

    inline int64_t SqliteStore::getBlockCount() {
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

    inline int64_t SqliteStore::getTransactionCount() {
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

    inline int64_t SqliteStore::getAnchorCount() {
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

    inline int64_t SqliteStore::getLatestBlockHeight() {
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

    inline bool SqliteStore::quickCheck() {
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
    // Raw SQL access
    // ===========================================

    inline bool SqliteStore::executeSql(const std::string &sql) {
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

    inline int64_t SqliteStore::executeUpdate(const std::string &sql, const std::vector<std::string> &params) {
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

    inline bool SqliteStore::executeQuery(const std::string &sql,
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

} // namespace blockit::storage
