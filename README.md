# Blockit

Type-safe blockchain + storage library for C++20 with cryptographic anchoring and Proof of Authority consensus.

## Development Status

See [TODO.md](./TODO.md) for the complete development plan and current progress.

## Overview

Blockit is a modern C++20 library that combines a full-featured blockchain ledger with an efficient file-based storage system. It provides a unified API (`blockit::Blockit<T>`) that manages blockchain operations and persistent storage atomically, ensuring cryptographic integrity through SHA256-based anchoring of stored records to on-chain transactions.

The library is designed for applications that need immutable, cryptographically verifiable record keeping without the complexity of external databases. Its zero-dependency storage layer uses append-only binary files with in-memory indexes, while the blockchain supports both simple Proof of Work and advanced Proof of Authority consensus mechanisms. Built with thread-safety from the ground up using `std::shared_mutex`, Blockit is suitable for high-throughput, multi-threaded environments.

Key design principles include type-safety through templates, header-only implementations where possible, and a clean separation between the blockchain ledger and storage layers. The library leverages the Datapod utility library for common types and Keylock for cryptographic operations, providing a cohesive, modern C++ experience for blockchain and data integrity applications.

### Architecture Diagrams

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          blockit::Blockit<T>                            │
│                          (Unified High-Level API)                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌─────────────────────────────────────┐     ┌────────────────────────┐ │
│  │      ledger::Chain<T>               │     │   storage::FileStore   │ │
│  ├─────────────────────────────────────┤     ├────────────────────────┤ │
│  │  - Block<T> chain                   │     │  - blocks.dat         │ │
│  │  - Transaction<T> records           │     │  - transactions.dat   │ │
│  │  - Merkle Tree verification         │     │  - anchors.dat        │ │
│  │  - Authenticator (PoA)              │     │  - In-memory indexes  │ │
│  └─────────────────────────────────────┘     └────────────────────────┘ │
│            │                                          │                 │
│            └────────────┬─────────────────────────────┘                 │
│                         │                                               │
│                 ┌───────▼────────┐                                      │
│                 │  Anchoring     │  Cryptographic linking                │
│                 │  (SHA256)      │  between storage and blockchain      │
│                 └────────────────┘                                      │
└─────────────────────────────────────────────────────────────────────────┘
                                  │
                           ┌──────▼────────┐
                           │   Your App    │
                           │  (Domain T)   │
                           └───────────────┘
```

```
┌───────────────────────────────────────────────────────────────────────┐
│                         Blockchain Ledger Layer                        │
├───────────────────┬───────────────────┬───────────────┬───────────────┤
│     Chain<T>      │    Block<T>       │  Transaction  │    Merkle      │
│                   │                   │      <T>      │    Tree        │
│  - Block chain    │  - Transactions   │  - UUID       │  - Root hash   │
│  - Validation     │  - Hash links    │  - Priority   │  - Verification│
│  - Add/Get blocks │  - Merkle root   │  - Timestamp  │                │
└───────────────────┴───────────────────┴───────────────┴───────────────┘
       │                   │                   │               │
       └───────────────────┴───────────────────┴───────────────┘
                            │
                    ┌───────▼────────┐
                    │  PoAConsensus  │
                    │  - Validators  │
                    │  - Signatures  │
                    │  - Quorum mgmt │
                    └────────────────┘
```

## Installation

### Quick Start (CMake FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
  blockit
  GIT_REPOSITORY https://github.com/robolibs/blockit
  GIT_TAG main
)
FetchContent_MakeAvailable(blockit)

target_link_libraries(your_target PRIVATE blockit)
```

### Recommended: XMake

[XMake](https://xmake.io/) is a modern, fast, and cross-platform build system.

**Install XMake:**
```bash
curl -fsSL https://xmake.io/shget.text | bash
```

**Add to your xmake.lua:**
```lua
add_requires("blockit")

target("your_target")
    set_kind("binary")
    add_packages("blockit")
    add_files("src/*.cpp")
```

**Build:**
```bash
xmake
xmake run
```

### Using the Makefile

Blockit includes a powerful Makefile that automatically detects and works with CMake, XMake, or Zig build systems:

```bash
make config      # Configure with examples and tests enabled
make build       # Build the project (runs clang-format first)
make test        # Run all tests (or TEST=<name> for specific test)
make clean       # Clean build artifacts (requires internet)
make reconfig    # Full reconfigure with clean cache (requires internet)
```

**Compiler selection:**
```bash
make config CC=clang   # Use Clang
make config CC=gcc     # Use GCC
```

**Enable big transfer tests (100MB+):**
```bash
make config BIG_TRANSFER=1
```

### Complete Development Environment (Nix + Direnv + Devbox)

For the ultimate reproducible development environment:

**1. Install Nix (package manager from NixOS):**
```bash
# Determinate Nix Installer (recommended)
curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix | sh -s -- install
```
[Nix](https://nixos.org/) - Reproducible, declarative package management

**2. Install direnv (automatic environment switching):**
```bash
sudo apt install direnv

# Add to your shell (~/.bashrc or ~/.zshrc):
eval "$(direnv hook bash)"  # or zsh
```
[direnv](https://direnv.net/) - Load environment variables based on directory

**3. Install Devbox (Nix-powered development environments):**
```bash
curl -fsSL https://get.jetpack.io/devbox | bash
```
[Devbox](https://www.jetpack.io/devbox/) - Portable, isolated dev environments

**4. Use the environment:**
```bash
cd blockit
direnv allow  # Allow .envrc (one-time)
# Environment automatically loaded! All dependencies available.

make config
make build
make test
```

## Usage

### Basic Usage

```cpp
#include <blockit/blockit.hpp>

// 1. Define your data type
struct Document {
    std::string id, title, content;

    std::string to_string() const { return title; }

    datapod::Vector<uint8_t> toBytes() const {
        std::string data = id + "|" + title + "|" + content;
        return datapod::Vector<uint8_t>(data.begin(), data.end());
    }
};

int main() {
    // 2. Initialize unified store (blockchain + storage)
    blockit::Blockit<Document> store;
    auto crypto = std::make_shared<blockit::Crypto>("secret_key");
    Document genesis{"genesis", "Genesis Doc", "Initial content"};

    auto init_result = store.initialize(
        "myapp_store", "MyChain", "genesis_tx", genesis, crypto);

    if (!init_result.is_ok()) {
        std::cerr << "Failed to initialize\n";
        return 1;
    }

    // 3. Create transaction for anchoring
    Document doc{"doc_001", "My Document", "Document content"};
    auto tx_result = store.createTransaction(
        "tx_001", doc, "doc_001", doc.toBytes(), 100);

    if (!tx_result.is_ok()) {
        std::cerr << "Failed to create transaction\n";
        return 1;
    }

    // 4. Add block to chain (stores transactions, creates anchors)
    blockit::Transaction<Document> tx("tx_001", doc, 100);
    auto sign_result = tx.signTransaction(crypto);
    std::vector<blockit::Transaction<Document>> txs{tx};

    auto block_result = store.addBlock(txs);
    if (!block_result.is_ok()) {
        std::cerr << "Failed to add block\n";
        return 1;
    }

    // 5. Verify content against blockchain
    auto verify_result = store.verifyContent("doc_001", doc.toBytes());
    if (verify_result.is_ok() && verify_result.value()) {
        std::cout << "Content verified against blockchain!\n";
    }

    return 0;
}
```

### Low-Level Storage API

For direct storage operations without blockchain:

```cpp
#include <blockit/storage/file_store.hpp>

using namespace blockit::storage;

int main() {
    FileStore store;
    auto open_result = store.open("my_storage", OpenOptions{});

    if (!open_result.is_ok()) {
        return 1;
    }

    // Store a block
    auto result = store.storeBlock(
        0,                              // height
        "hash_001",                     // hash
        "prev_hash_000",                // previous hash
        "merkle_root_001",              // merkle root
        currentTimestamp(),             // timestamp
        12345                           // nonce
    );

    // Create an anchor (cryptographic link)
    TxRef tx_ref;
    tx_ref.tx_id = "tx_001";
    tx_ref.block_height = 0;
    // ... set merkle_root

    Vector<u8> content_hash = computeSHA256(content_bytes);
    store.createAnchor("content_001", content_hash, tx_ref);

    return 0;
}
```

### Examples Overview

The repository includes comprehensive examples demonstrating various use cases:

- **`complete_stack_demo.cpp`** - Full-featured document management system with versioning, updates, and audit trails. Shows the recommended production approach using `blockit::Blockit<T>`.

- **`unified_api_demo.cpp`** - Basic unified API usage demonstrating the core workflow: initialize, create transactions, add blocks, and verify content.

- **`poa_demo.cpp`** - Proof of Authority consensus example with validator management, proposal creation, signature collection, quorum checking, and rate limiting.

- **`farming_demo.cpp`** - Agricultural use case showing how to track farming operations, sensor readings, and maintenance records with blockchain verification.

- **`file_integration_demo.cpp`** - Direct storage layer operations for advanced use cases requiring fine-grained control over the storage system.

- **`enhanced_demo.cpp`** - Advanced features including querying, filtering, and bulk operations.

Run examples:
```bash
make config
make build
cd build
./complete_stack_demo
./unified_api_demo
./poa_demo
./farming_demo
```

### Advanced Usage: Proof of Authority Consensus

```cpp
#include <blockit/blockit.hpp>

using namespace blockit::ledger;

int main() {
    // 1. Setup PoA configuration
    PoAConfig config;
    config.initial_required_signatures = 2;
    config.signature_timeout_ms = 30000;      // 30 seconds
    config.offline_threshold_ms = 120000;     // 2 minutes

    PoAConsensus consensus(config);

    // 2. Generate and register validators
    auto alice_key = Key::generate();
    auto bob_key = Key::generate();
    auto charlie_key = Key::generate();

    consensus.addValidator("alice", alice_key.value());
    consensus.addValidator("bob", bob_key.value());
    consensus.addValidator("charlie", charlie_key.value());

    // 3. Create proposal for new block
    auto proposal_id = consensus.createProposal("block_hash_001", "alice");

    // 4. Collect signatures from validators
    std::vector<uint8_t> block_data{0x01, 0x02, 0x03};

    auto sig1 = alice_key.value().sign(block_data);
    consensus.addSignature(proposal_id, alice_key.value().getId(), sig1.value());

    auto sig2 = bob_key.value().sign(block_data);
    consensus.addSignature(proposal_id, bob_key.value().getId(), sig2.value());

    // 5. Check if quorum is reached
    if (consensus.isProposalReady(proposal_id)) {
        auto signatures = consensus.getFinalizedSignatures(proposal_id);
        std::cout << "Quorum reached! Signatures: "
                  << signatures.value().size() << "\n";
    }

    return 0;
}
```

### Advanced Usage: Document Management with Versioning

See `examples/complete_stack_demo.cpp` for a full production example demonstrating:
- Multi-version document storage
- Update tracking with blockchain verification
- Query by content ID and transaction history
- Complete audit trails with cryptographic proofs

## Features

- **Unified API (`blockit::Blockit<T>`)** - High-level interface that manages blockchain and storage atomically
  ```cpp
  blockit::Blockit<Document> store;
  store.initialize(path, chain_name, genesis_tx_id, genesis_data, crypto);
  ```
  Provides `createTransaction()`, `addBlock()`, `verifyContent()`, and query operations in one cohesive interface.

- **Cryptographic Anchoring** - Storage records linked to blockchain via SHA256 hashes
  ```cpp
  auto result = store.verifyContent("doc_id", current_bytes);
  ```
  Enables tamper-proof verification where any content change breaks the cryptographic link to the blockchain.

- **Zero Database Dependencies** - Uses efficient append-only binary file storage
  ```
  <storage_dir>/
    blocks.dat        # Append-only block records
    transactions.dat  # Append-only transaction records
    anchors.dat       # Append-only anchor records
  ```
  In-memory indexes are rebuilt on load for fast queries without external database complexity.

- **Type-Safe Templates** - Works with any custom type that implements `to_string()` and `toBytes()`
  ```cpp
  struct UserData { std::string name, email; /* ... */ };
  blockit::Blockit<UserData> user_store;  // Fully type-safe!
  ```
  Compile-time type checking prevents runtime errors in data handling.

- **Proof of Authority Consensus** - Validator-based consensus with quorum management
  ```cpp
  PoAConsensus consensus(config);
  consensus.addValidator("alice", key);
  auto sigs = consensus.getFinalizedSignatures(proposal_id);
  ```
  Ideal for permissioned blockchains where trusted validators sign blocks.

- **Thread-Safe Architecture** - Built with `std::shared_mutex` for concurrent read/write access
  ```cpp
  // Multiple threads can read simultaneously
  // Write operations are exclusive and atomic
  auto chain = store.getChain();  // Thread-safe read
  ```
  Suitable for high-throughput, multi-threaded applications.

- **Merkle Tree Verification** - Efficient proof generation and verification for transaction integrity
  ```cpp
  auto merkle_root = block.merkle_root_;
  // O(log n) proof verification for transaction inclusion
  ```

- **Identity and Signing** - Key-based identity management via Keylock library
  ```cpp
  auto key = Key::generate();
  auto signature = key->sign(data);
  auto verified = key->verify(data, signature);
  ```
  Secure cryptographic signing with Ed25519/XChaCha20-Poly1305 support.

- **Flexible Storage Options** - File-based storage with query filters
  ```cpp
  storage::LedgerQuery query;
  query.block_height_min = 100;
  query.block_height_max = 200;
  query.limit = 50;
  auto results = store.getStorage().queryBlocks(query);
  ```

- **Authentication and Authorization** - Participant management with capabilities
  ```cpp
  Authenticator auth;
  auth.registerParticipant("alice", "active", {{"role", "admin"}});
  auth.checkCapability("alice", "create_block");
  ```

- **Comprehensive Error Handling** - Result<T, Error> pattern for explicit error handling
  ```cpp
  auto result = store.addBlock(transactions);
  if (!result.is_ok()) {
    std::cerr << "Error: " << result.error().message << "\n";
  }
  ```

**Performance Characteristics:**
- **Storage**: Append-only binary format with O(1) appends, O(n) index rebuild on load
- **Verification**: SHA256 hashing with hardware acceleration support (SIMD)
- **Concurrency**: Read-optimized with shared mutex (multiple readers, single writer)
- **Memory**: In-memory indexes for fast queries; data remains on disk

**Integration Capabilities:**
- CMake FetchContent for easy dependency management
- XMake package manager support
- Header-only and compiled library hybrid architecture
- Works with Datapod and Keylock for seamless integration in ROBOLIBS ecosystem

## Testing

```bash
make config
make build
make test
```

## API Reference

### High-Level API: `blockit::Blockit<T>`

```cpp
// Initialize with storage and blockchain
auto init_result = store.initialize(
    "myapp_store", "MyChain", "genesis_tx", genesis_data, crypto);

// Create transaction for anchoring
auto tx_result = store.createTransaction(
    "tx_001", data, "content_id", content_bytes, 100);

// Add block (atomically stores in blockchain + storage)
auto block_result = store.addBlock(transactions);

// Verify content against blockchain
auto verify_result = store.verifyContent("content_id", current_bytes);

// Access underlying components
auto& chain = store.getChain();
auto& storage = store.getStorage();

// Statistics
i64 blocks = store.getBlockCount();
i64 txs = store.getTransactionCount();
i64 anchors = store.getAnchorCount();
```

### Blockchain: `ledger::Chain<T>`

```cpp
// Create chain with genesis
ledger::Chain<Document> chain(
    "MyChain", "genesis_tx", genesis_data, crypto);

// Add block
auto result = chain.addBlock(block);

// Query blocks
auto last = chain.getLastBlock();
auto by_height = chain.getBlock(5);
i64 length = chain.getChainLength();

// Validate chain
auto valid = chain.isValid();
```

### Proof of Authority: `ledger::PoAConsensus`

```cpp
// Setup configuration
PoAConfig config;
config.initial_required_signatures = 2;
config.signature_timeout_ms = 30000;

PoAConsensus consensus(config);

// Add validators
consensus.addValidator("alice", validator_key);

// Create proposal and collect signatures
String proposal_id = consensus.createProposal("block_hash", "alice");
consensus.addSignature(proposal_id, validator_id, signature);

// Check quorum
if (consensus.isProposalReady(proposal_id)) {
    auto sigs = consensus.getFinalizedSignatures(proposal_id);
}
```

### Error Handling

All functions return `Result<T, Error>` for explicit error handling:

```cpp
auto result = store.addBlock(transactions);
if (!result.is_ok()) {
    std::cerr << "Error (" << result.error().code << "): "
              << result.error().message << "\n";
}
```

Common error codes:
- `ERR_CHAIN_EMPTY (100)` - Chain is empty
- `ERR_INVALID_BLOCK (101)` - Block validation failed
- `ERR_DUPLICATE_TX (102)` - Transaction already exists
- `ERR_UNAUTHORIZED (103)` - Unauthorized participant
- `ERR_SIGNING_FAILED (106)` - Signing operation failed
- `ERR_VERIFICATION_FAILED (107)` - Verification failed

## License

MIT License - see [LICENSE](./LICENSE) for details.

## Acknowledgments

Made possible thanks to these amazing projects:
- **[Datapod](https://github.com/robolibs/datapod)** - Utility library providing Result<T, Error>, String, Vector, and common types
- **[Keylock](https://github.com/bresilla/keylock)** - Cryptographic operations (hashing, signing, encryption)
- **[Echo](https://github.com/bresilla/echo)** - Logging and utilities
- **[libsodium](https://github.com/jedisct1/libsodium)** - Modern, easy-to-use software encryption library
- **[Doctest](https://github.com/doctest/doctest)** - Lightweight C++ testing framework
