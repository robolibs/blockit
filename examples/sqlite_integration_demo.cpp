/**
 * Example: Using Blockit with SQLite for fast queries + blockchain anchoring
 *
 * This demo shows how to:
 * 1. Define your own domain schema (user records in this case)
 * 2. Integrate with Blockit's ledger for anchoring
 * 3. Query your data efficiently while maintaining blockchain verification
 */

#include <blockit/ledger/block.hpp>
#include <blockit/ledger/chain.hpp>
#include <blockit/ledger/transaction.hpp>
#include <blockit/storage/sqlite_store.hpp>
#include <iostream>
#include <memory>
#include <vector>

using namespace blockit::ledger;
using namespace blockit::storage;

// ===========================================
// Step 1: Define your domain data structure
// ===========================================

struct UserRecord {
    std::string user_id;
    std::string name;
    std::string email;
    int64_t created_at;

    std::string to_string() const { return user_id + "|" + name + "|" + email + "|" + std::to_string(created_at); }

    // Serialize to bytes for hashing
    std::vector<uint8_t> toBytes() const {
        std::string data = to_string();
        return std::vector<uint8_t>(data.begin(), data.end());
    }
};

// ===========================================
// Step 2: Define your custom schema extension
// ===========================================

class UserSchemaExtension : public ISchemaExtension {
  public:
    std::vector<std::string> getCreateTableStatements() const override {
        return {
            R"(CREATE TABLE IF NOT EXISTS users (
                user_id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE,
                created_at INTEGER NOT NULL,
                content_hash BLOB
            ))",

            // Optional: full-text search table
            R"(CREATE VIRTUAL TABLE IF NOT EXISTS users_fts USING fts5(
                user_id UNINDEXED,
                name,
                email,
                content=users,
                content_rowid=rowid
            ))"};
    }

    std::vector<std::string> getCreateIndexStatements() const override {
        return {"CREATE INDEX IF NOT EXISTS idx_users_created ON users(created_at)",
                "CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)"};
    }

    // Optional: migrations for schema version upgrades
    std::vector<std::pair<int32_t, std::vector<std::string>>> getMigrations() const override {
        return {// Version 2: add phone number field (example)
                {2, {"ALTER TABLE users ADD COLUMN phone TEXT"}}};
    }
};

// ===========================================
// Step 3: Application logic with anchoring
// ===========================================

class UserManager {
  private:
    SqliteStore &store_;

  public:
    explicit UserManager(SqliteStore &store) : store_(store) {}

    // Create user and prepare for blockchain anchoring
    bool createUser(const UserRecord &user, std::string &out_tx_id) {
        auto tx = store_.beginTransaction();

        try {
            // 1. Compute content hash
            auto content_bytes = user.toBytes();
            auto content_hash = computeSHA256(content_bytes);

            // 2. Store in your custom table using raw SQL
            std::string insert_sql = "INSERT INTO users (user_id, name, email, created_at, content_hash) VALUES ('" +
                                     user.user_id + "', '" + user.name + "', '" + user.email + "', " +
                                     std::to_string(user.created_at) + ", x'" + hashToHex(content_hash) + "')";

            if (!store_.executeSql(insert_sql)) {
                return false;
            }

            // 3. Generate transaction ID for blockchain
            out_tx_id = "tx_" + user.user_id + "_" + std::to_string(currentTimestamp());

            tx->commit();
            return true;

        } catch (const std::exception &e) {
            std::cerr << "Error creating user: " << e.what() << std::endl;
            return false;
        }
    }

    // Finalize anchor after blockchain confirmation
    bool anchorUser(const std::string &user_id, const TxRef &tx_ref) {
        // Get current user data to recompute hash
        std::vector<std::string> row;
        bool found = false;

        store_.executeQuery("SELECT name, email, created_at FROM users WHERE user_id = '" + user_id + "'",
                            [&](const std::vector<std::string> &r) {
                                if (r.size() >= 3) {
                                    row = r;
                                    found = true;
                                }
                            });

        if (!found) {
            return false;
        }

        // Reconstruct user to compute hash
        UserRecord user;
        user.user_id = user_id;
        user.name = row[0];
        user.email = row[1];
        user.created_at = std::stoll(row[2]);

        auto content_hash = computeSHA256(user.toBytes());

        // Create anchor
        return store_.createAnchor(user_id, content_hash, tx_ref);
    }

    // Verify user data integrity
    bool verifyUser(const std::string &user_id) {
        // Get current user data
        std::vector<std::string> row;
        bool found = false;

        store_.executeQuery("SELECT name, email, created_at FROM users WHERE user_id = '" + user_id + "'",
                            [&](const std::vector<std::string> &r) {
                                if (r.size() >= 3) {
                                    row = r;
                                    found = true;
                                }
                            });

        if (!found) {
            return false;
        }

        // Reconstruct user record
        UserRecord user;
        user.user_id = user_id;
        user.name = row[0];
        user.email = row[1];
        user.created_at = std::stoll(row[2]);

        // Verify against anchor
        return store_.verifyAnchor(user_id, user.toBytes());
    }

    // Full-text search on users
    std::vector<std::string> searchUsers(const std::string &query) {
        std::vector<std::string> results;

        std::string sql = "SELECT user_id FROM users_fts WHERE users_fts MATCH '" + query + "' LIMIT 100";

        store_.executeQuery(sql, [&](const std::vector<std::string> &row) {
            if (!row.empty()) {
                results.push_back(row[0]);
            }
        });

        return results;
    }

    // Range query on creation time
    std::vector<std::string> getUsersCreatedBetween(int64_t start, int64_t end) {
        std::vector<std::string> results;

        std::string sql = "SELECT user_id FROM users WHERE created_at BETWEEN " + std::to_string(start) + " AND " +
                          std::to_string(end) + " ORDER BY created_at DESC";

        store_.executeQuery(sql, [&](const std::vector<std::string> &row) {
            if (!row.empty()) {
                results.push_back(row[0]);
            }
        });

        return results;
    }
};

// ===========================================
// Demo application
// ===========================================

int main() {
    std::cout << "=== Blockit SQLite Integration Demo ===\n\n";

    // 1. Initialize storage
    SqliteStore store;
    OpenOptions opts;
    opts.enable_wal = true;
    opts.sync_mode = OpenOptions::Synchronous::NORMAL;

    if (!store.open("demo_ledger.db", opts)) {
        std::cerr << "Failed to open database\n";
        return 1;
    }

    std::cout << "✓ Database opened\n";

    // 2. Initialize core ledger schema
    if (!store.initializeCoreSchema()) {
        std::cerr << "Failed to initialize core schema\n";
        return 1;
    }

    std::cout << "✓ Core ledger schema initialized\n";

    // 3. Register custom user schema
    UserSchemaExtension user_schema;
    if (!store.registerExtension(user_schema)) {
        std::cerr << "Failed to register user schema\n";
        return 1;
    }

    std::cout << "✓ User schema extension registered\n\n";

    // 4. Create user manager
    UserManager user_mgr(store);

    // 5. Create some users
    std::cout << "Creating users...\n";

    UserRecord alice;
    alice.user_id = "user_001";
    alice.name = "Alice Smith";
    alice.email = "alice@example.com";
    alice.created_at = currentTimestamp();

    std::string alice_tx_id;
    if (user_mgr.createUser(alice, alice_tx_id)) {
        std::cout << "  ✓ Created user: " << alice.user_id << " (tx: " << alice_tx_id << ")\n";
    }

    UserRecord bob;
    bob.user_id = "user_002";
    bob.name = "Bob Johnson";
    bob.email = "bob@example.com";
    bob.created_at = currentTimestamp();

    std::string bob_tx_id;
    if (user_mgr.createUser(bob, bob_tx_id)) {
        std::cout << "  ✓ Created user: " << bob.user_id << " (tx: " << bob_tx_id << ")\n";
    }

    // 6. Simulate blockchain block creation
    std::cout << "\nSimulating blockchain block...\n";

    std::array<uint8_t, 32> merkle_root = {0x1, 0x2, 0x3}; // Simplified

    if (store.storeBlock(1, "blockhash_001", "GENESIS", "merkleroot_001", currentTimestamp(), 12345)) {
        std::cout << "  ✓ Block #1 stored\n";
    }

    if (store.storeTransaction(alice_tx_id, 1, alice.created_at, 100, alice.toBytes())) {
        std::cout << "  ✓ Transaction for Alice stored\n";
    }

    // 7. Anchor users to blockchain
    std::cout << "\nAnchoring users to blockchain...\n";

    TxRef alice_ref(alice_tx_id, 1, merkle_root);
    if (user_mgr.anchorUser(alice.user_id, alice_ref)) {
        std::cout << "  ✓ Alice anchored to block #1\n";
    }

    // 8. Verify user integrity
    std::cout << "\nVerifying user integrity...\n";

    if (user_mgr.verifyUser(alice.user_id)) {
        std::cout << "  ✓ Alice's data verified successfully\n";
    } else {
        std::cout << "  ✗ Alice's data verification failed\n";
    }

    // 9. Query statistics
    std::cout << "\nLedger statistics:\n";
    std::cout << "  Blocks: " << store.getBlockCount() << "\n";
    std::cout << "  Transactions: " << store.getTransactionCount() << "\n";
    std::cout << "  Anchors: " << store.getAnchorCount() << "\n";
    std::cout << "  Latest block height: " << store.getLatestBlockHeight() << "\n";

    // 10. Range query example
    std::cout << "\nQuerying users created in last hour...\n";
    int64_t one_hour_ago = currentTimestamp() - 3600;
    auto recent_users = user_mgr.getUsersCreatedBetween(one_hour_ago, currentTimestamp());
    std::cout << "  Found " << recent_users.size() << " users\n";

    // 11. Verify chain continuity
    if (store.verifyChainContinuity()) {
        std::cout << "\n✓ Blockchain continuity verified\n";
    }

    // 12. Database health check
    if (store.quickCheck()) {
        std::cout << "✓ Database integrity check passed\n";
    }

    store.close();
    std::cout << "\n=== Demo completed successfully ===\n";

    return 0;
}
