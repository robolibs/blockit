#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <blockit/storage/sqlite_store.hpp>
#include <filesystem>
#include <thread>

using namespace blockit::storage;

// Test helper: cleanup database file
struct TestDB {
    std::string path;
    SqliteStore store;

    explicit TestDB(const std::string &name) : path(name + ".db") { cleanup(); }

    ~TestDB() {
        store.close();
        cleanup();
    }

    void cleanup() {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
        if (std::filesystem::exists(path + "-wal")) {
            std::filesystem::remove(path + "-wal");
        }
        if (std::filesystem::exists(path + "-shm")) {
            std::filesystem::remove(path + "-shm");
        }
    }
};

// ===========================================
// Utility function tests
// ===========================================

TEST_CASE("Utility functions") {
    SUBCASE("SHA256 hashing") {
        std::vector<uint8_t> data = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
        auto hash = computeSHA256(data);

        CHECK(hash.size() == 32);

        // Hash should be deterministic
        auto hash2 = computeSHA256(data);
        CHECK(hash == hash2);

        // Different data should produce different hash
        std::vector<uint8_t> data2 = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21}; // "Hello!"
        auto hash3 = computeSHA256(data2);
        CHECK(hash != hash3);
    }

    SUBCASE("Hex conversion") {
        std::vector<uint8_t> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
        std::string hex = hashToHex(bytes);

        CHECK(hex.length() > 0);

        auto bytes2 = hexToHash(hex);
        CHECK(bytes == bytes2);
    }

    SUBCASE("Timestamp") {
        int64_t ts1 = currentTimestamp();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int64_t ts2 = currentTimestamp();

        CHECK(ts2 >= ts1);
        CHECK(ts2 - ts1 <= 1); // Should be within 1 second
    }
}

// ===========================================
// Core database operations
// ===========================================

TEST_CASE("Database lifecycle") {
    TestDB test_db("test_lifecycle");

    SUBCASE("Open and close") {
        CHECK(test_db.store.open(test_db.path));
        CHECK(test_db.store.isOpen());

        test_db.store.close();
        CHECK_FALSE(test_db.store.isOpen());
    }

    SUBCASE("Open with options") {
        OpenOptions opts;
        opts.enable_wal = true;
        opts.enable_foreign_keys = true;
        opts.sync_mode = OpenOptions::Synchronous::NORMAL;
        opts.busy_timeout_ms = 3000;

        CHECK(test_db.store.open(test_db.path, opts));
        CHECK(test_db.store.isOpen());
    }

    SUBCASE("Initialize core schema") {
        CHECK(test_db.store.open(test_db.path));
        CHECK(test_db.store.initializeCoreSchema());

        // Should be idempotent
        CHECK(test_db.store.initializeCoreSchema());
    }
}

// ===========================================
// Block storage tests
// ===========================================

TEST_CASE("Block storage") {
    TestDB test_db("test_blocks");
    REQUIRE(test_db.store.open(test_db.path));
    REQUIRE(test_db.store.initializeCoreSchema());

    SUBCASE("Store and retrieve block") {
        bool stored = test_db.store.storeBlock(0, "hash_000", "GENESIS", "merkle_000", currentTimestamp(), 0);
        CHECK(stored);

        auto block = test_db.store.getBlockByHeight(0);
        CHECK(block.has_value());
        CHECK(block->find("hash_000") != std::string::npos);
        CHECK(block->find("GENESIS") != std::string::npos);
    }

    SUBCASE("Store multiple blocks") {
        for (int64_t i = 0; i < 10; ++i) {
            std::string hash = "hash_" + std::to_string(i);
            std::string prev = (i == 0) ? "GENESIS" : "hash_" + std::to_string(i - 1);
            std::string merkle = "merkle_" + std::to_string(i);

            CHECK(test_db.store.storeBlock(i, hash, prev, merkle, currentTimestamp(), i * 1000));
        }

        CHECK(test_db.store.getBlockCount() == 10);
        CHECK(test_db.store.getLatestBlockHeight() == 9);
    }

    SUBCASE("Get block by hash") {
        test_db.store.storeBlock(5, "unique_hash", "prev", "merkle", currentTimestamp(), 0);

        auto block = test_db.store.getBlockByHash("unique_hash");
        CHECK(block.has_value());
        CHECK(block->find("\"height\":5") != std::string::npos);
    }

    SUBCASE("Verify chain continuity") {
        // Continuous chain
        for (int64_t i = 0; i < 5; ++i) {
            test_db.store.storeBlock(i, "hash_" + std::to_string(i), "prev", "merkle", currentTimestamp(), 0);
        }
        CHECK(test_db.store.verifyChainContinuity());

        // Add gap - store block 10 when we only have 0-4
        test_db.store.storeBlock(10, "hash_10", "prev", "merkle", currentTimestamp(), 0);
        CHECK_FALSE(test_db.store.verifyChainContinuity());
    }
}

// ===========================================
// Transaction storage tests
// ===========================================

TEST_CASE("Transaction storage") {
    TestDB test_db("test_txs");
    REQUIRE(test_db.store.open(test_db.path));
    REQUIRE(test_db.store.initializeCoreSchema());

    // Create a block first (FK constraint)
    REQUIRE(test_db.store.storeBlock(1, "block_1", "GENESIS", "merkle", currentTimestamp(), 0));

    SUBCASE("Store and retrieve transaction") {
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
        bool stored = test_db.store.storeTransaction("tx_001", 1, currentTimestamp(), 100, payload);
        CHECK(stored);

        auto retrieved = test_db.store.getTransaction("tx_001");
        CHECK(retrieved.has_value());
        CHECK(*retrieved == payload);
    }

    SUBCASE("Store multiple transactions") {
        for (int i = 0; i < 20; ++i) {
            std::string tx_id = "tx_" + std::to_string(i);
            std::vector<uint8_t> payload = {static_cast<uint8_t>(i)};
            CHECK(test_db.store.storeTransaction(tx_id, 1, currentTimestamp(), 100, payload));
        }

        CHECK(test_db.store.getTransactionCount() == 20);
    }

    SUBCASE("Query transactions") {
        // Store transactions across multiple blocks
        test_db.store.storeBlock(2, "block_2", "block_1", "merkle", currentTimestamp(), 0);
        test_db.store.storeBlock(3, "block_3", "block_2", "merkle", currentTimestamp(), 0);

        std::vector<uint8_t> payload = {0x01};
        test_db.store.storeTransaction("tx_1_1", 1, 1000, 100, payload);
        test_db.store.storeTransaction("tx_2_1", 2, 2000, 100, payload);
        test_db.store.storeTransaction("tx_3_1", 3, 3000, 100, payload);

        LedgerQuery query;
        query.block_height_min = 2;
        query.block_height_max = 3;

        auto results = test_db.store.queryTransactions(query);
        CHECK(results.size() == 2);
    }
}

// ===========================================
// Anchor tests
// ===========================================

TEST_CASE("Anchoring") {
    TestDB test_db("test_anchors");
    REQUIRE(test_db.store.open(test_db.path));
    REQUIRE(test_db.store.initializeCoreSchema());

    // Setup: create block and transaction
    REQUIRE(test_db.store.storeBlock(1, "block_1", "GENESIS", "merkle_root", currentTimestamp(), 0));
    std::vector<uint8_t> tx_payload = {0x01};
    REQUIRE(test_db.store.storeTransaction("tx_001", 1, currentTimestamp(), 100, tx_payload));

    SUBCASE("Create and retrieve anchor") {
        std::vector<uint8_t> content = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
        auto content_hash = computeSHA256(content);

        std::array<uint8_t, 32> merkle_root = {0};
        TxRef tx_ref("tx_001", 1, merkle_root);

        bool created = test_db.store.createAnchor("doc_001", content_hash, tx_ref);
        CHECK(created);

        auto anchor = test_db.store.getAnchor("doc_001");
        CHECK(anchor.has_value());
        CHECK(anchor->content_id == "doc_001");
        CHECK(anchor->content_hash == content_hash);
        CHECK(anchor->tx_id == "tx_001");
        CHECK(anchor->block_height == 1);
    }

    SUBCASE("Verify anchor integrity") {
        std::vector<uint8_t> original_content = {0x41, 0x42, 0x43}; // "ABC"
        auto content_hash = computeSHA256(original_content);

        std::array<uint8_t, 32> merkle_root = {0};
        TxRef tx_ref("tx_001", 1, merkle_root);

        test_db.store.createAnchor("doc_002", content_hash, tx_ref);

        // Verify with same content - should pass
        CHECK(test_db.store.verifyAnchor("doc_002", original_content));

        // Verify with different content - should fail
        std::vector<uint8_t> tampered_content = {0x41, 0x42, 0x43, 0x44}; // "ABCD"
        CHECK_FALSE(test_db.store.verifyAnchor("doc_002", tampered_content));
    }

    SUBCASE("Get anchors by block") {
        std::array<uint8_t, 32> merkle_root = {0};

        for (int i = 0; i < 5; ++i) {
            std::string content_id = "doc_" + std::to_string(i);
            std::vector<uint8_t> content = {static_cast<uint8_t>(i)};
            auto hash = computeSHA256(content);

            TxRef tx_ref("tx_001", 1, merkle_root);
            test_db.store.createAnchor(content_id, hash, tx_ref);
        }

        auto anchors = test_db.store.getAnchorsByBlock(1);
        CHECK(anchors.size() == 5);
    }

    SUBCASE("Get anchors in range") {
        test_db.store.storeBlock(2, "block_2", "block_1", "merkle", currentTimestamp(), 0);
        test_db.store.storeBlock(3, "block_3", "block_2", "merkle", currentTimestamp(), 0);

        std::vector<uint8_t> payload = {0x01};
        test_db.store.storeTransaction("tx_002", 2, currentTimestamp(), 100, payload);
        test_db.store.storeTransaction("tx_003", 3, currentTimestamp(), 100, payload);

        std::array<uint8_t, 32> merkle_root = {0};
        std::vector<uint8_t> content = {0x42};
        auto hash = computeSHA256(content);

        test_db.store.createAnchor("doc_b1", hash, TxRef("tx_001", 1, merkle_root));
        test_db.store.createAnchor("doc_b2", hash, TxRef("tx_002", 2, merkle_root));
        test_db.store.createAnchor("doc_b3", hash, TxRef("tx_003", 3, merkle_root));

        auto anchors = test_db.store.getAnchorsInRange(2, 3);
        CHECK(anchors.size() == 2);
    }
}

// ===========================================
// Schema extension tests
// ===========================================

class TestExtension : public ISchemaExtension {
  public:
    std::vector<std::string> getCreateTableStatements() const override {
        return {
            R"(CREATE TABLE IF NOT EXISTS test_data (
                id TEXT PRIMARY KEY,
                value INTEGER NOT NULL
            ))"};
    }

    std::vector<std::string> getCreateIndexStatements() const override {
        return {"CREATE INDEX IF NOT EXISTS idx_test_value ON test_data(value)"};
    }
};

TEST_CASE("Schema extensions") {
    TestDB test_db("test_extensions");
    REQUIRE(test_db.store.open(test_db.path));
    REQUIRE(test_db.store.initializeCoreSchema());

    SUBCASE("Register extension") {
        TestExtension ext;
        CHECK(test_db.store.registerExtension(ext));

        // Verify table was created
        bool success = test_db.store.executeSql("INSERT INTO test_data (id, value) VALUES ('test1', 42)");
        CHECK(success);

        // Query the data
        std::vector<std::string> result;
        test_db.store.executeQuery("SELECT value FROM test_data WHERE id = 'test1'",
                                   [&](const std::vector<std::string> &row) {
                                       if (!row.empty())
                                           result = row;
                                   });

        CHECK(result.size() == 1);
        CHECK(result[0] == "42");
    }
}

// ===========================================
// Transaction guard tests
// ===========================================

TEST_CASE("Transaction guard") {
    TestDB test_db("test_tx_guard");
    REQUIRE(test_db.store.open(test_db.path));
    REQUIRE(test_db.store.initializeCoreSchema());

    SUBCASE("Commit transaction") {
        {
            auto tx = test_db.store.beginTransaction();
            test_db.store.storeBlock(1, "hash1", "GENESIS", "merkle", currentTimestamp(), 0);
            tx->commit();
        }

        CHECK(test_db.store.getBlockCount() == 1);
    }

    SUBCASE("Rollback on exception") {
        try {
            auto tx = test_db.store.beginTransaction();
            test_db.store.storeBlock(2, "hash2", "GENESIS", "merkle", currentTimestamp(), 0);
            // Don't commit - let it rollback
            throw std::runtime_error("test error");
        } catch (...) {
        }

        CHECK(test_db.store.getBlockCount() == 0);
    }

    SUBCASE("Explicit rollback") {
        {
            auto tx = test_db.store.beginTransaction();
            test_db.store.storeBlock(3, "hash3", "GENESIS", "merkle", currentTimestamp(), 0);
            tx->rollback();
        }

        CHECK(test_db.store.getBlockCount() == 0);
    }
}

// ===========================================
// Diagnostics tests
// ===========================================

TEST_CASE("Diagnostics") {
    TestDB test_db("test_diagnostics");
    REQUIRE(test_db.store.open(test_db.path));
    REQUIRE(test_db.store.initializeCoreSchema());

    SUBCASE("Statistics") {
        CHECK(test_db.store.getBlockCount() == 0);
        CHECK(test_db.store.getTransactionCount() == 0);
        CHECK(test_db.store.getAnchorCount() == 0);
        CHECK(test_db.store.getLatestBlockHeight() == -1);

        test_db.store.storeBlock(0, "hash", "GENESIS", "merkle", currentTimestamp(), 0);
        CHECK(test_db.store.getBlockCount() == 1);
        CHECK(test_db.store.getLatestBlockHeight() == 0);
    }

    SUBCASE("Quick check") { CHECK(test_db.store.quickCheck()); }
}

// ===========================================
// Raw SQL access tests
// ===========================================

TEST_CASE("Raw SQL access") {
    TestDB test_db("test_raw_sql");
    REQUIRE(test_db.store.open(test_db.path));
    REQUIRE(test_db.store.initializeCoreSchema());

    SUBCASE("Execute SQL") {
        bool success =
            test_db.store.executeSql("CREATE TABLE IF NOT EXISTS custom_table (id INTEGER PRIMARY KEY, name TEXT)");
        CHECK(success);

        success = test_db.store.executeSql("INSERT INTO custom_table (id, name) VALUES (1, 'test')");
        CHECK(success);
    }

    SUBCASE("Execute query with callback") {
        test_db.store.executeSql("CREATE TABLE IF NOT EXISTS items (id INTEGER, value TEXT)");
        test_db.store.executeSql("INSERT INTO items VALUES (1, 'a'), (2, 'b'), (3, 'c')");

        std::vector<std::vector<std::string>> rows;
        test_db.store.executeQuery("SELECT * FROM items ORDER BY id",
                                   [&](const std::vector<std::string> &row) { rows.push_back(row); });

        CHECK(rows.size() == 3);
        CHECK(rows[0][1] == "a");
        CHECK(rows[1][1] == "b");
        CHECK(rows[2][1] == "c");
    }
}
