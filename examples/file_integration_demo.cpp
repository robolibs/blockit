/**
 * Example: Using Blockit with file-based storage for blockchain anchoring
 *
 * This demo shows how to:
 * 1. Define your own domain data structure
 * 2. Integrate with Blockit's ledger for anchoring
 * 3. Store and verify data with blockchain verification
 */

#include <blockit/ledger/block.hpp>
#include <blockit/ledger/chain.hpp>
#include <blockit/ledger/transaction.hpp>
#include <blockit/storage/file_store.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

using namespace blockit::ledger;
using namespace blockit::storage;
using namespace datapod;

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
    Vector<u8> toBytes() const {
        std::string data = to_string();
        return Vector<u8>(data.begin(), data.end());
    }
};

// ===========================================
// Step 2: Application logic with anchoring
// ===========================================

class UserManager {
  private:
    FileStore &store_;
    std::map<std::string, UserRecord> users_; // In-memory cache

  public:
    explicit UserManager(FileStore &store) : store_(store) {}

    // Create user and prepare for blockchain anchoring
    bool createUser(const UserRecord &user, std::string &out_tx_id) {
        try {
            // 1. Store user in memory cache
            users_[user.user_id] = user;

            // 2. Generate transaction ID for blockchain
            out_tx_id = "tx_" + user.user_id + "_" + std::to_string(currentTimestamp());

            return true;

        } catch (const std::exception &e) {
            std::cerr << "Error creating user: " << e.what() << std::endl;
            return false;
        }
    }

    // Finalize anchor after blockchain confirmation
    bool anchorUser(const std::string &user_id, const TxRef &tx_ref) {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            return false;
        }

        auto content_hash = computeSHA256(it->second.toBytes());
        return store_.createAnchor(String(user_id.c_str()), content_hash, tx_ref).is_ok();
    }

    // Verify user data integrity
    bool verifyUser(const std::string &user_id) {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            return false;
        }

        auto result = store_.verifyAnchor(String(user_id.c_str()), it->second.toBytes());
        return result.is_ok() && result.value();
    }

    // Get user by ID
    std::optional<UserRecord> getUser(const std::string &user_id) const {
        auto it = users_.find(user_id);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// ===========================================
// Demo application
// ===========================================

int main() {
    std::cout << "=== Blockit File-Based Storage Demo ===\n\n";

    // Cleanup previous run
    const std::string storage_path = "demo_ledger_store";
    if (std::filesystem::exists(storage_path)) {
        std::filesystem::remove_all(storage_path);
    }

    // 1. Initialize storage
    FileStore store;
    OpenOptions opts;
    opts.sync_mode = OpenOptions::Synchronous::NORMAL;

    if (!store.open(String(storage_path.c_str()), opts).is_ok()) {
        std::cerr << "Failed to open storage\n";
        return 1;
    }

    std::cout << "[OK] Storage opened at: " << storage_path << "\n";

    // 2. Initialize core ledger schema
    if (!store.initializeCoreSchema().is_ok()) {
        std::cerr << "Failed to initialize core schema\n";
        return 1;
    }

    std::cout << "[OK] Core ledger schema initialized\n\n";

    // 3. Create user manager
    UserManager user_mgr(store);

    // 4. Create some users
    std::cout << "Creating users...\n";

    UserRecord alice;
    alice.user_id = "user_001";
    alice.name = "Alice Smith";
    alice.email = "alice@example.com";
    alice.created_at = currentTimestamp();

    std::string alice_tx_id;
    if (user_mgr.createUser(alice, alice_tx_id)) {
        std::cout << "  [OK] Created user: " << alice.user_id << " (tx: " << alice_tx_id << ")\n";
    }

    UserRecord bob;
    bob.user_id = "user_002";
    bob.name = "Bob Johnson";
    bob.email = "bob@example.com";
    bob.created_at = currentTimestamp();

    std::string bob_tx_id;
    if (user_mgr.createUser(bob, bob_tx_id)) {
        std::cout << "  [OK] Created user: " << bob.user_id << " (tx: " << bob_tx_id << ")\n";
    }

    // 5. Simulate blockchain block creation
    std::cout << "\nSimulating blockchain block...\n";

    auto tx = store.beginTransaction();

    Array<u8, 32> merkle_root = {}; // Simplified
    merkle_root[0] = 0x1;
    merkle_root[1] = 0x2;
    merkle_root[2] = 0x3;

    if (store.storeBlock(1, String("blockhash_001"), String("GENESIS"), String("merkleroot_001"), currentTimestamp(), 12345)
            .is_ok()) {
        std::cout << "  [OK] Block #1 stored\n";
    }

    if (store.storeTransaction(String(alice_tx_id.c_str()), 1, alice.created_at, 100, alice.toBytes()).is_ok()) {
        std::cout << "  [OK] Transaction for Alice stored\n";
    }

    if (store.storeTransaction(String(bob_tx_id.c_str()), 1, bob.created_at, 100, bob.toBytes()).is_ok()) {
        std::cout << "  [OK] Transaction for Bob stored\n";
    }

    // 6. Anchor users to blockchain
    std::cout << "\nAnchoring users to blockchain...\n";

    TxRef alice_ref;
    alice_ref.tx_id = String(alice_tx_id.c_str());
    alice_ref.block_height = 1;
    alice_ref.merkle_root = merkle_root;
    if (user_mgr.anchorUser(alice.user_id, alice_ref)) {
        std::cout << "  [OK] Alice anchored to block #1\n";
    }

    TxRef bob_ref;
    bob_ref.tx_id = String(bob_tx_id.c_str());
    bob_ref.block_height = 1;
    bob_ref.merkle_root = merkle_root;
    if (user_mgr.anchorUser(bob.user_id, bob_ref)) {
        std::cout << "  [OK] Bob anchored to block #1\n";
    }

    tx->commit();

    // 7. Verify user integrity
    std::cout << "\nVerifying user integrity...\n";

    if (user_mgr.verifyUser(alice.user_id)) {
        std::cout << "  [OK] Alice's data verified successfully\n";
    } else {
        std::cout << "  [FAIL] Alice's data verification failed\n";
    }

    if (user_mgr.verifyUser(bob.user_id)) {
        std::cout << "  [OK] Bob's data verified successfully\n";
    } else {
        std::cout << "  [FAIL] Bob's data verification failed\n";
    }

    // 8. Query statistics
    std::cout << "\nLedger statistics:\n";
    std::cout << "  Blocks: " << store.getBlockCount() << "\n";
    std::cout << "  Transactions: " << store.getTransactionCount() << "\n";
    std::cout << "  Anchors: " << store.getAnchorCount() << "\n";
    std::cout << "  Latest block height: " << store.getLatestBlockHeight() << "\n";

    // 9. Verify chain continuity
    auto cont = store.verifyChainContinuity();
    if (cont.is_ok() && cont.value()) {
        std::cout << "\n[OK] Blockchain continuity verified\n";
    }

    // 10. Storage health check
    auto check = store.quickCheck();
    if (check.is_ok() && check.value()) {
        std::cout << "[OK] Storage integrity check passed\n";
    }

    // 11. Test anchor retrieval
    std::cout << "\nRetrieving anchors by block...\n";
    auto anchors = store.getAnchorsByBlock(1);
    std::cout << "  Found " << anchors.size() << " anchors in block #1\n";

    for (const auto &anchor : anchors) {
        std::cout << "    - " << anchor.content_id.c_str() << " (tx: " << anchor.tx_id.c_str() << ")\n";
    }

    store.close();
    std::cout << "\n=== Demo completed successfully ===\n";

    // Cleanup
    std::filesystem::remove_all(storage_path);

    return 0;
}
