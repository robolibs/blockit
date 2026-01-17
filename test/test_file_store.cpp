#include <doctest/doctest.h>

#include <blockit/storage/file_store.hpp>
#include <filesystem>
#include <thread>

using namespace blockit::storage;
using namespace datapod;

// Test helper: cleanup storage directory
struct TestStore {
    std::string path;
    FileStore store;

    explicit TestStore(const std::string &name) : path(name + "_store") { cleanup(); }

    ~TestStore() {
        store.close();
        cleanup();
    }

    void cleanup() {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove_all(path);
        }
    }
};

// ===========================================
// Utility function tests
// ===========================================

TEST_CASE("Utility functions") {
    SUBCASE("SHA256 hashing") {
        Vector<u8> data = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
        auto hash = computeSHA256(data);

        CHECK(hash.size() == 32);

        // Hash should be deterministic
        auto hash2 = computeSHA256(data);
        CHECK(hash.size() == hash2.size());
        bool equal = true;
        for (usize i = 0; i < hash.size(); ++i) {
            if (hash[i] != hash2[i])
                equal = false;
        }
        CHECK(equal);

        // Different data should produce different hash
        Vector<u8> data2 = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21}; // "Hello!"
        auto hash3 = computeSHA256(data2);
        bool different = false;
        for (usize i = 0; i < hash.size(); ++i) {
            if (hash[i] != hash3[i])
                different = true;
        }
        CHECK(different);
    }

    SUBCASE("Hex conversion") {
        Vector<u8> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
        String hex = hashToHex(bytes);

        CHECK(hex.size() > 0);

        auto bytes2 = hexToHash(hex);
        CHECK(bytes.size() == bytes2.size());
        bool equal = true;
        for (usize i = 0; i < bytes.size(); ++i) {
            if (bytes[i] != bytes2[i])
                equal = false;
        }
        CHECK(equal);
    }

    SUBCASE("Timestamp") {
        i64 ts1 = currentTimestamp();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        i64 ts2 = currentTimestamp();

        CHECK(ts2 >= ts1);
        CHECK(ts2 - ts1 <= 1); // Should be within 1 second
    }
}

// ===========================================
// Core storage operations
// ===========================================

TEST_CASE("Storage lifecycle") {
    TestStore test_store("test_lifecycle");

    SUBCASE("Open and close") {
        CHECK(test_store.store.open(String(test_store.path.c_str())).is_ok());
        CHECK(test_store.store.isOpen());

        test_store.store.close();
        CHECK_FALSE(test_store.store.isOpen());
    }

    SUBCASE("Open with options") {
        OpenOptions opts;
        opts.sync_mode = OpenOptions::Synchronous::NORMAL;

        CHECK(test_store.store.open(String(test_store.path.c_str()), opts).is_ok());
        CHECK(test_store.store.isOpen());
    }

    SUBCASE("Initialize core schema") {
        CHECK(test_store.store.open(String(test_store.path.c_str())).is_ok());
        CHECK(test_store.store.initializeCoreSchema().is_ok());

        // Should be idempotent
        CHECK(test_store.store.initializeCoreSchema().is_ok());
    }
}

// ===========================================
// Block storage tests
// ===========================================

TEST_CASE("Block storage") {
    TestStore test_store("test_blocks");
    REQUIRE(test_store.store.open(String(test_store.path.c_str())).is_ok());
    REQUIRE(test_store.store.initializeCoreSchema().is_ok());

    SUBCASE("Store and retrieve block") {
        auto tx = test_store.store.beginTransaction();
        auto stored = test_store.store.storeBlock(0, String("hash_000"), String("GENESIS"), String("merkle_000"),
                                                  currentTimestamp(), 0);
        CHECK(stored.is_ok());
        tx->commit();

        auto block = test_store.store.getBlockByHeight(0);
        CHECK(block.has_value());
        CHECK(std::string(block->c_str()).find("hash_000") != std::string::npos);
        CHECK(std::string(block->c_str()).find("GENESIS") != std::string::npos);
    }

    SUBCASE("Store multiple blocks") {
        auto tx = test_store.store.beginTransaction();
        for (i64 i = 0; i < 10; ++i) {
            std::string hash = "hash_" + std::to_string(i);
            std::string prev = (i == 0) ? "GENESIS" : "hash_" + std::to_string(i - 1);
            std::string merkle = "merkle_" + std::to_string(i);

            CHECK(test_store.store
                      .storeBlock(i, String(hash.c_str()), String(prev.c_str()), String(merkle.c_str()),
                                  currentTimestamp(), i * 1000)
                      .is_ok());
        }
        tx->commit();

        CHECK(test_store.store.getBlockCount() == 10);
        CHECK(test_store.store.getLatestBlockHeight() == 9);
    }

    SUBCASE("Get block by hash") {
        auto tx = test_store.store.beginTransaction();
        test_store.store.storeBlock(5, String("unique_hash"), String("prev"), String("merkle"), currentTimestamp(), 0);
        tx->commit();

        auto block = test_store.store.getBlockByHash(String("unique_hash"));
        CHECK(block.has_value());
        CHECK(std::string(block->c_str()).find("\"height\":5") != std::string::npos);
    }

    SUBCASE("Verify chain continuity") {
        auto tx = test_store.store.beginTransaction();
        // Continuous chain
        for (i64 i = 0; i < 5; ++i) {
            std::string hash = "hash_" + std::to_string(i);
            test_store.store.storeBlock(i, String(hash.c_str()), String("prev"), String("merkle"), currentTimestamp(),
                                        0);
        }
        tx->commit();
        auto cont1 = test_store.store.verifyChainContinuity();
        CHECK(cont1.is_ok());
        CHECK(cont1.value());

        // Add gap - store block 10 when we only have 0-4
        auto tx2 = test_store.store.beginTransaction();
        test_store.store.storeBlock(10, String("hash_10"), String("prev"), String("merkle"), currentTimestamp(), 0);
        tx2->commit();
        auto cont2 = test_store.store.verifyChainContinuity();
        CHECK(cont2.is_ok());
        CHECK_FALSE(cont2.value());
    }
}

// ===========================================
// Transaction storage tests
// ===========================================

TEST_CASE("Transaction storage") {
    TestStore test_store("test_txs");
    REQUIRE(test_store.store.open(String(test_store.path.c_str())).is_ok());
    REQUIRE(test_store.store.initializeCoreSchema().is_ok());

    // Create a block first
    auto tx_init = test_store.store.beginTransaction();
    REQUIRE(
        test_store.store.storeBlock(1, String("block_1"), String("GENESIS"), String("merkle"), currentTimestamp(), 0)
            .is_ok());
    tx_init->commit();

    SUBCASE("Store and retrieve transaction") {
        Vector<u8> payload = {0x01, 0x02, 0x03, 0x04};
        auto tx = test_store.store.beginTransaction();
        auto stored = test_store.store.storeTransaction(String("tx_001"), 1, currentTimestamp(), 100, payload);
        CHECK(stored.is_ok());
        tx->commit();

        auto retrieved = test_store.store.getTransaction(String("tx_001"));
        CHECK(retrieved.has_value());
        CHECK(retrieved->size() == payload.size());
        for (usize i = 0; i < payload.size(); ++i) {
            CHECK((*retrieved)[i] == payload[i]);
        }
    }

    SUBCASE("Store multiple transactions") {
        auto tx = test_store.store.beginTransaction();
        for (int i = 0; i < 20; ++i) {
            std::string tx_id = "tx_" + std::to_string(i);
            Vector<u8> payload = {static_cast<u8>(i)};
            CHECK(
                test_store.store.storeTransaction(String(tx_id.c_str()), 1, currentTimestamp(), 100, payload).is_ok());
        }
        tx->commit();

        CHECK(test_store.store.getTransactionCount() == 20);
    }

    SUBCASE("Query transactions") {
        auto tx = test_store.store.beginTransaction();
        // Store transactions across multiple blocks
        test_store.store.storeBlock(2, String("block_2"), String("block_1"), String("merkle"), currentTimestamp(), 0);
        test_store.store.storeBlock(3, String("block_3"), String("block_2"), String("merkle"), currentTimestamp(), 0);

        Vector<u8> payload = {0x01};
        test_store.store.storeTransaction(String("tx_1_1"), 1, 1000, 100, payload);
        test_store.store.storeTransaction(String("tx_2_1"), 2, 2000, 100, payload);
        test_store.store.storeTransaction(String("tx_3_1"), 3, 3000, 100, payload);
        tx->commit();

        LedgerQuery query;
        query.block_height_min = Optional<i64>(2);
        query.block_height_max = Optional<i64>(3);

        auto results = test_store.store.queryTransactions(query);
        CHECK(results.size() == 2);
    }
}

// ===========================================
// Anchor tests
// ===========================================

TEST_CASE("Anchoring") {
    TestStore test_store("test_anchors");
    REQUIRE(test_store.store.open(String(test_store.path.c_str())).is_ok());
    REQUIRE(test_store.store.initializeCoreSchema().is_ok());

    // Setup: create block and transaction
    auto tx_init = test_store.store.beginTransaction();
    REQUIRE(test_store.store
                .storeBlock(1, String("block_1"), String("GENESIS"), String("merkle_root"), currentTimestamp(), 0)
                .is_ok());
    Vector<u8> tx_payload = {0x01};
    REQUIRE(test_store.store.storeTransaction(String("tx_001"), 1, currentTimestamp(), 100, tx_payload).is_ok());
    tx_init->commit();

    SUBCASE("Create and retrieve anchor") {
        Vector<u8> content = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
        auto content_hash = computeSHA256(content);

        Array<u8, 32> merkle_root = {};
        TxRef tx_ref;
        tx_ref.tx_id = String("tx_001");
        tx_ref.block_height = 1;
        tx_ref.merkle_root = merkle_root;

        auto tx = test_store.store.beginTransaction();
        auto created = test_store.store.createAnchor(String("doc_001"), content_hash, tx_ref);
        CHECK(created.is_ok());
        tx->commit();

        auto anchor = test_store.store.getAnchor(String("doc_001"));
        CHECK(anchor.has_value());
        CHECK(std::string(anchor->content_id.c_str()) == "doc_001");
        CHECK(anchor->content_hash.size() == content_hash.size());
        CHECK(std::string(anchor->tx_id.c_str()) == "tx_001");
        CHECK(anchor->block_height == 1);
    }

    SUBCASE("Verify anchor integrity") {
        Vector<u8> original_content = {0x41, 0x42, 0x43}; // "ABC"
        auto content_hash = computeSHA256(original_content);

        Array<u8, 32> merkle_root = {};
        TxRef tx_ref;
        tx_ref.tx_id = String("tx_001");
        tx_ref.block_height = 1;
        tx_ref.merkle_root = merkle_root;

        auto tx = test_store.store.beginTransaction();
        test_store.store.createAnchor(String("doc_002"), content_hash, tx_ref);
        tx->commit();

        // Verify with same content - should pass
        auto verify1 = test_store.store.verifyAnchor(String("doc_002"), original_content);
        CHECK(verify1.is_ok());
        CHECK(verify1.value());

        // Verify with different content - should fail
        Vector<u8> tampered_content = {0x41, 0x42, 0x43, 0x44}; // "ABCD"
        auto verify2 = test_store.store.verifyAnchor(String("doc_002"), tampered_content);
        CHECK(verify2.is_ok());
        CHECK_FALSE(verify2.value());
    }

    SUBCASE("Get anchors by block") {
        Array<u8, 32> merkle_root = {};

        auto tx = test_store.store.beginTransaction();
        for (int i = 0; i < 5; ++i) {
            std::string content_id = "doc_" + std::to_string(i);
            Vector<u8> content = {static_cast<u8>(i)};
            auto hash = computeSHA256(content);

            TxRef tx_ref;
            tx_ref.tx_id = String("tx_001");
            tx_ref.block_height = 1;
            tx_ref.merkle_root = merkle_root;
            test_store.store.createAnchor(String(content_id.c_str()), hash, tx_ref);
        }
        tx->commit();

        auto anchors = test_store.store.getAnchorsByBlock(1);
        CHECK(anchors.size() == 5);
    }

    SUBCASE("Get anchors in range") {
        auto tx = test_store.store.beginTransaction();
        test_store.store.storeBlock(2, String("block_2"), String("block_1"), String("merkle"), currentTimestamp(), 0);
        test_store.store.storeBlock(3, String("block_3"), String("block_2"), String("merkle"), currentTimestamp(), 0);

        Vector<u8> payload = {0x01};
        test_store.store.storeTransaction(String("tx_002"), 2, currentTimestamp(), 100, payload);
        test_store.store.storeTransaction(String("tx_003"), 3, currentTimestamp(), 100, payload);

        Array<u8, 32> merkle_root = {};
        Vector<u8> content = {0x42};
        auto hash = computeSHA256(content);

        TxRef tx_ref1;
        tx_ref1.tx_id = String("tx_001");
        tx_ref1.block_height = 1;
        tx_ref1.merkle_root = merkle_root;
        test_store.store.createAnchor(String("doc_b1"), hash, tx_ref1);

        TxRef tx_ref2;
        tx_ref2.tx_id = String("tx_002");
        tx_ref2.block_height = 2;
        tx_ref2.merkle_root = merkle_root;
        test_store.store.createAnchor(String("doc_b2"), hash, tx_ref2);

        TxRef tx_ref3;
        tx_ref3.tx_id = String("tx_003");
        tx_ref3.block_height = 3;
        tx_ref3.merkle_root = merkle_root;
        test_store.store.createAnchor(String("doc_b3"), hash, tx_ref3);
        tx->commit();

        auto anchors = test_store.store.getAnchorsInRange(2, 3);
        CHECK(anchors.size() == 2);
    }
}

// ===========================================
// Transaction guard tests
// ===========================================

TEST_CASE("Transaction guard") {
    TestStore test_store("test_tx_guard");
    REQUIRE(test_store.store.open(String(test_store.path.c_str())).is_ok());
    REQUIRE(test_store.store.initializeCoreSchema().is_ok());

    SUBCASE("Commit transaction") {
        {
            auto tx = test_store.store.beginTransaction();
            test_store.store.storeBlock(1, String("hash1"), String("GENESIS"), String("merkle"), currentTimestamp(), 0);
            tx->commit();
        }

        CHECK(test_store.store.getBlockCount() == 1);
    }

    SUBCASE("Rollback on exception") {
        try {
            auto tx = test_store.store.beginTransaction();
            test_store.store.storeBlock(2, String("hash2"), String("GENESIS"), String("merkle"), currentTimestamp(), 0);
            // Don't commit - let it rollback
            throw std::runtime_error("test error");
        } catch (...) {
        }

        CHECK(test_store.store.getBlockCount() == 0);
    }

    SUBCASE("Explicit rollback") {
        {
            auto tx = test_store.store.beginTransaction();
            test_store.store.storeBlock(3, String("hash3"), String("GENESIS"), String("merkle"), currentTimestamp(), 0);
            tx->rollback();
        }

        CHECK(test_store.store.getBlockCount() == 0);
    }
}

// ===========================================
// Diagnostics tests
// ===========================================

TEST_CASE("Diagnostics") {
    TestStore test_store("test_diagnostics");
    REQUIRE(test_store.store.open(String(test_store.path.c_str())).is_ok());
    REQUIRE(test_store.store.initializeCoreSchema().is_ok());

    SUBCASE("Statistics") {
        CHECK(test_store.store.getBlockCount() == 0);
        CHECK(test_store.store.getTransactionCount() == 0);
        CHECK(test_store.store.getAnchorCount() == 0);
        CHECK(test_store.store.getLatestBlockHeight() == -1);

        auto tx = test_store.store.beginTransaction();
        test_store.store.storeBlock(0, String("hash"), String("GENESIS"), String("merkle"), currentTimestamp(), 0);
        tx->commit();

        CHECK(test_store.store.getBlockCount() == 1);
        CHECK(test_store.store.getLatestBlockHeight() == 0);
    }

    SUBCASE("Quick check") {
        auto result = test_store.store.quickCheck();
        CHECK(result.is_ok());
        CHECK(result.value());
    }
}

// ===========================================
// Persistence tests
// ===========================================

TEST_CASE("Persistence across sessions") {
    const std::string path = "test_persistence_store";

    // Cleanup
    if (std::filesystem::exists(path)) {
        std::filesystem::remove_all(path);
    }

    // First session: write data
    {
        FileStore store;
        REQUIRE(store.open(String(path.c_str())).is_ok());
        REQUIRE(store.initializeCoreSchema().is_ok());

        auto tx = store.beginTransaction();
        store.storeBlock(0, String("genesis"), String("GENESIS"), String("merkle0"), 1000, 0);
        store.storeBlock(1, String("block1"), String("genesis"), String("merkle1"), 1001, 100);
        store.storeTransaction(String("tx1"), 1, 1001, 50, Vector<u8>{0x01, 0x02, 0x03});

        Array<u8, 32> merkle = {};
        TxRef tx_ref;
        tx_ref.tx_id = String("tx1");
        tx_ref.block_height = 1;
        tx_ref.merkle_root = merkle;
        store.createAnchor(String("doc1"), computeSHA256(Vector<u8>{0xAB, 0xCD}), tx_ref);
        tx->commit();

        store.close();
    }

    // Second session: read data
    {
        FileStore store;
        REQUIRE(store.open(String(path.c_str())).is_ok());

        CHECK(store.getBlockCount() == 2);
        CHECK(store.getTransactionCount() == 1);
        CHECK(store.getAnchorCount() == 1);
        CHECK(store.getLatestBlockHeight() == 1);

        auto block = store.getBlockByHeight(1);
        CHECK(block.has_value());
        CHECK(std::string(block->c_str()).find("block1") != std::string::npos);

        auto tx_payload = store.getTransaction(String("tx1"));
        CHECK(tx_payload.has_value());
        CHECK(tx_payload->size() == 3);

        auto anchor = store.getAnchor(String("doc1"));
        CHECK(anchor.has_value());
        CHECK(std::string(anchor->tx_id.c_str()) == "tx1");

        store.close();
    }

    // Cleanup
    std::filesystem::remove_all(path);
}
