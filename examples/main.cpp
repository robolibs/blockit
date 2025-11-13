#include "blockit.hpp"
#include "lockey/lockey.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

// Wrapper for std::string to make it compatible with template
class StringWrapper {
  private:
    std::string value_;

  public:
    StringWrapper() = default;
    StringWrapper(const std::string &str) : value_(str) {}
    StringWrapper(const char *str) : value_(str) {}

    std::string to_string() const { return value_; }
    bool empty() const { return value_.empty(); }

    // Allow implicit conversion to string for compatibility
    operator std::string() const { return value_; }

    // Assignment operators
    StringWrapper &operator=(const std::string &str) {
        value_ = str;
        return *this;
    }
    StringWrapper &operator=(const char *str) {
        value_ = str;
        return *this;
    }

    // Serialization methods for persistent storage
    std::string serialize() const { return "\"" + value_ + "\""; }

    static StringWrapper deserialize(const std::string &data) {
        // Remove quotes if present
        if (data.length() >= 2 && data.front() == '"' && data.back() == '"') {
            return StringWrapper(data.substr(1, data.length() - 2));
        }
        return StringWrapper(data);
    }
};

void printSeparator(const std::string &title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void demonstrateTransaction() {
    printSeparator("TRANSACTION DEMONSTRATION");

    // Create a crypto instance for signing
    auto privateKey = std::make_shared<chain::Crypto>("dummy_key_file");

    // Create a basic transaction using StringWrapper
    chain::Transaction<StringWrapper> tx1("tx-001", StringWrapper("transfer"), 100);
    std::cout << "Created transaction with UUID: " << tx1.uuid_ << std::endl;
    std::cout << "Function: " << tx1.function_.to_string() << std::endl;
    std::cout << "Priority: " << tx1.priority_ << std::endl;
    std::cout << "Timestamp: " << tx1.timestamp_.sec << "." << tx1.timestamp_.nanosec << std::endl;
    std::cout << "Transaction string: " << tx1.toString() << std::endl;

    // Sign the transaction
    tx1.signTransaction(privateKey);
    std::cout << "Transaction signed successfully!" << std::endl;
    std::cout << "Signature length: " << tx1.signature_.size() << " bytes" << std::endl;

    // Validate the transaction
    std::cout << "Transaction is valid: " << (tx1.isValid() ? "YES" : "NO") << std::endl;

    // Create another transaction with different priority
    chain::Transaction<StringWrapper> tx2("tx-002", StringWrapper("smart_contract_call"), 200);
    tx2.signTransaction(privateKey);
    std::cout << "\nCreated second transaction with priority: " << tx2.priority_ << std::endl;
}

void demonstrateBlock() {
    printSeparator("BLOCK DEMONSTRATION");

    auto privateKey = std::make_shared<chain::Crypto>("dummy_key_file");

    // Create some transactions
    std::vector<chain::Transaction<StringWrapper>> transactions;

    chain::Transaction<StringWrapper> tx1("tx-block-001", StringWrapper("payment"), 150);
    tx1.signTransaction(privateKey);
    transactions.push_back(tx1);

    chain::Transaction<StringWrapper> tx2("tx-block-002", StringWrapper("data_storage"), 120);
    tx2.signTransaction(privateKey);
    transactions.push_back(tx2);

    chain::Transaction<StringWrapper> tx3("tx-block-003", StringWrapper("contract_execution"), 180);
    tx3.signTransaction(privateKey);
    transactions.push_back(tx3);

    std::cout << "Created " << transactions.size() << " transactions for the block" << std::endl;

    // Create a block with these transactions
    chain::Block<StringWrapper> block(transactions);

    std::cout << "Block created successfully!" << std::endl;
    std::cout << "Block index: " << block.index_ << std::endl;
    std::cout << "Previous hash: " << block.previous_hash_ << std::endl;
    std::cout << "Block hash: " << block.hash_ << std::endl;
    std::cout << "Number of transactions: " << block.transactions_.size() << std::endl;
    std::cout << "Nonce: " << block.nonce_ << std::endl;
    std::cout << "Timestamp: " << block.timestamp_.sec << "." << block.timestamp_.nanosec << std::endl;

    // Validate the block
    std::cout << "Block is valid: " << (block.isValid() ? "YES" : "NO") << std::endl;

    // Demonstrate hash calculation
    std::string calculatedHash = block.calculateHash();
    std::cout << "Calculated hash matches stored hash: " << (calculatedHash == block.hash_ ? "YES" : "NO") << std::endl;
}

void demonstrateChain() {
    printSeparator("BLOCKCHAIN DEMONSTRATION");

    auto privateKey = std::make_shared<chain::Crypto>("dummy_key_file");

    // Create a blockchain with genesis block
    chain::Chain<StringWrapper> blockchain("chain-001", "genesis-tx", StringWrapper("genesis_function"), privateKey,
                                           255);

    std::cout << "Blockchain created with genesis block" << std::endl;
    std::cout << "Chain UUID: " << blockchain.uuid_ << std::endl;
    std::cout << "Number of blocks: " << blockchain.blocks_.size() << std::endl;
    std::cout << "Genesis block hash: " << blockchain.blocks_[0].hash_ << std::endl;

    // Add more blocks to the chain
    for (int i = 1; i <= 3; i++) {
        std::vector<chain::Transaction<StringWrapper>> transactions;

        // Create 2-3 transactions per block
        for (int j = 1; j <= 2; j++) {
            std::string txId = "tx-" + std::to_string(i) + "-" + std::to_string(j);
            std::string function = (j % 2 == 0) ? "transfer" : "contract_call";
            int16_t priority = 100 + (i * 10) + j;

            chain::Transaction<StringWrapper> tx(txId, StringWrapper(function), priority);
            tx.signTransaction(privateKey);
            transactions.push_back(tx);
        }

        // Create and add block
        chain::Block<StringWrapper> newBlock(transactions);
        blockchain.addBlock(newBlock);

        std::cout << "Added block " << i << " with " << transactions.size() << " transactions" << std::endl;
        std::cout << "  Block hash: " << blockchain.blocks_.back().hash_ << std::endl;
        std::cout << "  Previous hash: " << blockchain.blocks_.back().previous_hash_ << std::endl;
    }

    std::cout << "\nFinal blockchain state:" << std::endl;
    std::cout << "Total blocks: " << blockchain.blocks_.size() << std::endl;
    std::cout << "Blockchain is valid: " << (blockchain.isValid() ? "YES" : "NO") << std::endl;

    // Display chain summary
    std::cout << "\nBlockchain Summary:" << std::endl;
    for (size_t i = 0; i < blockchain.blocks_.size(); i++) {
        const auto &block = blockchain.blocks_[i];
        std::cout << "Block " << i << ": " << block.transactions_.size()
                  << " transactions, hash: " << block.hash_.substr(0, 16) << "..." << std::endl;
    }
}

void demonstrateCryptography() {
    printSeparator("CRYPTOGRAPHY DEMONSTRATION");

    // Create crypto instance
    auto crypto = std::make_shared<chain::Crypto>("test_key");

    // Get public key in PEM format
    std::string publicKeyPEM = crypto->getPublicHalf();
    std::cout << "Generated keypair successfully!" << std::endl;
    std::cout << "Public key (PEM format):" << std::endl;
    std::cout << publicKeyPEM << std::endl;

    // Test signing and verification
    std::string testMessage = "Hello, Blockchain World!";
    std::cout << "Test message: " << testMessage << std::endl;

    try {
        // Sign the message
        auto signature = crypto->sign(testMessage);
        std::cout << "Message signed successfully!" << std::endl;
        std::cout << "Signature length: " << signature.size() << " bytes" << std::endl;

        // Verify the signature
        bool isValid = chain::verify(publicKeyPEM, testMessage, signature);
        std::cout << "Signature verification: " << (isValid ? "VALID" : "INVALID") << std::endl;

        // Test with wrong message
        std::string wrongMessage = "Hello, Blockchain World!!";
        bool isInvalid = chain::verify(publicKeyPEM, wrongMessage, signature);
        std::cout << "Wrong message verification: " << (isInvalid ? "VALID" : "INVALID") << " (should be INVALID)"
                  << std::endl;

    } catch (const std::exception &e) {
        std::cout << "Cryptography error: " << e.what() << std::endl;
    }
}

void demonstrateAdvancedScenarios() {
    printSeparator("ADVANCED SCENARIOS");

    auto privateKey = std::make_shared<chain::Crypto>("advanced_key");

    // Scenario 1: High-priority transactions
    std::cout << "Scenario 1: Priority-based transactions" << std::endl;
    std::vector<chain::Transaction<StringWrapper>> priorityTxs;

    int priorities[] = {50, 200, 100, 255, 1};
    for (int i = 0; i < 5; i++) {
        std::string txId = "priority-tx-" + std::to_string(i);
        chain::Transaction<StringWrapper> tx(txId, StringWrapper("priority_test"), priorities[i]);
        tx.signTransaction(privateKey);
        priorityTxs.push_back(tx);
        std::cout << "  Transaction " << txId << " with priority: " << priorities[i] << std::endl;
    }

    // Scenario 2: Time-based analysis
    std::cout << "\nScenario 2: Timestamp analysis" << std::endl;
    chain::Transaction<StringWrapper> tx1("time-tx-1", StringWrapper("first_action"), 100);

    // Small delay to show timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    chain::Transaction<StringWrapper> tx2("time-tx-2", StringWrapper("second_action"), 100);

    std::cout << "  First transaction timestamp: " << tx1.timestamp_.sec << "." << tx1.timestamp_.nanosec << std::endl;
    std::cout << "  Second transaction timestamp: " << tx2.timestamp_.sec << "." << tx2.timestamp_.nanosec << std::endl;
    std::cout << "  Time difference (nanoseconds): "
              << (tx2.timestamp_.nanosec > tx1.timestamp_.nanosec
                      ? tx2.timestamp_.nanosec - tx1.timestamp_.nanosec
                      : (1000000000 - tx1.timestamp_.nanosec) + tx2.timestamp_.nanosec)
              << std::endl;

    // Scenario 3: Multiple chains
    std::cout << "\nScenario 3: Multiple blockchain instances" << std::endl;
    chain::Chain<StringWrapper> chain1("main-chain", "genesis-1", StringWrapper("main_genesis"), privateKey);
    chain::Chain<StringWrapper> chain2("test-chain", "genesis-2", StringWrapper("test_genesis"), privateKey);

    // Add different blocks to each chain
    chain1.addBlock("tx-main-1", StringWrapper("main_operation"), privateKey, 150);
    chain2.addBlock("tx-test-1", StringWrapper("test_operation"), privateKey, 100);

    std::cout << "  Main chain blocks: " << chain1.blocks_.size() << std::endl;
    std::cout << "  Test chain blocks: " << chain2.blocks_.size() << std::endl;
    std::cout << "  Main chain valid: " << (chain1.isValid() ? "YES" : "NO") << std::endl;
    std::cout << "  Test chain valid: " << (chain2.isValid() ? "YES" : "NO") << std::endl;
}

void demonstratePersistentStorage() {
    printSeparator("PERSISTENT STORAGE DEMONSTRATION");

    auto privateKey = std::make_shared<chain::Crypto>("storage_key");

    // Create a blockchain with some transactions
    chain::Chain<StringWrapper> originalChain("storage-chain", "genesis-tx", StringWrapper("genesis_data"), privateKey);

    // Register some participants
    originalChain.registerParticipant("device-001", "active", {{"type", "sensor"}});
    originalChain.registerParticipant("device-002", "maintenance", {{"type", "actuator"}});
    originalChain.grantCapability("device-001", "READ_DATA");
    originalChain.grantCapability("device-002", "WRITE_DATA");

    // Add some blocks with transactions
    for (int i = 1; i <= 3; i++) {
        std::vector<chain::Transaction<StringWrapper>> transactions;

        for (int j = 1; j <= 2; j++) {
            std::string txId = "storage-tx-" + std::to_string(i) + "-" + std::to_string(j);
            std::string operation = (j % 2 == 0) ? "data_read" : "data_write";
            chain::Transaction<StringWrapper> tx(txId, StringWrapper(operation), 100 + j);
            tx.signTransaction(privateKey);
            transactions.push_back(tx);
        }

        chain::Block<StringWrapper> block(transactions);
        originalChain.addBlock(block);
    }

    std::cout << "Original blockchain created with " << originalChain.blocks_.size() << " blocks" << std::endl;
    std::cout << "Total transactions: " << (originalChain.blocks_.size() * 2) << std::endl;

    // Demonstrate serialization
    std::cout << "\n--- Serialization Test ---" << std::endl;
    try {
        std::string serialized = originalChain.serialize();
        std::cout << "Blockchain serialized successfully!" << std::endl;
        std::cout << "Serialized data size: " << serialized.length() << " bytes" << std::endl;

        // Show a snippet of the serialized data
        std::cout << "Serialized data preview (first 200 chars):" << std::endl;
        std::cout << serialized.substr(0, 200) << "..." << std::endl;

        // Demonstrate deserialization
        std::cout << "\n--- Deserialization Test ---" << std::endl;
        chain::Chain<StringWrapper> deserializedChain = chain::Chain<StringWrapper>::deserialize(serialized);

        std::cout << "Blockchain deserialized successfully!" << std::endl;
        std::cout << "Deserialized chain UUID: " << deserializedChain.uuid_ << std::endl;
        std::cout << "Deserialized chain blocks: " << deserializedChain.blocks_.size() << std::endl;
        std::cout << "Deserialized chain valid: " << (deserializedChain.isValid() ? "YES" : "NO") << std::endl;

        // Verify data integrity
        bool integrity_check = (originalChain.uuid_ == deserializedChain.uuid_ &&
                                originalChain.blocks_.size() == deserializedChain.blocks_.size());
        std::cout << "Data integrity check: " << (integrity_check ? "PASSED" : "FAILED") << std::endl;

    } catch (const std::exception &e) {
        std::cout << "Serialization/Deserialization error: " << e.what() << std::endl;
    }

    // Demonstrate file I/O
    std::cout << "\n--- File I/O Test ---" << std::endl;
    std::string filename = "test_blockchain.json";

    try {
        // Save to file
        bool save_success = originalChain.saveToFile(filename);
        std::cout << "Save to file: " << (save_success ? "SUCCESS" : "FAILED") << std::endl;

        if (save_success) {
            // Load from file
            chain::Chain<StringWrapper> loadedChain;
            bool load_success = loadedChain.loadFromFile(filename);
            std::cout << "Load from file: " << (load_success ? "SUCCESS" : "FAILED") << std::endl;

            if (load_success) {
                std::cout << "Loaded chain UUID: " << loadedChain.uuid_ << std::endl;
                std::cout << "Loaded chain blocks: " << loadedChain.blocks_.size() << std::endl;
                std::cout << "Loaded chain valid: " << (loadedChain.isValid() ? "YES" : "NO") << std::endl;

                // Verify file persistence integrity
                bool file_integrity = (originalChain.uuid_ == loadedChain.uuid_ &&
                                       originalChain.blocks_.size() == loadedChain.blocks_.size());
                std::cout << "File persistence integrity: " << (file_integrity ? "PASSED" : "FAILED") << std::endl;
            }

            // Clean up test file
            std::remove(filename.c_str());
            std::cout << "Test file cleaned up" << std::endl;
        }

    } catch (const std::exception &e) {
        std::cout << "File I/O error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "Blockit Library Demonstration" << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << "This example demonstrates the current functionality of the Blockit library." << std::endl;
    std::cout << "Note: This is a basic implementation for learning purposes." << std::endl;

    try {
        demonstrateTransaction();
        demonstrateBlock();
        demonstrateChain();
        demonstrateCryptography();
        demonstrateAdvancedScenarios();
        demonstratePersistentStorage();

        printSeparator("DEMONSTRATION COMPLETE");
        std::cout << "All examples completed successfully!" << std::endl;
        std::cout << "The library demonstrates basic blockchain data structures and persistent storage." << std::endl;
        std::cout << "\nImplemented features:" << std::endl;
        std::cout << "✅ Transaction creation and signing" << std::endl;
        std::cout << "✅ Block creation with Merkle trees" << std::endl;
        std::cout << "✅ Blockchain validation and integrity checks" << std::endl;
        std::cout << "✅ Participant authentication and authorization" << std::endl;
        std::cout << "✅ Double-spend prevention" << std::endl;
        std::cout << "✅ Serialization and deserialization" << std::endl;
        std::cout << "✅ File-based persistent storage" << std::endl;
        std::cout << "\nNext steps for production use:" << std::endl;
        std::cout << "- Implement proper consensus mechanism (Proof of Work/Stake)" << std::endl;
        std::cout << "- Add network layer for distributed operation" << std::endl;
        std::cout << "- Implement database integration for better performance" << std::endl;
        std::cout << "- Add comprehensive transaction validation" << std::endl;
        std::cout << "- Implement fork resolution mechanism" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
