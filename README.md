
<img align="right" width="26%" src="./misc/logo.png">

Blockit
===

Type-safe blockchain + storage library for C++20 with cryptographic anchoring.

## Features

- **Unified API**: `blockit::Blockit<T>` manages blockchain + file-based storage atomically
- **Cryptographic Anchoring**: Storage records linked to blockchain with SHA256 hashes
- **Zero Dependencies**: No external database required - uses binary file storage
- **Type-Safe Templates**: Works with any type that has `to_string()`
- **Header-Only**: No separate compilation required
- **Append-Only Storage**: Efficient binary logs with in-memory indexes

## Quick Start

```cpp
#include <blockit/blockit.hpp>

// 1. Define your data type
struct Document {
    std::string id, title, content;
    std::string to_string() const { return title; }
    std::vector<uint8_t> toBytes() const { /* serialize */ }
};

// 2. Initialize (manages blockchain + storage)
blockit::Blockit<Document> store;
auto crypto = std::make_shared<blockit::ledger::Crypto>("key");
Document genesis{"genesis", "Genesis Doc", "Initial"};

store.initialize("myapp_store", "MyChain", "genesis_tx", genesis, crypto);

// 3. Create transactions for anchoring
store.createTransaction("tx_001", doc, "doc_001", doc.toBytes(), 100);

// 4. Commit - ONE CALL does everything atomically:
//    - Adds block to blockchain
//    - Stores transactions in storage
//    - Creates cryptographic anchors
std::vector<blockit::ledger::Transaction<Document>> txs;
// ... populate txs ...
store.addBlock(txs);

// 5. Verify integrity (cryptographic proof!)
store.verifyContent("doc_001", current_bytes);  // Verified against blockchain!
```

## Examples

```bash
cd build
./complete_stack_demo      # Full-featured document system (RECOMMENDED)
./unified_api_demo         # Basic unified API usage
./file_integration_demo    # Direct storage layer
```

### Complete Stack Demo Output

```
[OK] Storage opened
[OK] Blockchain initialized
[OK] Documents created
[OK] Block added + anchors created (ATOMIC)
[OK] All documents verified against blockchain
[OK] Blockchain and storage synchronized
```

## Building

```bash
mkdir build && cd build
cmake ..
make
./complete_stack_demo
```

## API Reference

### blockit::Blockit<T> - Unified Interface

```cpp
// Initialize
bool initialize(path, chain_name, genesis_tx_id, genesis_data, crypto);

// Operations
bool createTransaction(tx_id, data, content_id, content_bytes, priority);
bool addBlock(transactions);  // Atomic: blockchain + storage + anchors

// Access
ledger::Chain<T>& getChain();
storage::FileStore& getStorage();

// Verification
bool verifyContent(content_id, content_bytes);
bool verifyConsistency();
```

### storage::FileStore - Low-Level Storage

```cpp
// Block operations
bool storeBlock(height, hash, prev_hash, merkle_root, timestamp, nonce);
std::optional<std::string> getBlockByHeight(height);
std::optional<std::string> getBlockByHash(hash);

// Transaction operations
bool storeTransaction(tx_id, block_height, timestamp, priority, payload);
std::optional<std::vector<uint8_t>> getTransaction(tx_id);

// Anchor operations
bool createAnchor(content_id, content_hash, tx_ref);
std::optional<Anchor> getAnchor(content_id);
bool verifyAnchor(content_id, content_bytes);
```

## Storage Format

The file-based storage uses append-only binary logs:

```
<storage_dir>/
  blocks.dat        # Append-only block records
  transactions.dat  # Append-only transaction records
  anchors.dat       # Append-only anchor records
```

Each record is length-prefixed binary data. Indexes are rebuilt in-memory on load.

## How It Works

```
Your Data -> Hash -> Blockchain Transaction
    |                      |
  Storage <-- Anchor ------+
    |
Fast Lookups + Cryptographic Verification
```

## Testing

```bash
./test_file_store  # Storage layer tests
```

## Dependencies

- Keylock (cryptography)
- C++20
- CMake 3.15+

## Use Cases

- Document management with blockchain verification
- Audit trails with cryptographic proofs
- Content-addressed storage systems
- Immutable record keeping

---

**See `complete_stack_demo.cpp` for full production example.**
