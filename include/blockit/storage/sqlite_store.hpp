#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declaration for sqlite3 C API
struct sqlite3;
struct sqlite3_stmt;

namespace blockit::storage {

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
        SqliteStore();
        ~SqliteStore();

        // Non-copyable, movable
        SqliteStore(const SqliteStore &) = delete;
        SqliteStore &operator=(const SqliteStore &) = delete;
        SqliteStore(SqliteStore &&) noexcept;
        SqliteStore &operator=(SqliteStore &&) noexcept;

        /// Open or create database at given path
        /// @param path Database file path (e.g. "data/blockit.db")
        /// @param opts Configuration options
        /// @return true on success, false on failure
        bool open(const std::string &path, const OpenOptions &opts = OpenOptions{});

        /// Close database connection
        void close();

        /// Check if database is open
        bool isOpen() const;

        /// Initialize core ledger schema and run migrations
        /// @return true on success, false on failure
        bool initializeCoreSchema();

        /// Register and initialize user's schema extension
        /// Call this after open() but can be called multiple times for multiple extensions
        /// @param extension User's schema extension
        /// @return true on success, false on failure
        bool registerExtension(const ISchemaExtension &extension);

        // ===========================================
        // Transaction management (RAII)
        // ===========================================

        class TxGuard {
          public:
            explicit TxGuard(SqliteStore &store);
            ~TxGuard();

            TxGuard(const TxGuard &) = delete;
            TxGuard &operator=(const TxGuard &) = delete;

            void commit();
            void rollback();

          private:
            SqliteStore &store_;
            bool active_;
            bool committed_;
        };

        std::unique_ptr<TxGuard> beginTransaction();

        // ===========================================
        // Core Ledger Operations
        // ===========================================

        /// Store a block in the ledger
        /// @param index Block index
        /// @param hash Block hash (hex string)
        /// @param previous_hash Previous block hash
        /// @param merkle_root Merkle root (hex string)
        /// @param timestamp Block timestamp (Unix epoch seconds)
        /// @param nonce Proof-of-work nonce
        /// @return true on success, false on failure
        bool storeBlock(int64_t index, const std::string &hash, const std::string &previous_hash,
                        const std::string &merkle_root, int64_t timestamp, int64_t nonce);

        /// Store a transaction in the ledger
        /// @param tx_id Transaction ID (uuid)
        /// @param block_height Block height where tx is included
        /// @param timestamp Transaction timestamp
        /// @param priority Transaction priority
        /// @param payload Transaction payload (serialized)
        /// @return true on success, false on failure
        bool storeTransaction(const std::string &tx_id, int64_t block_height, int64_t timestamp, int16_t priority,
                              const std::vector<uint8_t> &payload);

        /// Create an anchor linking external content to on-chain transaction
        /// @param content_id User's content identifier
        /// @param content_hash SHA256 hash of content
        /// @param tx_ref Transaction reference (tx_id, block_height, merkle_root)
        /// @return true on success, false on failure
        bool createAnchor(const std::string &content_id, const std::vector<uint8_t> &content_hash, const TxRef &tx_ref);

        /// Get anchor by content ID
        /// @param content_id Content identifier
        /// @return Anchor if found, nullopt otherwise
        std::optional<Anchor> getAnchor(const std::string &content_id);

        /// Get all anchors for a specific block height
        /// @param block_height Block height
        /// @return Vector of anchors
        std::vector<Anchor> getAnchorsByBlock(int64_t block_height);

        /// Get all anchors within a height range
        /// @param min_height Minimum block height (inclusive)
        /// @param max_height Maximum block height (inclusive)
        /// @return Vector of anchors
        std::vector<Anchor> getAnchorsInRange(int64_t min_height, int64_t max_height);

        /// Query transactions with filters
        /// @param query Filter parameters
        /// @return Vector of transaction IDs matching the query
        std::vector<std::string> queryTransactions(const LedgerQuery &query);

        /// Get block by height
        /// @param height Block height
        /// @return Block data as JSON string if found, nullopt otherwise
        std::optional<std::string> getBlockByHeight(int64_t height);

        /// Get block by hash
        /// @param hash Block hash (hex string)
        /// @return Block data as JSON string if found, nullopt otherwise
        std::optional<std::string> getBlockByHash(const std::string &hash);

        /// Get transaction by ID
        /// @param tx_id Transaction ID
        /// @return Transaction data if found, nullopt otherwise
        std::optional<std::vector<uint8_t>> getTransaction(const std::string &tx_id);

        // ===========================================
        // Verification & Integrity
        // ===========================================

        /// Verify anchor integrity by recomputing hash
        /// User provides the current content to verify against stored hash
        /// @param content_id Content identifier
        /// @param current_content Current content bytes to verify
        /// @return true if hash matches anchor, false otherwise
        bool verifyAnchor(const std::string &content_id, const std::vector<uint8_t> &current_content);

        /// Check if chain is continuous (no gaps in block heights)
        /// @return true if chain is valid, false if gaps detected
        bool verifyChainContinuity();

        // ===========================================
        // Statistics & Diagnostics
        // ===========================================

        /// Get total number of blocks stored
        int64_t getBlockCount();

        /// Get total number of transactions stored
        int64_t getTransactionCount();

        /// Get total number of anchors
        int64_t getAnchorCount();

        /// Get latest block height
        int64_t getLatestBlockHeight();

        /// Run SQLite integrity check
        /// @return true if database is healthy, false on corruption
        bool quickCheck();

        // ===========================================
        // Raw SQL access for extensions
        // ===========================================

        /// Execute raw SQL statement (for user extensions)
        /// @param sql SQL statement
        /// @return true on success, false on failure
        bool executeSql(const std::string &sql);

        /// Execute prepared statement with parameters
        /// @param sql SQL with placeholders (?)
        /// @param params Parameters to bind
        /// @return Number of affected rows, -1 on error
        int64_t executeUpdate(const std::string &sql, const std::vector<std::string> &params);

        /// Query with result callback
        /// @param sql SQL query
        /// @param callback Called for each row with column values
        /// @return true on success, false on error
        bool executeQuery(const std::string &sql, std::function<void(const std::vector<std::string> &row)> callback);

      private:
        sqlite3 *db_;
        std::string db_path_;
        bool is_open_;

        // Core schema creation
        void applyPragmas(const OpenOptions &opts);
        bool createCoreSchemaV1();
        bool tableExists(const std::string &table_name);
        int32_t getCurrentSchemaVersion();
        bool setSchemaVersion(int32_t version);

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

    // ===========================================
    // Utility functions
    // ===========================================

    /// Compute SHA256 hash using Lockey
    /// @param data Bytes to hash
    /// @return 32-byte SHA256 hash
    std::vector<uint8_t> computeSHA256(const std::vector<uint8_t> &data);

    /// Convert hash bytes to hex string
    /// @param hash Hash bytes
    /// @return Hex string representation
    std::string hashToHex(const std::vector<uint8_t> &hash);

    /// Convert hex string to hash bytes
    /// @param hex Hex string
    /// @return Hash bytes
    std::vector<uint8_t> hexToHash(const std::string &hex);

    /// Get current Unix timestamp in seconds
    /// @return Current timestamp
    int64_t currentTimestamp();

} // namespace blockit::storage
