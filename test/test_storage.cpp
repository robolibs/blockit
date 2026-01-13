#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <memory>

// Test data structure with datapod serialization support
struct StorageTestData {
    dp::String identifier{};
    double value{0.0};

    StorageTestData() = default;
    StorageTestData(const std::string &id, double val) : identifier(dp::String(id.c_str())), value(val) {}

    std::string to_string() const {
        return "StorageTestData{" + std::string(identifier.c_str()) + ":" + std::to_string(value) + "}";
    }

    // Required for datapod serialization
    auto members() { return std::tie(identifier, value); }
    auto members() const { return std::tie(identifier, value); }
};

TEST_SUITE("Persistent Storage - Binary Serialization") {
    TEST_CASE("Blockchain binary serialization") {
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

        // Test binary serialization
        dp::ByteBuf serialized = originalChain.serialize();
        CHECK_FALSE(serialized.empty());
        CHECK(serialized.size() > 0);

        std::cout << "Serialization successful! Data size: " << serialized.size() << " bytes" << std::endl;
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
        dp::ByteBuf serializedData = originalChain.serialize();

        // Deserialize it back
        auto deser_result = chain::Chain<StorageTestData>::deserialize(serializedData);
        REQUIRE(deser_result.is_ok());
        chain::Chain<StorageTestData> deserializedChain = std::move(deser_result.value());

        // Verify the deserialized chain
        CHECK(std::string(deserializedChain.uuid_.c_str()) == "test-chain");
        CHECK(deserializedChain.getChainLength() == originalChain.getChainLength());
        auto valid_result = deserializedChain.isValid();
        CHECK(valid_result.is_ok());
        CHECK(valid_result.value());

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

        std::string filename = "test_blockchain.bin";

        // Test file operations
        CHECK(chain.saveToFile(filename).is_ok());
        CHECK(std::filesystem::exists(filename));

        chain::Chain<StorageTestData> loadedChain;
        CHECK(loadedChain.loadFromFile(filename).is_ok());
        CHECK(loadedChain.getChainLength() == chain.getChainLength());
        CHECK(std::string(loadedChain.uuid_.c_str()) == std::string(chain.uuid_.c_str()));
        auto valid_result = loadedChain.isValid();
        CHECK(valid_result.is_ok());
        CHECK(valid_result.value());

        // Verify that the loaded chain has the same data
        for (size_t i = 0; i < chain.blocks_.size(); ++i) {
            CHECK(loadedChain.blocks_[i].index_ == chain.blocks_[i].index_);
            CHECK(std::string(loadedChain.blocks_[i].hash_.c_str()) == std::string(chain.blocks_[i].hash_.c_str()));
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
        dp::ByteBuf serialized = originalTx.serialize();
        CHECK_FALSE(serialized.empty());
        CHECK(serialized.size() > 0);

        // Test deserialization
        auto deser_result = chain::Transaction<StorageTestData>::deserialize(serialized);
        REQUIRE(deser_result.is_ok());
        chain::Transaction<StorageTestData> deserializedTx = std::move(deser_result.value());

        CHECK(std::string(deserializedTx.uuid_.c_str()) == std::string(originalTx.uuid_.c_str()));
        CHECK(deserializedTx.priority_ == originalTx.priority_);
        CHECK(std::string(deserializedTx.function_.identifier.c_str()) ==
              std::string(originalTx.function_.identifier.c_str()));
        CHECK(deserializedTx.function_.value == originalTx.function_.value);
        CHECK(deserializedTx.signature_.size() == originalTx.signature_.size());

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
        dp::ByteBuf serialized = originalBlock.serialize();
        CHECK_FALSE(serialized.empty());
        CHECK(serialized.size() > 0);

        // Test deserialization
        auto deser_result = chain::Block<StorageTestData>::deserialize(serialized);
        REQUIRE(deser_result.is_ok());
        chain::Block<StorageTestData> deserializedBlock = std::move(deser_result.value());

        CHECK(deserializedBlock.index_ == originalBlock.index_);
        CHECK(std::string(deserializedBlock.hash_.c_str()) == std::string(originalBlock.hash_.c_str()));
        CHECK(std::string(deserializedBlock.previous_hash_.c_str()) ==
              std::string(originalBlock.previous_hash_.c_str()));
        CHECK(deserializedBlock.transactions_.size() == originalBlock.transactions_.size());
        CHECK(std::string(deserializedBlock.merkle_root_.c_str()) == std::string(originalBlock.merkle_root_.c_str()));

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
        dp::ByteBuf chainData = chain.serialize();
        CHECK_FALSE(chainData.empty());

        // Simulate retrieving from database
        auto deser_result = chain::Chain<StorageTestData>::deserialize(chainData);
        REQUIRE(deser_result.is_ok());
        chain::Chain<StorageTestData> retrievedChain = std::move(deser_result.value());

        CHECK(retrievedChain.getChainLength() == 11); // Including genesis
        auto valid_result = retrievedChain.isValid();
        CHECK(valid_result.is_ok());
        CHECK(valid_result.value());

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
        std::vector<dp::ByteBuf> archivedBlocks;
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
        dp::ByteBuf snapshot = chain.serialize();
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
        auto deser_result = chain::Chain<StorageTestData>::deserialize(snapshot);
        REQUIRE(deser_result.is_ok());
        chain::Chain<StorageTestData> restoredChain = std::move(deser_result.value());

        CHECK(restoredChain.getChainLength() == 21); // Back to snapshot point
        auto valid_result = restoredChain.isValid();
        CHECK(valid_result.is_ok());
        CHECK(valid_result.value());

        std::cout << "State snapshots simulation test passed! Restored to block height "
                  << restoredChain.getChainLength() << std::endl;
    }

    TEST_CASE("Serialization size and efficiency") {
        auto privateKey = std::make_shared<chain::Crypto>("perf_test_key");

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

        // Measure binary serialization size
        dp::ByteBuf binaryData = block.serialize();
        CHECK_FALSE(binaryData.empty());

        // Test deserialization
        auto bin_deser = chain::Block<StorageTestData>::deserialize(binaryData);
        REQUIRE(bin_deser.is_ok());
        chain::Block<StorageTestData> deserializedBlock = std::move(bin_deser.value());

        // Verify results
        CHECK(deserializedBlock.transactions_.size() == block.transactions_.size());
        CHECK(std::string(deserializedBlock.hash_.c_str()) == std::string(block.hash_.c_str()));
        CHECK(std::string(deserializedBlock.merkle_root_.c_str()) == std::string(block.merkle_root_.c_str()));

        // Report size metrics
        std::cout << "=== Binary Serialization Size ===" << std::endl;
        std::cout << "Binary size: " << binaryData.size() << " bytes" << std::endl;
        std::cout << "Transactions: " << block.transactions_.size() << std::endl;
        std::cout << "Bytes per transaction: " << binaryData.size() / block.transactions_.size() << std::endl;

        std::cout << "Serialization efficiency test passed!" << std::endl;
    }

    TEST_CASE("Round-trip serialization integrity") {
        auto privateKey = std::make_shared<chain::Crypto>("roundtrip_key");

        // Create a chain with mixed operations
        chain::Chain<StorageTestData> originalChain("roundtrip-chain", "genesis",
                                                    StorageTestData{"genesis_roundtrip", 0.0}, privateKey);

        // Add several blocks with different data
        for (int i = 1; i <= 5; i++) {
            chain::Transaction<StorageTestData> tx("roundtrip-tx-" + std::to_string(i),
                                                   StorageTestData{"roundtrip_data_" + std::to_string(i), i * 2.5},
                                                   100 + i);
            tx.signTransaction(privateKey);
            chain::Block<StorageTestData> block({tx});
            originalChain.addBlock(block);
        }

        // Test binary serialization round-trip
        dp::ByteBuf chainBinary = originalChain.serialize();
        CHECK_FALSE(chainBinary.empty());

        // Deserialize the chain
        auto chain_deser = chain::Chain<StorageTestData>::deserialize(chainBinary);
        REQUIRE(chain_deser.is_ok());
        chain::Chain<StorageTestData> deserializedChain = std::move(chain_deser.value());

        CHECK(std::string(deserializedChain.uuid_.c_str()) == std::string(originalChain.uuid_.c_str()));
        CHECK(deserializedChain.getChainLength() == originalChain.getChainLength());
        auto valid_result = deserializedChain.isValid();
        CHECK(valid_result.is_ok());
        CHECK(valid_result.value());

        // Test individual transaction serialization round-trip
        auto &testTx = originalChain.blocks_[1].transactions_[0];

        dp::ByteBuf txBinary = testTx.serialize();
        auto tx_deser = chain::Transaction<StorageTestData>::deserialize(txBinary);
        REQUIRE(tx_deser.is_ok());
        chain::Transaction<StorageTestData> txFromBinary = std::move(tx_deser.value());

        CHECK(std::string(txFromBinary.uuid_.c_str()) == std::string(testTx.uuid_.c_str()));
        CHECK(std::string(txFromBinary.function_.identifier.c_str()) ==
              std::string(testTx.function_.identifier.c_str()));
        CHECK(txFromBinary.function_.value == testTx.function_.value);
        CHECK(txFromBinary.priority_ == testTx.priority_);

        // Test individual block serialization round-trip
        auto &testBlock = originalChain.blocks_[1];

        dp::ByteBuf blockBinary = testBlock.serialize();
        auto block_deser = chain::Block<StorageTestData>::deserialize(blockBinary);
        REQUIRE(block_deser.is_ok());
        chain::Block<StorageTestData> blockFromBinary = std::move(block_deser.value());

        CHECK(blockFromBinary.index_ == testBlock.index_);
        CHECK(std::string(blockFromBinary.hash_.c_str()) == std::string(testBlock.hash_.c_str()));
        CHECK(std::string(blockFromBinary.merkle_root_.c_str()) == std::string(testBlock.merkle_root_.c_str()));
        CHECK(blockFromBinary.transactions_.size() == testBlock.transactions_.size());

        std::cout << "=== Round-trip Serialization Test Results ===" << std::endl;
        std::cout << "Chain binary size: " << chainBinary.size() << " bytes" << std::endl;
        std::cout << "Transaction binary size: " << txBinary.size() << " bytes" << std::endl;
        std::cout << "Block binary size: " << blockBinary.size() << " bytes" << std::endl;

        std::cout << "Round-trip serialization integrity test passed!" << std::endl;
    }
}
