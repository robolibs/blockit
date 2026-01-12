/**
 * Unified API Demo - Using blockit::Blockit for joined blockchain + database operations
 *
 * This shows the recommended way to use Blockit where blockchain and database
 * are managed together atomically.
 */

#include <blockit/blockit_store.hpp>
#include <iostream>

// Your custom data type
struct UserData {
    std::string name;
    std::string email;
    int age;

    std::string to_string() const { return "User{" + name + ", " + email + ", " + std::to_string(age) + "}"; }

    // Serialize to bytes for database storage and anchoring
    std::vector<uint8_t> toBytes() const {
        std::string data = name + "|" + email + "|" + std::to_string(age);
        return std::vector<uint8_t>(data.begin(), data.end());
    }
};

// Define your custom database schema
class UserSchema : public blockit::storage::ISchemaExtension {
  public:
    std::vector<std::string> getCreateTableStatements() const override {
        return {
            R"(CREATE TABLE IF NOT EXISTS users (
                user_id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL,
                age INTEGER NOT NULL,
                created_at INTEGER NOT NULL
            ))"};
    }

    std::vector<std::string> getCreateIndexStatements() const override {
        return {"CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)",
                "CREATE INDEX IF NOT EXISTS idx_users_age ON users(age)"};
    }
};

int main() {
    std::cout << "=== Unified Blockit API Demo ===\n\n";

    // 1. Create unified store (manages both blockchain and database)
    blockit::Blockit<UserData> store;

    // 2. Initialize with database and blockchain
    auto crypto = std::make_shared<blockit::ledger::Crypto>("demo_key");

    UserData genesis{"Genesis User", "genesis@blockit.io", 0};

    if (!store.initialize("unified_demo.db", "UserChain", "genesis_tx", genesis, crypto)) {
        std::cerr << "Failed to initialize store\n";
        return 1;
    }
    std::cout << "✓ Unified store initialized\n";

    // 3. Register your custom schema
    UserSchema schema;
    if (!store.registerSchema(schema)) {
        std::cerr << "Failed to register schema\n";
        return 1;
    }
    std::cout << "✓ User schema registered\n\n";

    // 4. Create transactions with automatic database integration
    std::cout << "Creating users...\n";

    // User 1
    UserData alice{"Alice Smith", "alice@example.com", 30};
    std::string alice_id = "user_001";

    // Insert into database
    std::string sql1 = "INSERT INTO users (user_id, name, email, age, created_at) VALUES ('" + alice_id + "', '" +
                       alice.name + "', '" + alice.email + "', " + std::to_string(alice.age) + ", " +
                       std::to_string(blockit::storage::currentTimestamp()) + ")";
    store.getStorage().executeSql(sql1);

    // Register for anchoring (links blockchain tx to database record)
    if (!store.createTransaction("tx_001", alice, alice_id, alice.toBytes(), 100)) {
        std::cerr << "Failed to create transaction\n";
        return 1;
    }
    std::cout << "  ✓ Alice registered (pending anchor)\n";

    // User 2
    UserData bob{"Bob Johnson", "bob@example.com", 25};
    std::string bob_id = "user_002";

    std::string sql2 = "INSERT INTO users (user_id, name, email, age, created_at) VALUES ('" + bob_id + "', '" +
                       bob.name + "', '" + bob.email + "', " + std::to_string(bob.age) + ", " +
                       std::to_string(blockit::storage::currentTimestamp()) + ")";
    store.getStorage().executeSql(sql2);

    if (!store.createTransaction("tx_002", bob, bob_id, bob.toBytes(), 100)) {
        std::cerr << "Failed to create transaction\n";
        return 1;
    }
    std::cout << "  ✓ Bob registered (pending anchor)\n\n";

    // 5. Create blockchain transactions
    std::vector<blockit::ledger::Transaction<UserData>> transactions;

    blockit::ledger::Transaction<UserData> tx1("tx_001", alice, 100);
    tx1.signTransaction(crypto);
    transactions.push_back(tx1);

    blockit::ledger::Transaction<UserData> tx2("tx_002", bob, 100);
    tx2.signTransaction(crypto);
    transactions.push_back(tx2);

    // 6. Add block - THIS IS WHERE THE MAGIC HAPPENS!
    // One call commits to both blockchain AND creates database anchors atomically
    std::cout << "Adding block (commits blockchain + creates anchors)...\n";
    if (!store.addBlock(transactions)) {
        std::cerr << "Failed to add block\n";
        return 1;
    }
    std::cout << "  ✓ Block added\n";
    std::cout << "  ✓ Transactions stored\n";
    std::cout << "  ✓ Anchors created\n\n";

    // 7. Verify the anchors work
    std::cout << "Verifying data integrity...\n";

    if (store.verifyContent(alice_id, alice.toBytes())) {
        std::cout << "  ✓ Alice verified against blockchain\n";
    } else {
        std::cout << "  ✗ Alice verification failed\n";
    }

    if (store.verifyContent(bob_id, bob.toBytes())) {
        std::cout << "  ✓ Bob verified against blockchain\n";
    } else {
        std::cout << "  ✗ Bob verification failed\n";
    }

    // 8. Query anchor info
    std::cout << "\nAnchor information:\n";
    auto alice_anchor = store.getAnchor(alice_id);
    if (alice_anchor) {
        std::cout << "  Alice anchored in:\n";
        std::cout << "    - Transaction: " << alice_anchor->tx_id << "\n";
        std::cout << "    - Block height: " << alice_anchor->block_height << "\n";
        std::cout << "    - Timestamp: " << alice_anchor->anchored_at << "\n";
    }

    // 9. Statistics
    std::cout << "\nStatistics:\n";
    std::cout << "  Blocks: " << store.getBlockCount() << "\n";
    std::cout << "  Transactions: " << store.getTransactionCount() << "\n";
    std::cout << "  Anchors: " << store.getAnchorCount() << "\n";

    // 10. Verify consistency
    if (store.verifyConsistency()) {
        std::cout << "\n✓ Blockchain and database are consistent\n";
    } else {
        std::cout << "\n✗ Consistency check failed\n";
    }

    // 11. Query database directly
    std::cout << "\nQuerying database:\n";
    store.getStorage().executeQuery(
        "SELECT user_id, name, email FROM users ORDER BY age",
        [](const std::vector<std::string> &row) { std::cout << "  - " << row[1] << " (" << row[2] << ")\n"; });

    // 12. Access blockchain directly if needed
    std::cout << "\nBlockchain info:\n";
    std::cout << "  Chain UUID: " << store.getChain().uuid_ << "\n";
    std::cout << "  Total blocks: " << store.getChain().blocks_.size() << "\n";
    std::cout << "  Valid: " << (store.getChain().isValid() ? "Yes" : "No") << "\n";

    std::cout << "\n=== Demo completed successfully ===\n";

    return 0;
}
