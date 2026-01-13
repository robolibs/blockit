# Storage Module

The Storage module provides persistent file-based storage for blockchain data with content anchoring capabilities. It enables robots to store blockchain state, anchor off-chain content (sensor data, images) to the blockchain, and sync state across restarts.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [FileStore](#filestore)
- [Blockit Store](#blockit-store)
- [Content Anchoring](#content-anchoring)
- [Record Types](#record-types)
- [DID Integration](#did-integration)
- [Persistence and Recovery](#persistence-and-recovery)
- [Thread Safety](#thread-safety)
- [Robotics Use Cases](#robotics-use-cases)

## Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            STORAGE MODULE                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         Blockit<T>                                    │   │
│  │              (Unified Blockchain + Storage API)                       │   │
│  └───────────────────────────────┬──────────────────────────────────────┘   │
│                                  │                                           │
│            ┌─────────────────────┼─────────────────────┐                    │
│            │                     │                     │                    │
│            ▼                     ▼                     ▼                    │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐     │
│  │   Chain<T>      │  │   FileStore     │  │   DID Integration       │     │
│  │  (Blockchain)   │  │  (Persistence)  │  │  (Identity Support)     │     │
│  └────────┬────────┘  └────────┬────────┘  └───────────┬─────────────┘     │
│           │                    │                       │                    │
│           │           ┌────────┴────────┐              │                    │
│           │           │                 │              │                    │
│           │           ▼                 ▼              │                    │
│           │  ┌─────────────┐  ┌─────────────────┐      │                    │
│           │  │   Block     │  │    Anchor       │      │                    │
│           │  │  Records    │  │   Records       │      │                    │
│           │  └─────────────┘  └─────────────────┘      │                    │
│           │                                            │                    │
│           └────────────────────────────────────────────┘                    │
│                                  │                                           │
│                                  ▼                                           │
│                        ┌─────────────────┐                                   │
│                        │   File System   │                                   │
│                        │  ./storage_dir/ │                                   │
│                        └─────────────────┘                                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Architecture

### Storage Directory Structure

```
storage_dir/
├── chain.dat           # Serialized blockchain
├── blocks/             # Individual block records
│   ├── block_0.dat
│   ├── block_1.dat
│   └── ...
├── transactions/       # Transaction index
│   └── tx_index.dat
├── anchors/            # Content anchors
│   └── anchor_index.dat
└── metadata.dat        # Storage metadata
```

## FileStore

### Header: `<blockit/storage/file_store.hpp>`

Low-level file-based storage for blockchain components.

```cpp
namespace blockit {
namespace storage {

// Get current timestamp in milliseconds
inline dp::i64 currentTimestamp();

// Record types
struct BlockRecord {
    dp::i64 height;
    dp::String block_hash;
    dp::i64 stored_at;
    dp::Vector<dp::u8> block_data;

    auto members();
};

struct TransactionRecord {
    dp::String tx_id;
    dp::i64 block_height;
    dp::i64 tx_index;
    dp::i64 stored_at;
    dp::Vector<dp::u8> tx_data;

    auto members();
};

struct Anchor {
    dp::String content_id;      // Your content identifier
    dp::String content_hash;    // SHA256 of content
    dp::String tx_id;           // Transaction that anchored it
    dp::i64 block_height;       // Block containing the transaction
    dp::i64 anchored_at;        // Timestamp

    auto members();
};

class FileStore {
public:
    FileStore() = default;

    // === Initialization ===
    dp::Result<void, dp::Error> open(const dp::String& storage_path);
    dp::Result<void, dp::Error> close();
    bool isOpen() const;

    // === Block Storage ===
    dp::Result<void, dp::Error> storeBlock(const BlockRecord& record);
    dp::Result<BlockRecord, dp::Error> loadBlock(dp::i64 height) const;
    dp::Result<BlockRecord, dp::Error> loadBlockByHash(const dp::String& hash) const;
    std::vector<BlockRecord> loadAllBlocks() const;
    dp::i64 getBlockCount() const;

    // === Transaction Storage ===
    dp::Result<void, dp::Error> storeTransaction(const TransactionRecord& record);
    dp::Result<TransactionRecord, dp::Error> loadTransaction(const dp::String& tx_id) const;
    std::vector<TransactionRecord> loadTransactionsByBlock(dp::i64 block_height) const;
    dp::i64 getTransactionCount() const;

    // === Content Anchoring ===
    dp::Result<void, dp::Error> storeAnchor(const Anchor& anchor);
    dp::Result<Anchor, dp::Error> loadAnchor(const dp::String& content_id) const;
    std::vector<Anchor> loadAnchorsByTransaction(const dp::String& tx_id) const;
    std::vector<Anchor> loadAnchorsByBlock(dp::i64 block_height) const;
    bool hasAnchor(const dp::String& content_id) const;
    dp::i64 getAnchorCount() const;

    // === Queries ===
    std::vector<TransactionRecord> queryTransactions(
        dp::i64 from_block,
        dp::i64 to_block
    ) const;

    // === Persistence ===
    dp::Result<void, dp::Error> flush();  // Force write to disk

    // === Storage Path ===
    std::string getStoragePath() const;
};

} // namespace storage
} // namespace blockit
```

### Example Usage

```cpp
#include <blockit/storage/file_store.hpp>
using namespace blockit::storage;

FileStore store;
store.open(dp::String("./robot_data")).value();

// Store a block record
BlockRecord record;
record.height = 1;
record.block_hash = dp::String("abc123...");
record.stored_at = currentTimestamp();
record.block_data = serialized_block;
store.storeBlock(record).value();

// Load block
auto loaded = store.loadBlock(1).value();
std::cout << "Block hash: " << loaded.block_hash.c_str() << "\n";

// Store anchor
Anchor anchor;
anchor.content_id = dp::String("sensor_frame_001");
anchor.content_hash = dp::String("sha256_of_content");
anchor.tx_id = dp::String("tx_001");
anchor.block_height = 1;
anchor.anchored_at = currentTimestamp();
store.storeAnchor(anchor).value();

// Verify anchor exists
if (store.hasAnchor(dp::String("sensor_frame_001"))) {
    auto a = store.loadAnchor(dp::String("sensor_frame_001")).value();
    std::cout << "Anchored in block: " << a.block_height << "\n";
}

store.close();
```

## Blockit Store

### Header: `<blockit/storage/blockit_store.hpp>`

Unified API combining blockchain operations with persistent storage.

```cpp
namespace blockit {

template <typename T>
class Blockit {
public:
    Blockit() = default;

    // === Initialization ===
    dp::Result<void, dp::Error> initialize(
        const dp::String& storage_path,
        const dp::String& chain_name,
        const dp::String& genesis_tx_id,
        const T& genesis_data,
        std::shared_ptr<Crypto> crypto
    );

    bool isInitialized() const;

    // === Transaction Management ===

    // Create transaction (optionally with content anchor)
    dp::Result<void, dp::Error> createTransaction(
        const dp::String& tx_id,
        const T& data,
        dp::i16 priority = 100
    );

    // Create transaction with content to anchor
    dp::Result<void, dp::Error> createTransaction(
        const dp::String& tx_id,
        const T& data,
        const dp::String& anchor_id,
        const std::vector<dp::u8>& content_to_anchor,
        dp::i16 priority = 100
    );

    // Get pending transactions
    std::vector<ledger::Transaction<T>> getPendingTransactions() const;
    size_t getPendingTransactionCount() const;
    void clearPendingTransactions();

    // === Block Management ===

    // Add block from transactions
    dp::Result<void, dp::Error> addBlock(
        const std::vector<ledger::Transaction<T>>& transactions
    );

    // Get block info
    dp::i64 getBlockCount() const;
    dp::Result<ledger::Block<T>, dp::Error> getBlock(dp::i64 height) const;
    dp::Result<ledger::Block<T>, dp::Error> getLatestBlock() const;

    // === Chain Access ===
    ledger::Chain<T>& getChain();
    const ledger::Chain<T>& getChain() const;

    // === Content Anchoring ===

    // Get anchor by content ID
    std::optional<storage::Anchor> getAnchor(const dp::String& content_id) const;

    // Get all anchors for a transaction
    std::vector<storage::Anchor> getAnchorsForTransaction(const dp::String& tx_id) const;

    // Verify content against blockchain
    dp::Result<bool, dp::Error> verifyContent(
        const dp::String& anchor_id,
        const std::vector<dp::u8>& content
    ) const;

    // Get anchor counts
    dp::i64 getAnchorCount() const;

    // === Transaction Queries ===
    dp::i64 getTransactionCount() const;
    dp::Result<storage::TransactionRecord, dp::Error> getTransaction(
        const dp::String& tx_id
    ) const;

    // === Consistency ===
    dp::Result<bool, dp::Error> verifyConsistency() const;

    // === DID Integration ===

    // Initialize DID support
    void initializeDID();
    bool hasDIDSupport() const;

    // DID operations
    dp::Result<std::pair<DIDDocument, DIDOperation>, dp::Error> createDID(const Key& key);
    dp::Result<DIDDocument, dp::Error> resolveDID(const DID& did) const;
    dp::Result<DIDOperation, dp::Error> updateDID(
        const DID& did,
        const DIDDocument& doc,
        const Key& key
    );
    dp::Result<DIDOperation, dp::Error> deactivateDID(const DID& did, const Key& key);

    // Robot identity (creates DID + authorization credential)
    dp::Result<std::pair<DIDDocument, VerifiableCredential>, dp::Error> createRobotIdentity(
        const Key& robot_key,
        const std::string& robot_id,
        const std::vector<std::string>& capabilities,
        const Key& issuer_key
    );

    // Credential operations
    dp::Result<VerifiableCredential, dp::Error> issueCredential(
        const Key& issuer_key,
        const DID& subject_did,
        CredentialType type,
        const std::map<std::string, std::string>& claims,
        std::chrono::duration<int64_t> validity = std::chrono::hours(24 * 365)
    );

    // Access DID registry and credential status
    std::shared_ptr<DIDRegistry> getDIDRegistry();
    std::shared_ptr<CredentialStatusList> getCredentialStatusList();

    // === Storage Access ===
    storage::FileStore& getFileStore();
    const storage::FileStore& getFileStore() const;
};

} // namespace blockit
```

### Example Usage

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

// Define your data type
struct SensorReading {
    std::string sensor_id;
    double value;
    uint64_t timestamp;

    std::string to_string() const {
        return sensor_id + ":" + std::to_string(value);
    }

    auto members() { return std::tie(sensor_id, value, timestamp); }
};

// Initialize
Blockit<SensorReading> store;
auto crypto = std::make_shared<Crypto>("sensor_key");

SensorReading genesis{"SYSTEM", 0.0, 0};
store.initialize(
    dp::String("./sensor_data"),
    dp::String("SensorChain"),
    dp::String("genesis"),
    genesis,
    crypto
).value();

// Create transactions
SensorReading reading{"TEMP_001", 23.5, timestamp};
store.createTransaction(dp::String("tx_001"), reading, 100);

SensorReading reading2{"TEMP_002", 24.1, timestamp};
store.createTransaction(dp::String("tx_002"), reading2, 100);

// Add to blockchain
auto pending = store.getPendingTransactions();
store.addBlock(pending).value();

// Query
std::cout << "Blocks: " << store.getBlockCount() << "\n";
std::cout << "Transactions: " << store.getTransactionCount() << "\n";

// Verify consistency
auto consistent = store.verifyConsistency();
if (consistent.is_ok() && consistent.value()) {
    std::cout << "Storage is consistent with blockchain\n";
}
```

## Content Anchoring

Content anchoring links off-chain data (images, sensor readings, large files) to blockchain transactions. This provides:

1. **Tamper-proof timestamps** - Prove data existed at a specific time
2. **Data integrity** - Verify data hasn't been modified
3. **Efficient storage** - Large data stays off-chain, only hash goes on-chain

### How Anchoring Works

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Sensor Data    │────▶│  Compute Hash   │────▶│  Create Anchor  │
│  (e.g., image)  │     │  (SHA256)       │     │  Record         │
└─────────────────┘     └─────────────────┘     └────────┬────────┘
                                                         │
                                                         ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Transaction    │◀────│  Include Hash   │◀────│  Store Anchor   │
│  (on-chain)     │     │  in Transaction │     │  (FileStore)    │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

### Anchoring API

```cpp
// Create transaction with anchored content
std::vector<uint8_t> camera_frame = capture_camera();

SensorRecord record{"ROBOT_001", "camera", compute_hash(camera_frame), x, y, z, ts};

store.createTransaction(
    dp::String("tx_frame_001"),   // Transaction ID
    record,                        // On-chain data
    dp::String("frame_001"),       // Anchor ID (your identifier)
    camera_frame,                  // Content to anchor (stored off-chain)
    100                            // Priority
);

// Later: verify content integrity
auto result = store.verifyContent(dp::String("frame_001"), camera_frame);
if (result.is_ok() && result.value()) {
    std::cout << "Content verified - matches blockchain record\n";
}

// Get anchor details
auto anchor = store.getAnchor(dp::String("frame_001"));
if (anchor.has_value()) {
    std::cout << "Anchored in transaction: " << anchor->tx_id.c_str() << "\n";
    std::cout << "Block height: " << anchor->block_height << "\n";
    std::cout << "Anchored at: " << anchor->anchored_at << "\n";
}
```

### Verification Flow

```cpp
// Receive data and verify against blockchain
std::vector<uint8_t> received_data = receive_from_network();
std::string anchor_id = "sensor_reading_42";

// Check if anchor exists
auto anchor = store.getAnchor(dp::String(anchor_id.c_str()));
if (!anchor.has_value()) {
    std::cerr << "Unknown content - no anchor found\n";
    return false;
}

// Verify content hash
auto verify_result = store.verifyContent(
    dp::String(anchor_id.c_str()),
    received_data
);

if (verify_result.is_ok() && verify_result.value()) {
    std::cout << "Content authentic - hash matches blockchain\n";
    std::cout << "  Anchored at block: " << anchor->block_height << "\n";
    std::cout << "  Timestamp: " << anchor->anchored_at << "\n";
    return true;
} else {
    std::cerr << "Content tampered - hash mismatch!\n";
    return false;
}
```

## Record Types

### BlockRecord

```cpp
struct BlockRecord {
    dp::i64 height;              // Block number (0, 1, 2, ...)
    dp::String block_hash;       // SHA256 hash of block
    dp::i64 stored_at;           // Storage timestamp (ms)
    dp::Vector<dp::u8> block_data;  // Serialized block

    auto members() { return std::tie(height, block_hash, stored_at, block_data); }
};
```

### TransactionRecord

```cpp
struct TransactionRecord {
    dp::String tx_id;            // Unique transaction ID
    dp::i64 block_height;        // Block containing this transaction
    dp::i64 tx_index;            // Index within block
    dp::i64 stored_at;           // Storage timestamp (ms)
    dp::Vector<dp::u8> tx_data;  // Serialized transaction

    auto members() { return std::tie(tx_id, block_height, tx_index, stored_at, tx_data); }
};
```

### Anchor

```cpp
struct Anchor {
    dp::String content_id;       // Your identifier for the content
    dp::String content_hash;     // SHA256 hash of content
    dp::String tx_id;            // Transaction that anchored it
    dp::i64 block_height;        // Block containing the transaction
    dp::i64 anchored_at;         // Timestamp when anchored

    auto members() {
        return std::tie(content_id, content_hash, tx_id, block_height, anchored_at);
    }
};
```

## DID Integration

The Blockit store integrates DID support for robot identity management.

### Initialization

```cpp
Blockit<TaskData> blockit;
blockit.initialize(storage_path, chain_name, genesis_id, genesis_data, crypto);

// Enable DID support
blockit.initializeDID();

if (blockit.hasDIDSupport()) {
    std::cout << "DID support enabled\n";
}
```

### Robot Identity Workflow

```cpp
// 1. Create fleet manager identity
auto fleet_key = Key::generate().value();
auto [fleet_doc, _] = blockit.createDID(fleet_key).value();
auto fleet_did = fleet_doc.getId();

// 2. Onboard robot with DID and authorization
auto robot_key = Key::generate().value();
auto [robot_doc, auth_cred] = blockit.createRobotIdentity(
    robot_key,
    "DELIVERY_BOT_001",
    {"navigation", "pickup", "delivery"},
    fleet_key
).value();

// 3. Issue additional credentials
auto zone_cred = blockit.issueCredential(
    fleet_key,
    robot_doc.getId(),
    CredentialType::ZoneAccess,
    {{"zone", "warehouse_a"}, {"level", "full"}},
    std::chrono::hours(8)
).value();

// 4. Resolve DID
auto resolved = blockit.resolveDID(robot_doc.getId()).value();
std::cout << "Robot active: " << resolved.isActive() << "\n";

// 5. Access registries
auto registry = blockit.getDIDRegistry();
auto status_list = blockit.getCredentialStatusList();

std::cout << "DIDs: " << registry->size() << "\n";
std::cout << "Credentials: " << status_list->size() << "\n";
```

## Persistence and Recovery

### Automatic Persistence

The Blockit store automatically persists:
- Blockchain state (blocks and transactions)
- Content anchors
- Storage metadata

```cpp
// Data is persisted automatically on:
// - addBlock() - stores block and transaction records
// - createTransaction() with anchor - stores anchor record

// Manual flush (rarely needed)
store.getFileStore().flush();
```

### Recovery on Restart

```cpp
// Re-initialize with same storage path
Blockit<TaskData> store;
store.initialize(
    dp::String("./existing_data"),  // Same path
    dp::String("TaskChain"),
    dp::String("genesis"),
    genesis_data,
    crypto
);

// Blockchain state is automatically restored
std::cout << "Recovered blocks: " << store.getBlockCount() << "\n";
std::cout << "Recovered transactions: " << store.getTransactionCount() << "\n";
std::cout << "Recovered anchors: " << store.getAnchorCount() << "\n";

// Verify integrity after recovery
auto consistent = store.verifyConsistency();
if (consistent.is_ok() && consistent.value()) {
    std::cout << "Storage integrity verified\n";
}
```

### Chain Serialization

For full chain backup/restore:

```cpp
// Save entire chain to file
auto chain = store.getChain();
chain.saveToFile("/backup/chain.dat").value();

// Load chain from file
Chain<TaskData> restored_chain;
restored_chain.loadFromFile("/backup/chain.dat").value();
```

## Thread Safety

The storage module is thread-safe for concurrent access:

- **FileStore**: Uses internal locking for all operations
- **Blockit**: Thread-safe for read operations, synchronized writes
- **Anchors**: Safe concurrent reads, serialized writes

```cpp
// Safe: concurrent reads from multiple threads
std::thread reader1([&store]() {
    auto block = store.getBlock(1);
});

std::thread reader2([&store]() {
    auto tx = store.getTransaction(dp::String("tx_001"));
});

// Writes are automatically serialized
std::thread writer([&store]() {
    store.createTransaction(dp::String("new_tx"), data);
});
```

## Robotics Use Cases

### 1. Sensor Data Logging with Tamper-Proof Timestamps

```cpp
struct SensorLog {
    std::string robot_id;
    std::string sensor_type;
    std::string data_hash;
    double x, y, z;
    uint64_t timestamp;

    std::string to_string() const { return robot_id + ":" + sensor_type; }
    auto members() { return std::tie(robot_id, sensor_type, data_hash, x, y, z, timestamp); }
};

Blockit<SensorLog> sensor_store;
sensor_store.initialize("./sensors", "SensorChain", "genesis", genesis, crypto);

// Log camera frame with anchor
std::vector<uint8_t> frame = capture_camera();
SensorLog log{
    "ROBOT_001",
    "camera",
    compute_sha256(frame),
    current_x, current_y, current_z,
    timestamp
};

sensor_store.createTransaction(
    dp::String("frame_" + std::to_string(frame_count)),
    log,
    dp::String("img_" + std::to_string(frame_count)),
    frame
);

// Periodically commit to blockchain
if (sensor_store.getPendingTransactionCount() >= 10) {
    sensor_store.addBlock(sensor_store.getPendingTransactions());
}
```

### 2. Task Audit Trail

```cpp
struct TaskEvent {
    std::string task_id;
    std::string event_type;  // "created", "assigned", "started", "completed", "failed"
    std::string robot_id;
    std::string details;
    uint64_t timestamp;

    std::string to_string() const { return task_id + ":" + event_type; }
    auto members() { return std::tie(task_id, event_type, robot_id, details, timestamp); }
};

Blockit<TaskEvent> task_store;

// Log task lifecycle
task_store.createTransaction(dp::String("task_001_created"),
    TaskEvent{"TASK_001", "created", "LEADER", "Patrol sector 7", ts});

task_store.createTransaction(dp::String("task_001_assigned"),
    TaskEvent{"TASK_001", "assigned", "SCOUT_003", "", ts});

task_store.createTransaction(dp::String("task_001_completed"),
    TaskEvent{"TASK_001", "completed", "SCOUT_003", "Area clear", ts});

// Commit all events
task_store.addBlock(task_store.getPendingTransactions());

// Query task history
auto& file_store = task_store.getFileStore();
auto records = file_store.queryTransactions(0, task_store.getBlockCount());
// Filter by task_id in application logic
```

### 3. Fleet State Synchronization

```cpp
struct RobotState {
    std::string robot_id;
    double x, y, z;
    double battery;
    std::string status;
    uint64_t timestamp;

    std::string to_string() const { return robot_id + ":" + status; }
    auto members() { return std::tie(robot_id, x, y, z, battery, status, timestamp); }
};

// Each robot maintains its own store
Blockit<RobotState> my_store;
my_store.initialize("./robot_state", "StateChain", "genesis", genesis, crypto);

// Periodically broadcast state
RobotState my_state{
    "ROBOT_042",
    current_x, current_y, current_z,
    battery_level,
    "patrolling",
    timestamp
};

my_store.createTransaction(
    dp::String("state_" + std::to_string(seq_num)),
    my_state
);

my_store.addBlock(my_store.getPendingTransactions());

// Sync with other robots (network layer handles this)
auto latest = my_store.getLatestBlock().value();
broadcast_block(latest.serialize());
```

### 4. Evidence Collection

```cpp
struct Evidence {
    std::string case_id;
    std::string robot_id;
    std::string evidence_type;
    std::string hash;
    double x, y, z;
    uint64_t timestamp;

    std::string to_string() const { return case_id + ":" + evidence_type; }
    auto members() { return std::tie(case_id, robot_id, evidence_type, hash, x, y, z, timestamp); }
};

Blockit<Evidence> evidence_store;

// Capture and anchor evidence
std::vector<uint8_t> photo = capture_evidence_photo();
std::vector<uint8_t> lidar = capture_lidar_scan();

Evidence photo_ev{
    "CASE_001",
    "SECURITY_BOT_01",
    "photo",
    compute_sha256(photo),
    x, y, z,
    timestamp
};

evidence_store.createTransaction(
    dp::String("photo_001"),
    photo_ev,
    dp::String("evidence_photo_001"),
    photo  // Original photo anchored
);

Evidence lidar_ev{
    "CASE_001",
    "SECURITY_BOT_01",
    "lidar",
    compute_sha256(lidar),
    x, y, z,
    timestamp
};

evidence_store.createTransaction(
    dp::String("lidar_001"),
    lidar_ev,
    dp::String("evidence_lidar_001"),
    lidar  // Original scan anchored
);

// Commit evidence block
evidence_store.addBlock(evidence_store.getPendingTransactions());

// Later: verify evidence authenticity in court
auto verify = evidence_store.verifyContent(
    dp::String("evidence_photo_001"),
    photo
);
// verify.value() == true proves photo is authentic
```

### 5. Cross-Robot Data Validation

```cpp
// Robot A captures data
std::vector<uint8_t> sensor_data = capture_data();
std::string anchor_id = "sensor_" + std::to_string(seq);

store_a.createTransaction(tx_id, record, dp::String(anchor_id.c_str()), sensor_data);
store_a.addBlock(store_a.getPendingTransactions());

// Robot A sends data to Robot B
send_to_robot_b(sensor_data, anchor_id);

// Robot B receives and verifies
std::vector<uint8_t> received_data = receive_from_robot_a();
std::string received_anchor_id = get_anchor_id();

// Robot B syncs blockchain from Robot A (network layer)
sync_blockchain_from(robot_a);

// Robot B verifies data
auto verify = store_b.verifyContent(
    dp::String(received_anchor_id.c_str()),
    received_data
);

if (verify.is_ok() && verify.value()) {
    std::cout << "Data from Robot A verified\n";
    // Safe to use data
} else {
    std::cerr << "Data verification failed - possible tampering!\n";
    // Reject data
}
```
