#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <memory>
#include <thread>
#include <vector>

struct BlockTestData {
    std::string content;
    int value;

    std::string to_string() const { return "BlockTestData{" + content + "," + std::to_string(value) + "}"; }
};

TEST_SUITE("Block Tests") {
    TEST_CASE("Block creation with transactions") {
        auto privateKey = std::make_shared<chain::Crypto>("block_test_key");

        // Create transactions
        std::vector<chain::Transaction<BlockTestData>> transactions;

        chain::Transaction<BlockTestData> tx1("tx-1", BlockTestData{"data1", 10}, 100);
        tx1.signTransaction(privateKey);
        transactions.push_back(tx1);

        chain::Transaction<BlockTestData> tx2("tx-2", BlockTestData{"data2", 20}, 150);
        tx2.signTransaction(privateKey);
        transactions.push_back(tx2);

        // Create block
        chain::Block<BlockTestData> block(transactions);

        CHECK(block.index_ == 0); // Genesis block
        CHECK(block.previous_hash_ == "GENESIS");
        CHECK(block.transactions_.size() == 2);
        CHECK_FALSE(block.hash_.empty());
        CHECK_FALSE(block.merkle_root_.empty());
        CHECK(block.nonce_ == 0);
    }

    TEST_CASE("Block hash calculation") {
        auto privateKey = std::make_shared<chain::Crypto>("hash_test_key");

        chain::Transaction<BlockTestData> tx("tx-hash", BlockTestData{"hash_test", 42}, 200);
        tx.signTransaction(privateKey);

        chain::Block<BlockTestData> block({tx});

        std::string originalHash = block.hash_;
        std::string calculatedHash = block.calculateHash();

        CHECK(originalHash == calculatedHash);
        CHECK_FALSE(originalHash.empty());
    }

    TEST_CASE("Block validation") {
        auto privateKey = std::make_shared<chain::Crypto>("validation_test_key");

        chain::Transaction<BlockTestData> tx("tx-valid", BlockTestData{"valid_data", 99}, 180);
        tx.signTransaction(privateKey);

        chain::Block<BlockTestData> block({tx});

        CHECK(block.isValid());

        // Test with invalid conditions
        chain::Block<BlockTestData> invalidBlock;
        invalidBlock.index_ = -1; // Invalid index
        CHECK_FALSE(invalidBlock.isValid());
    }

    TEST_CASE("Merkle tree integration in blocks") {
        auto privateKey = std::make_shared<chain::Crypto>("merkle_test_key");

        std::vector<chain::Transaction<BlockTestData>> transactions;
        for (int i = 0; i < 5; i++) {
            chain::Transaction<BlockTestData> tx("tx-" + std::to_string(i),
                                                 BlockTestData{"data" + std::to_string(i), i}, 100 + i);
            tx.signTransaction(privateKey);
            transactions.push_back(tx);
        }

        chain::Block<BlockTestData> block(transactions);

        // Check that Merkle tree was built
        CHECK_FALSE(block.merkle_root_.empty());
        CHECK(block.transactions_.size() == 5);

        // Verify transactions using Merkle tree
        for (size_t i = 0; i < transactions.size(); i++) {
            bool verified = block.verifyTransaction(i);
            CHECK(verified);
        }
    }

    TEST_CASE("Empty block creation") {
        std::vector<chain::Transaction<BlockTestData>> empty_transactions;
        chain::Block<BlockTestData> block(empty_transactions);

        CHECK(block.transactions_.empty());
        CHECK(block.merkle_root_.empty());
    }

    TEST_CASE("Block timestamp precision") {
        auto privateKey = std::make_shared<chain::Crypto>("timestamp_key");

        chain::Transaction<BlockTestData> tx("tx-time", BlockTestData{"time_test", 1}, 100);
        tx.signTransaction(privateKey);

        chain::Block<BlockTestData> block1({tx});

        // Small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        chain::Block<BlockTestData> block2({tx});

        // Timestamps should be different
        CHECK(block1.timestamp_.sec <= block2.timestamp_.sec);
        if (block1.timestamp_.sec == block2.timestamp_.sec) {
            CHECK(block1.timestamp_.nanosec < block2.timestamp_.nanosec);
        }
    }

    TEST_CASE("Block hash changes with content") {
        auto privateKey = std::make_shared<chain::Crypto>("content_test_key");

        chain::Transaction<BlockTestData> tx1("tx-content1", BlockTestData{"content1", 1}, 100);
        tx1.signTransaction(privateKey);

        chain::Transaction<BlockTestData> tx2("tx-content2", BlockTestData{"content2", 2}, 100);
        tx2.signTransaction(privateKey);

        chain::Block<BlockTestData> block1({tx1});
        chain::Block<BlockTestData> block2({tx2});

        CHECK(block1.hash_ != block2.hash_);
        CHECK(block1.merkle_root_ != block2.merkle_root_);
    }
}
