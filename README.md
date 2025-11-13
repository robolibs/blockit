
<img align="right" width="26%" src="./misc/logo.png">

Blockit
===

Type-safe blockchain + database library for C++20 with cryptographic anchoring.

## Features

- **Unified API**: `blockit::Blockit<T>` manages blockchain + SQLite database atomically
- **Cryptographic Anchoring**: Database records linked to blockchain with SHA256 hashes
- **Fast Queries**: SQLite indexes, full-text search, complex filters
- **Type-Safe Templates**: Works with any type that has `to_string()`
- **Extensible Schema**: Define your own tables, indexes, triggers
- **Header-Only**: No separate compilation required
- **Pure C SQLite**: No C++ wrapper dependencies

## Quick Start

```cpp
#include <blockit.hpp>

// 1. Define your data type
struct Document {
    std::string id, title, content;
    std::string to_string() const { return title; }
    std::vector<uint8_t> toBytes() const { /* serialize */ }
};

// 2. Define your database schema
class DocSchema : public blockit::storage::ISchemaExtension {
    std::vector<std::string> getCreateTableStatements() const override {
        return {"CREATE TABLE documents (id TEXT PRIMARY KEY, title TEXT, content TEXT)"};
    }
    std::vector<std::string> getCreateIndexStatements() const override {
        return {"CREATE INDEX idx_title ON documents(title)"};
    }
};

// 3. Initialize (manages blockchain + database)
blockit::Blockit<Document> store;
auto crypto = std::make_shared<blockit::ledger::Crypto>("key");
Document genesis{"genesis", "Genesis Doc", "Initial"};

store.initialize("myapp.db", "MyChain", "genesis_tx", genesis, crypto);
store.registerSchema(DocSchema{});

// 4. Insert data + register for anchoring
store.getStorage().executeSql("INSERT INTO documents VALUES (...)");
store.createTransaction("tx_001", doc, "doc_001", doc.toBytes(), 100);

// 5. Commit - ONE CALL does everything atomically:
//    ✓ Adds block to blockchain
//    ✓ Stores transactions in database
//    ✓ Creates cryptographic anchors
std::vector<blockit::ledger::Transaction<Document>> txs;
// ... populate txs ...
store.addBlock(txs);

// 6. Query database (fast!)
store.getStorage().executeQuery("SELECT * FROM documents WHERE title = ?", ...);

// 7. Verify integrity (cryptographic proof!)
store.verifyContent("doc_001", current_bytes);  // ✓ Verified against blockchain!
```

## Examples

```bash
cd build
./complete_stack_demo     # Full-featured document system (RECOMMENDED)
./unified_api_demo        # Basic unified API usage
./sqlite_integration_demo # Direct storage layer
```

### Complete Stack Demo Output

```
✓ Database opened
✓ Blockchain initialized
✓ Documents created
✓ Block added + anchors created (ATOMIC)
✓ All documents verified against blockchain
✓ Blockchain and database synchronized
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
bool registerSchema(const ISchemaExtension&);

// Operations
bool createTransaction(tx_id, data, content_id, content_bytes, priority);
bool addBlock(transactions);  // Atomic: blockchain + DB + anchors

// Access
ledger::Chain<T>& getChain();
storage::SqliteStore& getStorage();

// Verification
bool verifyContent(content_id, content_bytes);
bool verifyConsistency();
```

### storage::ISchemaExtension - Define Your Schema

```cpp
class MySchema : public ISchemaExtension {
    std::vector<std::string> getCreateTableStatements() const override;
    std::vector<std::string> getCreateIndexStatements() const override;
    std::vector<std::pair<int32_t, std::vector<std::string>>> getMigrations() const override;
};
```

## Core Schema (Managed by Library)

```sql
-- Blockchain ledger
CREATE TABLE blocks (height, hash, previous_hash, merkle_root, timestamp, nonce);
CREATE TABLE transactions (tx_id, block_height, timestamp, priority, payload);

-- Cryptographic anchors (links your data to blockchain)
CREATE TABLE anchors (content_id, content_hash, tx_id, block_height, merkle_root, anchored_at);
```

## How It Works

```
Your Data → Hash → Blockchain Transaction
    ↓                      ↓
  Database ←── Anchor ─────┘
    ↓
Fast Queries + Cryptographic Verification
```

## Testing

```bash
./test_sqlite_store       # 9 test cases, 139 assertions
```

## Dependencies

- SQLite3 (system, pure C API)
- Lockey (cryptography)
- C++20
- CMake 3.15+

## Use Cases


---

**See `complete_stack_demo.cpp` for full production example.**
