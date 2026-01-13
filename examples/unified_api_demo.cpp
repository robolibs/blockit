/**
 * Unified API Demo - Using blockit::Blockit for joined blockchain + database operations
 *
 * This shows the recommended way to use Blockit where blockchain and storage
 * are managed together atomically.
 */

#include <blockit/storage/blockit_store.hpp>
#include <filesystem>
#include <iostream>

using namespace datapod;

// Your custom data type
struct UserData {
    std::string name;
    std::string email;
    int age;

    std::string to_string() const { return "User{" + name + ", " + email + ", " + std::to_string(age) + "}"; }

    // Serialize to bytes for storage and anchoring
    Vector<u8> toBytes() const {
        std::string data = name + "|" + email + "|" + std::to_string(age);
        return Vector<u8>(data.begin(), data.end());
    }
};

int main() {
    std::cout << "=== Unified Blockit API Demo ===\n\n";

    // Cleanup previous run
    const std::string storage_path = "unified_demo_store";
    if (std::filesystem::exists(storage_path)) {
        std::filesystem::remove_all(storage_path);
    }

    // 1. Create unified store (manages both blockchain and storage)
    blockit::Blockit<UserData> store;

    // 2. Initialize with storage and blockchain
    auto crypto = std::make_shared<blockit::Crypto>("demo_key");

    UserData genesis{"Genesis User", "genesis@blockit.io", 0};

    auto init_result =
        store.initialize(String(storage_path.c_str()), String("UserChain"), String("genesis_tx"), genesis, crypto);
    if (!init_result.is_ok()) {
        std::cerr << "Failed to initialize store\n";
        return 1;
    }
    std::cout << "[OK] Unified store initialized\n\n";

    // 3. Create transactions with automatic storage integration
    std::cout << "Creating users...\n";

    // User 1
    UserData alice{"Alice Smith", "alice@example.com", 30};
    String alice_id("user_001");

    // Register for anchoring (links blockchain tx to storage record)
    auto tx1_result = store.createTransaction(String("tx_001"), alice, alice_id, alice.toBytes(), 100);
    if (!tx1_result.is_ok()) {
        std::cerr << "Failed to create transaction\n";
        return 1;
    }
    std::cout << "  [OK] Alice registered (pending anchor)\n";

    // User 2
    UserData bob{"Bob Johnson", "bob@example.com", 25};
    String bob_id("user_002");

    auto tx2_result = store.createTransaction(String("tx_002"), bob, bob_id, bob.toBytes(), 100);
    if (!tx2_result.is_ok()) {
        std::cerr << "Failed to create transaction\n";
        return 1;
    }
    std::cout << "  [OK] Bob registered (pending anchor)\n\n";

    // 4. Create blockchain transactions
    std::vector<blockit::Transaction<UserData>> transactions;

    blockit::Transaction<UserData> tx1("tx_001", alice, 100);
    tx1.signTransaction(crypto);
    transactions.push_back(tx1);

    blockit::Transaction<UserData> tx2("tx_002", bob, 100);
    tx2.signTransaction(crypto);
    transactions.push_back(tx2);

    // 5. Add block - THIS IS WHERE THE MAGIC HAPPENS!
    // One call commits to both blockchain AND creates storage anchors atomically
    std::cout << "Adding block (commits blockchain + creates anchors)...\n";
    auto add_result = store.addBlock(transactions);
    if (!add_result.is_ok()) {
        std::cerr << "Failed to add block\n";
        return 1;
    }
    std::cout << "  [OK] Block added\n";
    std::cout << "  [OK] Transactions stored\n";
    std::cout << "  [OK] Anchors created\n\n";

    // 6. Verify the anchors work
    std::cout << "Verifying data integrity...\n";

    auto alice_verify = store.verifyContent(alice_id, alice.toBytes());
    if (alice_verify.is_ok() && alice_verify.value()) {
        std::cout << "  [OK] Alice verified against blockchain\n";
    } else {
        std::cout << "  [FAIL] Alice verification failed\n";
    }

    auto bob_verify = store.verifyContent(bob_id, bob.toBytes());
    if (bob_verify.is_ok() && bob_verify.value()) {
        std::cout << "  [OK] Bob verified against blockchain\n";
    } else {
        std::cout << "  [FAIL] Bob verification failed\n";
    }

    // 7. Query anchor info
    std::cout << "\nAnchor information:\n";
    auto alice_anchor = store.getAnchor(alice_id);
    if (alice_anchor.has_value()) {
        std::cout << "  Alice anchored in:\n";
        std::cout << "    - Transaction: " << alice_anchor->tx_id.c_str() << "\n";
        std::cout << "    - Block height: " << alice_anchor->block_height << "\n";
        std::cout << "    - Timestamp: " << alice_anchor->anchored_at << "\n";
    }

    // 8. Statistics
    std::cout << "\nStatistics:\n";
    std::cout << "  Blocks: " << store.getBlockCount() << "\n";
    std::cout << "  Transactions: " << store.getTransactionCount() << "\n";
    std::cout << "  Anchors: " << store.getAnchorCount() << "\n";

    // 9. Verify consistency
    auto consistency = store.verifyConsistency();
    if (consistency.is_ok() && consistency.value()) {
        std::cout << "\n[OK] Blockchain and storage are consistent\n";
    } else {
        std::cout << "\n[FAIL] Consistency check failed\n";
    }

    // 10. Access blockchain directly if needed
    std::cout << "\nBlockchain info:\n";
    std::cout << "  Chain UUID: " << store.getChain().uuid_ << "\n";
    std::cout << "  Total blocks: " << store.getChain().blocks_.size() << "\n";
    std::cout << "  Valid: " << (store.getChain().isValid() ? "Yes" : "No") << "\n";

    std::cout << "\n=== Demo completed successfully ===\n";

    // Cleanup
    std::filesystem::remove_all(storage_path);

    return 0;
}
