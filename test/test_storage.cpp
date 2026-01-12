#include "blockit.hpp"
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <memory>

// Test data structure with serialization support
struct StorageTestData {
    std::string identifier;
    double value;

    std::string to_string() const { return "StorageTestData{" + identifier + ":" + std::to_string(value) + "}"; }

    // JSON serialization support
    std::string serialize() const {
        return R"({"identifier": ")" + identifier + R"(", "value": )" + std::to_string(value) + "}";
    }

    static StorageTestData deserialize(const std::string &data) {
        StorageTestData result;

        // Parse identifier
        size_t id_start = data.find("\"identifier\": \"") + 15;
        size_t id_end = data.find("\"", id_start);
        result.identifier = data.substr(id_start, id_end - id_start);

        // Parse value
        size_t val_start = data.find("\"value\": ") + 9;
        size_t val_end = data.find("}", val_start);
        result.value = std::stod(data.substr(val_start, val_end - val_start));

        return result;
    }

    // Binary serialization support
    std::vector<uint8_t> serializeBinary() const {
        std::vector<uint8_t> buffer;
        chain::BinarySerializer::writeString(buffer, identifier);
        chain::BinarySerializer::writeDouble(buffer, value);
        return buffer;
    }

    static StorageTestData deserializeBinary(const std::vector<uint8_t> &data) {
        StorageTestData result;
        size_t offset = 0;
        result.identifier = chain::BinarySerializer::readString(data, offset);
        result.value = chain::BinarySerializer::readDouble(data, offset);
        return result;
    }
};

TEST_SUITE("Persistent Storage - Implemented") {
    TEST_CASE("Blockchain serialization") {
        auto privateKey = std::make_shared<chain::Crypto>("storage_key");

        chain::Chain<StorageTestData> originalChain("storage-chain", "genesis", StorageTestData{"genesis", 0.0},
                                                    privateKey);

        // Add some blocks to the chain
        for (int i = 1; i <= 5; i++) {
            chain::Transaction<StorageTestData> tx("storage-tx-" + std::to_string(i),
                                                   StorageTestData{"data-" + std::to_string(i), i * 1.5}, 100);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            originalChain.addBlock(block);
        }

        // Test serialization
        std::string serialized = originalChain.serialize();
        CHECK_FALSE(serialized.empty());
        CHECK(serialized.find("genesis") != std::string::npos);
        CHECK(serialized.find("storage-tx-1") != std::string::npos);
        CHECK(serialized.find("storage-chain") != std::string::npos);

        std::cout << "Serialization successful! Data length: " << serialized.length() << " characters" << std::endl;
    }

    TEST_CASE("Blockchain deserialization") {
        auto privateKey = std::make_shared<chain::Crypto>("deserialize_key");

        // Create original chain
        chain::Chain<StorageTestData> originalChain("test-chain", "genesis", StorageTestData{"genesis", 0.0},
                                                    privateKey);

        // Add a few blocks
        for (int i = 1; i <= 3; i++) {
            chain::Transaction<StorageTestData> tx("deserialize-tx-" + std::to_string(i),
                                                   StorageTestData{"test-data-" + std::to_string(i), i * 0.5}, 100);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            originalChain.addBlock(block);
        }

        // Serialize the chain
        std::string serializedData = originalChain.serialize();

        // Deserialize it back
        chain::Chain<StorageTestData> deserializedChain = chain::Chain<StorageTestData>::deserialize(serializedData);

        // Verify the deserialized chain
        CHECK(deserializedChain.uuid_ == "test-chain");
        CHECK(deserializedChain.getChainLength() == originalChain.getChainLength());
        CHECK(deserializedChain.isValid());

        // Check that blocks match
        for (size_t i = 0; i < originalChain.blocks_.size(); ++i) {
            CHECK(deserializedChain.blocks_[i].index_ == originalChain.blocks_[i].index_);
            CHECK(deserializedChain.blocks_[i].transactions_.size() == originalChain.blocks_[i].transactions_.size());
        }

        std::cout << "Deserialization successful! Chain length: " << deserializedChain.getChainLength() << std::endl;
    }

    TEST_CASE("File-based storage") {
        auto privateKey = std::make_shared<chain::Crypto>("file_key");

        chain::Chain<StorageTestData> chain("file-chain", "genesis", StorageTestData{"genesis", 0.0}, privateKey);

        // Add blocks
        for (int i = 1; i <= 3; i++) {
            chain::Transaction<StorageTestData> tx("file-tx-" + std::to_string(i),
                                                   StorageTestData{"file-data", i * 2.0}, 100);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            chain.addBlock(block);
        }

        std::string filename = "test_blockchain.json";

        // Test file operations
        CHECK(chain.saveToFile(filename));
        CHECK(std::filesystem::exists(filename));

        chain::Chain<StorageTestData> loadedChain;
        CHECK(loadedChain.loadFromFile(filename));
        CHECK(loadedChain.getChainLength() == chain.getChainLength());
        CHECK(loadedChain.uuid_ == chain.uuid_);
        CHECK(loadedChain.isValid());

        // Verify that the loaded chain has the same data
        for (size_t i = 0; i < chain.blocks_.size(); ++i) {
            CHECK(loadedChain.blocks_[i].index_ == chain.blocks_[i].index_);
            CHECK(loadedChain.blocks_[i].hash_ == chain.blocks_[i].hash_);
            CHECK(loadedChain.blocks_[i].transactions_.size() == chain.blocks_[i].transactions_.size());
        }

        // Cleanup
        std::filesystem::remove(filename);

        std::cout << "File storage test passed! Saved and loaded " << loadedChain.getChainLength() << " blocks"
                  << std::endl;
    }

    TEST_CASE("Transaction serialization") {
        auto privateKey = std::make_shared<chain::Crypto>("tx_serialize_key");

        // Create a transaction
        chain::Transaction<StorageTestData> originalTx("test-tx-001", StorageTestData{"transaction_data", 42.5}, 150);
        originalTx.signTransaction(privateKey);

        // Test serialization
        std::string serialized = originalTx.serialize();
        CHECK_FALSE(serialized.empty());
        CHECK(serialized.find("test-tx-001") != std::string::npos);
        CHECK(serialized.find("transaction_data") != std::string::npos);

        // Test deserialization
        chain::Transaction<StorageTestData> deserializedTx =
            chain::Transaction<StorageTestData>::deserialize(serialized);

        CHECK(deserializedTx.uuid_ == originalTx.uuid_);
        CHECK(deserializedTx.priority_ == originalTx.priority_);
        CHECK(deserializedTx.function_.identifier == originalTx.function_.identifier);
        CHECK(deserializedTx.function_.value == originalTx.function_.value);
        CHECK(deserializedTx.signature_ == originalTx.signature_);

        std::cout << "Transaction serialization test passed!" << std::endl;
    }

    TEST_CASE("Block serialization") {
        auto privateKey = std::make_shared<chain::Crypto>("block_serialize_key");

        // Create transactions
        std::vector<chain::Transaction<StorageTestData>> transactions;
        for (int i = 1; i <= 3; i++) {
            chain::Transaction<StorageTestData> tx("block-tx-" + std::to_string(i),
                                                   StorageTestData{"block_data_" + std::to_string(i), i * 10.0}, 100);
            tx.signTransaction(privateKey);
            transactions.push_back(tx);
        }

        // Create block
        chain::Block<StorageTestData> originalBlock(transactions);

        // Test serialization
        std::string serialized = originalBlock.serialize();
        CHECK_FALSE(serialized.empty());
        CHECK(serialized.find("block-tx-1") != std::string::npos);

        // Test deserialization
        chain::Block<StorageTestData> deserializedBlock = chain::Block<StorageTestData>::deserialize(serialized);

        CHECK(deserializedBlock.index_ == originalBlock.index_);
        CHECK(deserializedBlock.hash_ == originalBlock.hash_);
        CHECK(deserializedBlock.previous_hash_ == originalBlock.previous_hash_);
        CHECK(deserializedBlock.transactions_.size() == originalBlock.transactions_.size());
        CHECK(deserializedBlock.merkle_root_ == originalBlock.merkle_root_);

        std::cout << "Block serialization test passed!" << std::endl;
    }

    TEST_CASE("Database integration (SIMULATED)") {
        auto privateKey = std::make_shared<chain::Crypto>("db_key");

        // Simulate database storage with file-based approach
        chain::Chain<StorageTestData> chain("db-chain", "genesis", StorageTestData{"genesis", 0.0}, privateKey);

        // Add blocks that would be automatically persisted in a real database
        for (int i = 1; i <= 10; i++) {
            chain::Transaction<StorageTestData> tx("db-tx-" + std::to_string(i), StorageTestData{"db-data", i * 0.1},
                                                   100);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            chain.addBlock(block);
        }

        // Simulate database operations using serialization
        std::string chainData = chain.serialize();
        CHECK_FALSE(chainData.empty());

        // Simulate retrieving from database
        chain::Chain<StorageTestData> retrievedChain = chain::Chain<StorageTestData>::deserialize(chainData);

        CHECK(retrievedChain.getChainLength() == 11); // Including genesis
        CHECK(retrievedChain.isValid());

        // Verify we can access specific blocks
        CHECK(retrievedChain.blocks_[5].index_ == 5);
        CHECK(retrievedChain.blocks_[5].transactions_.size() == 1);

        std::cout << "Database simulation test passed! Chain has " << retrievedChain.getChainLength() << " blocks"
                  << std::endl;
    }

    TEST_CASE("Block pruning and archival (SIMULATED)") {
        auto privateKey = std::make_shared<chain::Crypto>("pruning_key");

        chain::Chain<StorageTestData> chain("pruning-chain", "genesis", StorageTestData{"genesis", 0.0}, privateKey);

        // Add many blocks
        for (int i = 1; i <= 100; i++) {
            chain::Transaction<StorageTestData> tx("pruning-tx-" + std::to_string(i), StorageTestData{"data", i * 0.01},
                                                   100);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            chain.addBlock(block);
        }

        // Simulate pruning functionality by keeping only recent blocks
        CHECK(chain.getChainLength() == 101); // Including genesis

        // Simulate pruning by creating a new chain with only the last 50 blocks
        chain::Chain<StorageTestData> prunedChain("pruned-chain", "genesis", StorageTestData{"genesis", 0.0},
                                                  privateKey);

        // Add only the last 49 blocks (plus genesis = 50 total)
        for (int i = 52; i <= 100; i++) {
            if (i - 1 < static_cast<int>(chain.blocks_.size())) {
                prunedChain.addBlock(chain.blocks_[i]);
            }
        }

        // The original chain should still have all blocks
        CHECK(chain.getChainLength() == 101);

        // Simulate archival by serializing pruned blocks
        std::vector<std::string> archivedBlocks;
        for (int i = 1; i <= 51; i++) {
            if (i < static_cast<int>(chain.blocks_.size())) {
                archivedBlocks.push_back(chain.blocks_[i].serialize());
            }
        }

        CHECK(archivedBlocks.size() > 0);

        std::cout << "Block pruning simulation test passed! Archived " << archivedBlocks.size() << " blocks"
                  << std::endl;
    }

    TEST_CASE("State snapshots (SIMULATED)") {
        auto privateKey = std::make_shared<chain::Crypto>("snapshot_key");

        chain::Chain<StorageTestData> chain("snapshot-chain", "genesis", StorageTestData{"genesis", 0.0}, privateKey);

        // Add blocks to build state
        for (int i = 1; i <= 20; i++) {
            chain::Transaction<StorageTestData> tx(
                "snapshot-tx-" + std::to_string(i),
                StorageTestData{"state-" + std::to_string(i), static_cast<double>(i)}, 100);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            chain.addBlock(block);
        }

        // Simulate state snapshot by serializing current chain state
        std::string snapshot = chain.serialize();
        CHECK_FALSE(snapshot.empty());
        CHECK(chain.getChainLength() == 21); // 20 + genesis

        // Add more blocks
        for (int i = 21; i <= 30; i++) {
            chain::Transaction<StorageTestData> tx("snapshot-tx-" + std::to_string(i),
                                                   StorageTestData{"post-snapshot", static_cast<double>(i)}, 100);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            chain.addBlock(block);
        }

        CHECK(chain.getChainLength() == 31); // 30 + genesis

        // Simulate restoring from snapshot
        chain::Chain<StorageTestData> restoredChain = chain::Chain<StorageTestData>::deserialize(snapshot);
        CHECK(restoredChain.getChainLength() == 21); // Back to snapshot point
        CHECK(restoredChain.isValid());

        std::cout << "State snapshots simulation test passed! Restored to block height "
                  << restoredChain.getChainLength() << std::endl;
    }

    TEST_CASE("Binary vs JSON Transaction Serialization") {
        auto privateKey = std::make_shared<chain::Crypto>("binary_json_tx_key");

        // Create a transaction
        chain::Transaction<StorageTestData> originalTx("binary-json-tx-001", StorageTestData{"binary_test", 123.456},
                                                       200);
        originalTx.signTransaction(privateKey);

        // Test JSON serialization (default)
        std::string jsonSerialized = originalTx.serialize();
        CHECK_FALSE(jsonSerialized.empty());
        CHECK(jsonSerialized.find("binary-json-tx-001") != std::string::npos);
        CHECK(jsonSerialized.find("binary_test") != std::string::npos);
        CHECK(jsonSerialized[0] == '{'); // Should start with JSON brace

        // Test explicit JSON serialization
        std::string explicitJsonSerialized = originalTx.serializeJson();
        CHECK(jsonSerialized == explicitJsonSerialized);

        // Test binary serialization
        std::vector<uint8_t> binarySerialized = originalTx.serializeBinary();
        CHECK_FALSE(binarySerialized.empty());
        CHECK(binarySerialized.size() > 0);
        // Binary data shouldn't be readable as text
        CHECK(binarySerialized[0] != '{');

        // Test JSON deserialization
        chain::Transaction<StorageTestData> jsonDeserialized =
            chain::Transaction<StorageTestData>::deserialize(jsonSerialized);

        CHECK(jsonDeserialized.uuid_ == originalTx.uuid_);
        CHECK(jsonDeserialized.priority_ == originalTx.priority_);
        CHECK(jsonDeserialized.function_.identifier == originalTx.function_.identifier);
        CHECK(jsonDeserialized.function_.value == originalTx.function_.value);

        // Test binary deserialization
        chain::Transaction<StorageTestData> binaryDeserialized =
            chain::Transaction<StorageTestData>::deserializeBinary(binarySerialized);

        CHECK(binaryDeserialized.uuid_ == originalTx.uuid_);
        CHECK(binaryDeserialized.priority_ == originalTx.priority_);
        CHECK(binaryDeserialized.function_.identifier == originalTx.function_.identifier);
        CHECK(binaryDeserialized.function_.value == originalTx.function_.value);

        // Both deserialization methods should produce identical results
        CHECK(jsonDeserialized.uuid_ == binaryDeserialized.uuid_);
        CHECK(jsonDeserialized.priority_ == binaryDeserialized.priority_);
        CHECK(jsonDeserialized.function_.identifier == binaryDeserialized.function_.identifier);
        CHECK(jsonDeserialized.function_.value == binaryDeserialized.function_.value);

        // Test size comparison (binary should typically be smaller)
        std::cout << "JSON size: " << jsonSerialized.size() << " bytes" << std::endl;
        std::cout << "Binary size: " << binarySerialized.size() << " bytes" << std::endl;

        std::cout << "Binary vs JSON Transaction serialization test passed!" << std::endl;
    }

    TEST_CASE("Binary vs JSON Block Serialization") {
        auto privateKey = std::make_shared<chain::Crypto>("binary_json_block_key");

        // Create transactions for the block
        std::vector<chain::Transaction<StorageTestData>> transactions;
        for (int i = 1; i <= 3; i++) {
            chain::Transaction<StorageTestData> tx("binary-block-tx-" + std::to_string(i),
                                                   StorageTestData{"block_binary_test_" + std::to_string(i), i * 100.0},
                                                   150);
            tx.signTransaction(privateKey);
            transactions.push_back(tx);
        }

        // Create block
        chain::Block<StorageTestData> originalBlock(transactions);

        // Test JSON serialization (default)
        std::string jsonSerialized = originalBlock.serialize();
        CHECK_FALSE(jsonSerialized.empty());
        CHECK(jsonSerialized.find("binary-block-tx-1") != std::string::npos);
        CHECK(jsonSerialized[0] == '{'); // Should start with JSON brace

        // Test explicit JSON serialization
        std::string explicitJsonSerialized = originalBlock.serializeJson();
        CHECK(jsonSerialized == explicitJsonSerialized);

        // Test binary serialization
        std::vector<uint8_t> binarySerialized = originalBlock.serializeBinary();
        CHECK_FALSE(binarySerialized.empty());
        CHECK(binarySerialized.size() > 0);
        // Binary data shouldn't be readable as text
        CHECK(binarySerialized[0] != '{');

        // Test JSON deserialization
        chain::Block<StorageTestData> jsonDeserialized = chain::Block<StorageTestData>::deserialize(jsonSerialized);

        CHECK(jsonDeserialized.index_ == originalBlock.index_);
        CHECK(jsonDeserialized.hash_ == originalBlock.hash_);
        CHECK(jsonDeserialized.previous_hash_ == originalBlock.previous_hash_);
        CHECK(jsonDeserialized.merkle_root_ == originalBlock.merkle_root_);
        CHECK(jsonDeserialized.transactions_.size() == originalBlock.transactions_.size());

        // Test binary deserialization
        chain::Block<StorageTestData> binaryDeserialized =
            chain::Block<StorageTestData>::deserializeBinary(binarySerialized);

        CHECK(binaryDeserialized.index_ == originalBlock.index_);
        CHECK(binaryDeserialized.hash_ == originalBlock.hash_);
        CHECK(binaryDeserialized.previous_hash_ == originalBlock.previous_hash_);
        CHECK(binaryDeserialized.merkle_root_ == originalBlock.merkle_root_);
        CHECK(binaryDeserialized.transactions_.size() == originalBlock.transactions_.size());

        // Both deserialization methods should produce identical results
        CHECK(jsonDeserialized.index_ == binaryDeserialized.index_);
        CHECK(jsonDeserialized.hash_ == binaryDeserialized.hash_);
        CHECK(jsonDeserialized.previous_hash_ == binaryDeserialized.previous_hash_);
        CHECK(jsonDeserialized.merkle_root_ == binaryDeserialized.merkle_root_);
        CHECK(jsonDeserialized.transactions_.size() == binaryDeserialized.transactions_.size());

        // Test transaction data integrity
        for (size_t i = 0; i < originalBlock.transactions_.size(); i++) {
            CHECK(jsonDeserialized.transactions_[i].uuid_ == binaryDeserialized.transactions_[i].uuid_);
            CHECK(jsonDeserialized.transactions_[i].function_.identifier ==
                  binaryDeserialized.transactions_[i].function_.identifier);
            CHECK(jsonDeserialized.transactions_[i].function_.value ==
                  binaryDeserialized.transactions_[i].function_.value);
        }

        // Test size comparison
        std::cout << "JSON size: " << jsonSerialized.size() << " bytes" << std::endl;
        std::cout << "Binary size: " << binarySerialized.size() << " bytes" << std::endl;

        std::cout << "Binary vs JSON Block serialization test passed!" << std::endl;
    }

    TEST_CASE("Performance Comparison: Binary vs JSON") {
        auto privateKey = std::make_shared<chain::Crypto>("perf_test_key");

        // Create a substantial transaction for performance testing
        chain::Transaction<StorageTestData> tx(
            "performance-test-tx", StorageTestData{"performance_data_with_long_identifier", 987.654321}, 300);
        tx.signTransaction(privateKey);

        // Create a block with multiple transactions
        std::vector<chain::Transaction<StorageTestData>> transactions;
        for (int i = 0; i < 10; i++) {
            chain::Transaction<StorageTestData> perfTx(
                "perf-tx-" + std::to_string(i),
                StorageTestData{"performance_test_data_item_" + std::to_string(i), i * 3.14159}, 100 + i);
            perfTx.signTransaction(privateKey);
            transactions.push_back(perfTx);
        }

        chain::Block<StorageTestData> block(transactions);

        // Measure JSON serialization
        auto jsonStart = std::chrono::high_resolution_clock::now();
        std::string jsonData = block.serialize();
        auto jsonEnd = std::chrono::high_resolution_clock::now();
        auto jsonTime = std::chrono::duration_cast<std::chrono::microseconds>(jsonEnd - jsonStart);

        // Measure binary serialization
        auto binaryStart = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> binaryData = block.serializeBinary();
        auto binaryEnd = std::chrono::high_resolution_clock::now();
        auto binaryTime = std::chrono::duration_cast<std::chrono::microseconds>(binaryEnd - binaryStart);

        // Measure JSON deserialization
        auto jsonDeserStart = std::chrono::high_resolution_clock::now();
        chain::Block<StorageTestData> jsonDeserialized = chain::Block<StorageTestData>::deserialize(jsonData);
        auto jsonDeserEnd = std::chrono::high_resolution_clock::now();
        auto jsonDeserTime = std::chrono::duration_cast<std::chrono::microseconds>(jsonDeserEnd - jsonDeserStart);

        // Measure binary deserialization
        auto binaryDeserStart = std::chrono::high_resolution_clock::now();
        chain::Block<StorageTestData> binaryDeserialized = chain::Block<StorageTestData>::deserializeBinary(binaryData);
        auto binaryDeserEnd = std::chrono::high_resolution_clock::now();
        auto binaryDeserTime = std::chrono::duration_cast<std::chrono::microseconds>(binaryDeserEnd - binaryDeserStart);

        // Verify both methods produce correct results
        CHECK(jsonDeserialized.transactions_.size() == binaryDeserialized.transactions_.size());
        CHECK(jsonDeserialized.hash_ == binaryDeserialized.hash_);
        CHECK(jsonDeserialized.merkle_root_ == binaryDeserialized.merkle_root_);

        // Report performance metrics
        std::cout << "=== Performance Comparison ===" << std::endl;
        std::cout << "JSON size: " << jsonData.size() << " bytes" << std::endl;
        std::cout << "Binary size: " << binaryData.size() << " bytes" << std::endl;
        std::cout << "Size reduction: " << (100.0 * (jsonData.size() - binaryData.size()) / jsonData.size()) << "%"
                  << std::endl;
        std::cout << "JSON serialization time: " << jsonTime.count() << " microseconds" << std::endl;
        std::cout << "Binary serialization time: " << binaryTime.count() << " microseconds" << std::endl;
        std::cout << "JSON deserialization time: " << jsonDeserTime.count() << " microseconds" << std::endl;
        std::cout << "Binary deserialization time: " << binaryDeserTime.count() << " microseconds" << std::endl;

        // Binary format should typically be smaller
        CHECK(binaryData.size() <= jsonData.size());

        std::cout << "Performance comparison test passed!" << std::endl;
    }

    TEST_CASE("Format Auto-Detection") {
        auto privateKey = std::make_shared<chain::Crypto>("auto_detect_key");

        // Create a transaction
        chain::Transaction<StorageTestData> originalTx("auto-detect-tx", StorageTestData{"auto_detect_test", 42.0},
                                                       100);
        originalTx.signTransaction(privateKey);

        // Serialize in both formats
        std::string jsonData = originalTx.serializeJson();
        std::vector<uint8_t> binaryData = originalTx.serializeBinary();

        // Convert JSON string to bytes for auto-detection testing
        std::vector<uint8_t> jsonAsBytes(jsonData.begin(), jsonData.end());

        // Test auto-detection with JSON data (as bytes)
        chain::Transaction<StorageTestData> autoDetectedJson =
            chain::Transaction<StorageTestData>::deserializeAuto(jsonAsBytes);

        CHECK(autoDetectedJson.uuid_ == originalTx.uuid_);
        CHECK(autoDetectedJson.function_.identifier == originalTx.function_.identifier);
        CHECK(autoDetectedJson.function_.value == originalTx.function_.value);

        // Test auto-detection with binary data
        chain::Transaction<StorageTestData> autoDetectedBinary =
            chain::Transaction<StorageTestData>::deserializeAuto(binaryData);

        CHECK(autoDetectedBinary.uuid_ == originalTx.uuid_);
        CHECK(autoDetectedBinary.function_.identifier == originalTx.function_.identifier);
        CHECK(autoDetectedBinary.function_.value == originalTx.function_.value);

        // Both auto-detected results should be identical
        CHECK(autoDetectedJson.uuid_ == autoDetectedBinary.uuid_);
        CHECK(autoDetectedJson.function_.identifier == autoDetectedBinary.function_.identifier);
        CHECK(autoDetectedJson.function_.value == autoDetectedBinary.function_.value);

        std::cout << "Format auto-detection test passed!" << std::endl;
    }

    TEST_CASE("Unified Serialization System Integration") {
        auto privateKey = std::make_shared<chain::Crypto>("unified_test_key");

        // Create a chain with mixed operations
        chain::Chain<StorageTestData> originalChain("unified-test-chain", "genesis",
                                                    StorageTestData{"genesis_unified", 0.0}, privateKey);

        // Add several blocks with different data
        for (int i = 1; i <= 5; i++) {
            chain::Transaction<StorageTestData> tx("unified-tx-" + std::to_string(i),
                                                   StorageTestData{"unified_data_" + std::to_string(i), i * 2.5},
                                                   100 + i);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            originalChain.addBlock(block);
        }

        // Test that the chain maintains compatibility with existing JSON serialization
        std::string chainJson = originalChain.serialize();
        CHECK_FALSE(chainJson.empty());
        CHECK(chainJson.find("unified-test-chain") != std::string::npos);

        // Deserialize the chain
        chain::Chain<StorageTestData> deserializedChain = chain::Chain<StorageTestData>::deserialize(chainJson);

        CHECK(deserializedChain.uuid_ == originalChain.uuid_);
        CHECK(deserializedChain.getChainLength() == originalChain.getChainLength());
        CHECK(deserializedChain.isValid());

        // Test individual transaction serialization in both formats
        auto &testTx = originalChain.blocks_[1].transactions_[0];

        // JSON serialization (default for backward compatibility)
        std::string txJson = testTx.serialize();
        chain::Transaction<StorageTestData> txFromJson = chain::Transaction<StorageTestData>::deserialize(txJson);

        // Binary serialization (new capability)
        std::vector<uint8_t> txBinary = testTx.serializeBinary();
        chain::Transaction<StorageTestData> txFromBinary =
            chain::Transaction<StorageTestData>::deserializeBinary(txBinary);

        // Both should produce identical results
        CHECK(txFromJson.uuid_ == txFromBinary.uuid_);
        CHECK(txFromJson.function_.identifier == txFromBinary.function_.identifier);
        CHECK(txFromJson.function_.value == txFromBinary.function_.value);
        CHECK(txFromJson.priority_ == txFromBinary.priority_);

        // Test individual block serialization in both formats
        auto &testBlock = originalChain.blocks_[1];

        // JSON serialization (default for backward compatibility)
        std::string blockJson = testBlock.serialize();
        chain::Block<StorageTestData> blockFromJson = chain::Block<StorageTestData>::deserialize(blockJson);

        // Binary serialization (new capability)
        std::vector<uint8_t> blockBinary = testBlock.serializeBinary();
        chain::Block<StorageTestData> blockFromBinary = chain::Block<StorageTestData>::deserializeBinary(blockBinary);

        // Both should produce identical results
        CHECK(blockFromJson.index_ == blockFromBinary.index_);
        CHECK(blockFromJson.hash_ == blockFromBinary.hash_);
        CHECK(blockFromJson.merkle_root_ == blockFromBinary.merkle_root_);
        CHECK(blockFromJson.transactions_.size() == blockFromBinary.transactions_.size());

        // Verify that the type serializer works correctly with StorageTestData
        auto &testData = testTx.function_;

        // Test JSON serialization via TypeSerializer
        std::string dataJson = chain::TypeSerializer<StorageTestData>::serializeJson(testData);
        StorageTestData dataFromJson = chain::TypeSerializer<StorageTestData>::deserializeJson(dataJson);

        CHECK(dataFromJson.identifier == testData.identifier);
        CHECK(dataFromJson.value == testData.value);

        // Test Binary serialization via TypeSerializer
        std::vector<uint8_t> dataBinary = chain::TypeSerializer<StorageTestData>::serializeBinary(testData);
        StorageTestData dataFromBinary = chain::TypeSerializer<StorageTestData>::deserializeBinary(dataBinary);

        CHECK(dataFromBinary.identifier == testData.identifier);
        CHECK(dataFromBinary.value == testData.value);

        // Test SFINAE detection
        bool supportsBinary = chain::TypeSerializer<StorageTestData>::supportsBinary();
        bool supportsJson = chain::TypeSerializer<StorageTestData>::supportsJson();

        CHECK(supportsBinary == true); // StorageTestData has serializeBinary/deserializeBinary
        CHECK(supportsJson == true);   // StorageTestData has serialize/deserialize

        // Demonstrate format preferences
        std::cout << "=== Unified Serialization System Test Results ===" << std::endl;
        std::cout << "Chain JSON size: " << chainJson.size() << " bytes" << std::endl;
        std::cout << "Transaction JSON size: " << txJson.size() << " bytes" << std::endl;
        std::cout << "Transaction Binary size: " << txBinary.size() << " bytes" << std::endl;
        std::cout << "Block JSON size: " << blockJson.size() << " bytes" << std::endl;
        std::cout << "Block Binary size: " << blockBinary.size() << " bytes" << std::endl;
        std::cout << "StorageTestData supports binary: " << (supportsBinary ? "YES" : "NO") << std::endl;
        std::cout << "StorageTestData supports JSON: " << (supportsJson ? "YES" : "NO") << std::endl;

        std::cout << "Unified serialization system integration test passed!" << std::endl;
    }
}
