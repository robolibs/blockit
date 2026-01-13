# Ledger Module

The Ledger module provides the core blockchain implementation including chains, blocks, transactions, Merkle trees, Proof-of-Authority consensus, and access control. All types are generic templates allowing any data structure to be stored on-chain.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Key Class](#key-class)
- [Chain](#chain)
- [Block](#block)
- [Transaction](#transaction)
- [Merkle Tree](#merkle-tree)
- [Proof-of-Authority Consensus](#proof-of-authority-consensus)
- [Validator](#validator)
- [Authenticator](#authenticator)
- [Serialization](#serialization)
- [Robotics Use Cases](#robotics-use-cases)

## Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            LEDGER MODULE                                     │
│                        (blockit::ledger namespace)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         Chain<T>                                      │   │
│  │              (Ordered sequence of blocks)                             │   │
│  └───────────────────────────────┬──────────────────────────────────────┘   │
│                                  │                                           │
│                                  ▼                                           │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         Block<T>                                      │   │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────────┐  │   │
│  │  │   Index    │  │ Prev Hash  │  │   Hash     │  │ Merkle Root    │  │   │
│  │  └────────────┘  └────────────┘  └────────────┘  └────────────────┘  │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │              Transactions<T> (signed)                          │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │              PoA Signatures (validators)                       │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│  ┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐     │
│  │   PoA Consensus    │  │     Validator      │  │   Authenticator    │     │
│  │   (quorum voting)  │  │   (Ed25519 Key)    │  │  (access control)  │     │
│  └────────────────────┘  └────────────────────┘  └────────────────────┘     │
│                                                                              │
│  ┌────────────────────┐  ┌────────────────────┐                              │
│  │    Merkle Tree     │  │    Key (Ed25519)   │                              │
│  │  (tx verification) │  │   (signing/verify) │                              │
│  └────────────────────┘  └────────────────────┘                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Architecture

### Namespaces

```cpp
namespace blockit {
    // Key class (root namespace)
    class Key;

    namespace ledger {
        // All ledger types
        template<typename T> class Chain;
        template<typename T> class Block;
        template<typename T> class Transaction;
        class MerkleTree;
        class PoAConsensus;
        class Validator;
        class Authenticator;
        // ...
    }

    // Backward compatibility aliases in blockit namespace
    using ledger::Chain;
    using ledger::Block;
    using ledger::Transaction;
    // ...
}
```

## Key Class

### Header: `<blockit/key.hpp>`

Ed25519 cryptographic key for signing and verification.

```cpp
namespace blockit {

class Key {
public:
    // === Generation ===

    // Generate new key pair
    static dp::Result<Key, dp::Error> generate();

    // Generate with expiration time
    static dp::Result<Key, dp::Error> generateWithExpiration(
        std::chrono::system_clock::time_point expiration
    );

    // Create from existing keylock keypair
    explicit Key(const keylock::KeyPair& keypair);

    // === Identity ===

    // Get unique ID (SHA256 hash of public key)
    std::string getId() const;

    // Get raw public key bytes
    std::vector<uint8_t> getPublicKey() const;

    // === Cryptographic Operations ===

    // Sign data
    dp::Result<std::vector<uint8_t>, dp::Error> sign(
        const std::vector<uint8_t>& data
    ) const;

    // Verify signature
    dp::Result<bool, dp::Error> verify(
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& signature
    ) const;

    // === Validity ===

    // Check if key is valid (has private key and not expired)
    bool isValid() const;

    // Check if key has expired
    bool isExpired() const;

    // Get expiration time (0 if no expiration)
    dp::i64 getExpiration() const;

    // === Serialization ===

    // Serialize key (includes private key if available)
    std::vector<uint8_t> serialize() const;

    // Deserialize key
    static dp::Result<Key, dp::Error> deserialize(
        const std::vector<uint8_t>& data
    );

    // Create public-key-only Key from bytes
    static dp::Result<Key, dp::Error> fromPublicKey(
        const std::vector<uint8_t>& public_key
    );
};

} // namespace blockit
```

### Example Usage

```cpp
#include <blockit/key.hpp>
using namespace blockit;

// Generate key
auto key_result = Key::generate();
if (!key_result.is_ok()) {
    std::cerr << "Key generation failed\n";
    return;
}
auto key = key_result.value();

std::cout << "Key ID: " << key.getId() << "\n";
// 7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069

// Sign data
std::vector<uint8_t> message = {'H', 'E', 'L', 'L', 'O'};
auto signature = key.sign(message).value();

// Verify signature
auto valid = key.verify(message, signature).value();
std::cout << "Signature valid: " << (valid ? "yes" : "no") << "\n";

// Key with expiration
auto expiration = std::chrono::system_clock::now() + std::chrono::hours(24);
auto expiring_key = Key::generateWithExpiration(expiration).value();
std::cout << "Expired: " << (expiring_key.isExpired() ? "yes" : "no") << "\n";

// Serialize for storage
auto serialized = key.serialize();
auto restored = Key::deserialize(serialized).value();
std::cout << "ID matches: " << (key.getId() == restored.getId() ? "yes" : "no") << "\n";
```

## Chain

### Header: `<blockit/ledger/chain.hpp>`

Generic blockchain implementation - an ordered sequence of blocks.

```cpp
namespace blockit {
namespace ledger {

template <typename T>
class Chain {
public:
    // === Construction ===

    Chain() = default;

    // Create chain with genesis block
    Chain(const std::string& name,
          const std::string& genesis_tx_id,
          const T& genesis_data);

    // === Block Operations ===

    // Add a new block
    dp::Result<void, dp::Error> addBlock(const Block<T>& block);

    // Get block by index
    dp::Result<Block<T>, dp::Error> getBlock(dp::i64 index) const;

    // Get latest block
    dp::Result<Block<T>, dp::Error> getLatestBlock() const;

    // Get block count
    dp::i64 getBlockCount() const;

    // === Chain Validation ===

    // Validate entire chain integrity
    dp::Result<bool, dp::Error> isValid() const;

    // === Participant Management (via Authenticator) ===

    // Register participant
    dp::Result<void, dp::Error> registerParticipant(
        const std::string& participant_id,
        const std::string& initial_state = "inactive"
    );

    // Check if participant is authorized
    bool isParticipantAuthorized(const std::string& participant_id) const;

    // Update participant state
    dp::Result<void, dp::Error> updateParticipantState(
        const std::string& participant_id,
        const std::string& new_state
    );

    // === Capability Management ===

    // Grant capability to participant
    dp::Result<void, dp::Error> grantCapability(
        const std::string& participant_id,
        const std::string& capability
    );

    // Revoke capability from participant
    dp::Result<void, dp::Error> revokeCapability(
        const std::string& participant_id,
        const std::string& capability
    );

    // Check if participant has capability
    bool canParticipantPerform(
        const std::string& participant_id,
        const std::string& capability
    ) const;

    // Get participant capabilities
    std::vector<std::string> getParticipantCapabilities(
        const std::string& participant_id
    ) const;

    // === Transaction Management ===

    // Mark transaction as used (replay protection)
    dp::Result<void, dp::Error> markTransactionUsed(const std::string& tx_id);

    // Check if transaction was used
    bool isTransactionUsed(const std::string& tx_id) const;

    // === Persistence ===

    // Save chain to file
    dp::Result<void, dp::Error> saveToFile(const std::string& filepath) const;

    // Load chain from file
    dp::Result<void, dp::Error> loadFromFile(const std::string& filepath);

    // === Serialization ===

    dp::ByteBuf serialize() const;
    static dp::Result<Chain<T>, dp::Error> deserialize(const dp::ByteBuf& data);

    // === Chain Info ===

    std::string getName() const;
    Authenticator& getAuthenticator();
    const Authenticator& getAuthenticator() const;
};

} // namespace ledger
} // namespace blockit
```

### Example Usage

```cpp
#include <blockit/ledger/chain.hpp>
using namespace blockit;

// Define data type
struct TaskData {
    std::string task_id;
    std::string status;
    std::string to_string() const { return task_id + ":" + status; }
    auto members() { return std::tie(task_id, status); }
};

// Create chain with genesis
TaskData genesis{"GENESIS", "init"};
Chain<TaskData> task_chain("TaskChain", "genesis_tx", genesis);

// Register participants
task_chain.registerParticipant("LEADER_001", "active");
task_chain.registerParticipant("SCOUT_001", "active");

// Grant capabilities
task_chain.grantCapability("LEADER_001", "assign_task");
task_chain.grantCapability("LEADER_001", "approve_member");
task_chain.grantCapability("SCOUT_001", "claim_task");
task_chain.grantCapability("SCOUT_001", "complete_task");

// Check permissions
if (task_chain.canParticipantPerform("LEADER_001", "assign_task")) {
    std::cout << "Leader can assign tasks\n";
}

// Create and add block
auto key = Key::generate().value();
TaskData task{"TASK_001", "pending"};
Transaction<TaskData> tx("tx_001", task, 100);
tx.signTransaction(std::make_shared<Crypto>("key"));

Block<TaskData> block({tx});
task_chain.addBlock(block).value();

std::cout << "Blocks: " << task_chain.getBlockCount() << "\n";

// Validate chain
auto valid = task_chain.isValid();
if (valid.is_ok() && valid.value()) {
    std::cout << "Chain is valid\n";
}

// Save to file
task_chain.saveToFile("/backup/task_chain.dat").value();
```

## Block

### Header: `<blockit/ledger/block.hpp>`

A block containing transactions, Merkle root, and PoA signatures.

```cpp
namespace blockit {
namespace ledger {

template <typename T>
class Block {
public:
    // === Public Members ===
    dp::i64 index_;                              // Block number
    dp::String previous_hash_;                   // Hash of previous block
    dp::String hash_;                            // This block's hash
    dp::Vector<Transaction<T>> transactions_;   // Transactions in block
    dp::i64 nonce_;                              // For future PoW support
    Timestamp timestamp_;                        // Block creation time
    dp::String merkle_root_;                     // Merkle root of transactions

    // PoA consensus fields
    dp::String proposer_id_;                     // Who proposed this block
    dp::Vector<BlockSignature> validator_signatures_;  // Validator signatures

    // === Construction ===

    Block() = default;

    // Create block from transactions
    explicit Block(const std::vector<Transaction<T>>& txns);

    // === Serialization Support ===
    auto members();
    auto members() const;

    // === PoA Methods ===

    // Add validator signature
    dp::Result<void, dp::Error> addValidatorSignature(
        const std::string& validator_id,
        const std::string& participant_id,
        const std::vector<uint8_t>& signature
    );

    // Check if validator already signed
    bool hasSigned(const std::string& validator_id) const;

    // Count valid signatures
    size_t countValidSignatures() const;

    // Set/get proposer
    void setProposer(const std::string& proposer_id);
    std::string getProposer() const;

    // === Merkle Tree ===

    // Build Merkle tree from transactions
    dp::Result<void, dp::Error> buildMerkleTree();

    // === Hashing ===

    // Calculate block hash
    dp::Result<std::string, dp::Error> calculateHash() const;

    // === Validation ===

    // Validate block (hash, merkle root, transactions)
    dp::Result<bool, dp::Error> isValid() const;

    // Verify specific transaction using Merkle proof
    dp::Result<bool, dp::Error> verifyTransaction(size_t transaction_index) const;

    // === Serialization ===

    dp::ByteBuf serialize() const;
    static dp::Result<Block<T>, dp::Error> deserialize(const dp::ByteBuf& data);
    static dp::Result<Block<T>, dp::Error> deserialize(const dp::u8* data, dp::usize size);
};

// Block signature structure
struct BlockSignature {
    dp::String validator_id;      // Validator's key ID
    dp::String participant_id;    // Human-readable participant name
    dp::Vector<dp::u8> signature; // Ed25519 signature
    dp::i64 signed_at;            // Timestamp when signed

    auto members() {
        return std::tie(validator_id, participant_id, signature, signed_at);
    }
};

} // namespace ledger
} // namespace blockit
```

### Example Usage

```cpp
#include <blockit/ledger/block.hpp>
using namespace blockit;

// Create transactions
auto key = Key::generate().value();
auto crypto = std::make_shared<Crypto>("signing_key");

Transaction<TaskData> tx1("tx_001", TaskData{"TASK_001", "pending"}, 100);
tx1.signTransaction(crypto);

Transaction<TaskData> tx2("tx_002", TaskData{"TASK_002", "pending"}, 100);
tx2.signTransaction(crypto);

// Create block
Block<TaskData> block({tx1, tx2});

std::cout << "Block hash: " << block.hash_.c_str() << "\n";
std::cout << "Merkle root: " << block.merkle_root_.c_str() << "\n";
std::cout << "Transactions: " << block.transactions_.size() << "\n";

// Add PoA signatures
std::vector<uint8_t> hash_bytes(block.hash_.begin(), block.hash_.end());
auto sig1 = key.sign(hash_bytes).value();
block.addValidatorSignature(key.getId(), "LEADER_ALPHA", sig1);

auto key2 = Key::generate().value();
auto sig2 = key2.sign(hash_bytes).value();
block.addValidatorSignature(key2.getId(), "LEADER_BETA", sig2);

std::cout << "Signatures: " << block.countValidSignatures() << "\n";

// Validate block
auto valid = block.isValid();
if (valid.is_ok() && valid.value()) {
    std::cout << "Block is valid\n";
}

// Verify specific transaction
auto tx_valid = block.verifyTransaction(0);
if (tx_valid.is_ok() && tx_valid.value()) {
    std::cout << "Transaction 0 verified via Merkle proof\n";
}
```

## Transaction

### Header: `<blockit/ledger/transaction.hpp>`

A signed transaction containing typed data.

```cpp
namespace blockit {
namespace ledger {

// Timestamp structure
struct Timestamp {
    dp::i32 sec;      // Seconds since epoch
    dp::u32 nanosec;  // Nanoseconds

    auto members();
    auto members() const;
};

// Type trait: check for to_string method
template <typename T>
class has_to_string {
    // ... SFINAE implementation
public:
    static constexpr bool value;
};

template <typename T>
class Transaction {
    static_assert(has_to_string<T>::value,
                  "Type T must have a 'to_string() const' method");

public:
    // === Public Members ===
    Timestamp timestamp_;           // Creation timestamp
    dp::i16 priority_;              // Priority (0-255)
    dp::String uuid_;               // Unique transaction ID
    T function_;                    // The actual data
    dp::Vector<dp::u8> signature_;  // Ed25519 signature

    // === Construction ===

    Transaction() = default;

    Transaction(const std::string& uuid, T function, dp::i16 priority = 100);

    // === Serialization Support ===
    auto members();
    auto members() const;

    // === Signing ===

    // Sign transaction with Crypto instance
    dp::Result<void, dp::Error> signTransaction(std::shared_ptr<Crypto> privateKey);

    // === Validation ===

    // Check if transaction is valid (has uuid, function, signature)
    dp::Result<bool, dp::Error> isValid() const;

    // === String Representation ===

    // Get string for signing (timestamp + priority + uuid + function.to_string())
    std::string toString() const;

    // === Serialization ===

    dp::ByteBuf serialize() const;
    static dp::Result<Transaction<T>, dp::Error> deserialize(const dp::ByteBuf& data);
    static dp::Result<Transaction<T>, dp::Error> deserialize(const dp::u8* data, dp::usize size);
};

} // namespace ledger
} // namespace blockit
```

### Data Type Requirements

Your data type must have a `to_string() const` method:

```cpp
struct MyData {
    std::string id;
    int value;

    // REQUIRED: to_string() method for transaction signing
    std::string to_string() const {
        return id + ":" + std::to_string(value);
    }

    // REQUIRED: members() method for datapod serialization
    auto members() { return std::tie(id, value); }
    auto members() const { return std::tie(id, value); }
};
```

### Example Usage

```cpp
#include <blockit/ledger/transaction.hpp>
using namespace blockit;

struct SensorReading {
    std::string sensor_id;
    double value;

    std::string to_string() const {
        return sensor_id + ":" + std::to_string(value);
    }
    auto members() { return std::tie(sensor_id, value); }
};

// Create transaction
SensorReading reading{"TEMP_001", 23.5};
Transaction<SensorReading> tx("tx_sensor_001", reading, 100);

std::cout << "TX ID: " << tx.uuid_.c_str() << "\n";
std::cout << "Priority: " << tx.priority_ << "\n";
std::cout << "Data: " << tx.function_.to_string() << "\n";

// Sign transaction
auto crypto = std::make_shared<Crypto>("sensor_key");
tx.signTransaction(crypto).value();

std::cout << "Signature size: " << tx.signature_.size() << " bytes\n";

// Validate
auto valid = tx.isValid();
if (valid.is_ok() && valid.value()) {
    std::cout << "Transaction is valid\n";
}

// Serialize
auto serialized = tx.serialize();
auto restored = Transaction<SensorReading>::deserialize(serialized).value();
```

## Merkle Tree

### Header: `<blockit/ledger/merkle.hpp>`

Merkle tree for efficient transaction verification.

```cpp
namespace blockit {
namespace ledger {

class MerkleTree {
public:
    MerkleTree() = default;

    // Create from transaction strings
    explicit MerkleTree(const std::vector<std::string>& transaction_strings);

    // === Tree Building ===

    // Build/rebuild tree
    dp::Result<void, dp::Error> buildTree();

    // === Root Access ===

    // Get root hash (Result version)
    dp::Result<std::string, dp::Error> getRoot() const;

    // Get root hash (unsafe version, returns empty if tree empty)
    std::string getRootUnsafe() const;

    // Check if tree is empty
    bool isEmpty() const;

    // Get transaction count
    size_t getTransactionCount() const;

    // === Proof Generation ===

    // Get proof for transaction at index
    dp::Result<std::vector<std::string>, dp::Error> getProof(
        size_t transaction_index
    ) const;

    // Generate proof by finding transaction data
    dp::Result<std::vector<std::string>, dp::Error> generateProof(
        const std::string& transaction_data
    ) const;

    // === Proof Verification ===

    // Verify proof for transaction at index
    dp::Result<bool, dp::Error> verifyProof(
        const std::string& transaction_data,
        size_t transaction_index,
        const std::vector<std::string>& proof
    ) const;

    // Verify proof against expected root
    dp::Result<bool, dp::Error> verifyProof(
        const std::string& transaction_data,
        const std::vector<std::string>& proof,
        const std::string& expected_root
    ) const;
};

} // namespace ledger
} // namespace blockit
```

### How Merkle Trees Work

```
Level 3 (Root):                    [Root Hash]
                                   /          \
Level 2:               [Hash(L0+L1)]          [Hash(L2+L3)]
                       /          \            /          \
Level 1:         [Hash(A)]    [Hash(B)]  [Hash(C)]    [Hash(D)]
                    |            |          |            |
Level 0 (Leaves):  TX_A        TX_B       TX_C         TX_D
```

**Proof for TX_B**: `[Hash(A), Hash(L2+L3)]`
- Start with Hash(TX_B)
- Combine with Hash(A) to get Hash(L0+L1)
- Combine with Hash(L2+L3) to get Root
- Compare with stored root

### Example Usage

```cpp
#include <blockit/ledger/merkle.hpp>
using namespace blockit::ledger;

// Create tree from transaction strings
std::vector<std::string> tx_strings = {
    "tx1:data1",
    "tx2:data2",
    "tx3:data3",
    "tx4:data4"
};

MerkleTree tree(tx_strings);

// Get root
auto root = tree.getRoot().value();
std::cout << "Merkle root: " << root << "\n";

// Generate proof for transaction at index 1
auto proof = tree.getProof(1).value();
std::cout << "Proof elements: " << proof.size() << "\n";

// Verify the proof
auto valid = tree.verifyProof("tx2:data2", 1, proof);
if (valid.is_ok() && valid.value()) {
    std::cout << "Transaction verified!\n";
}

// Verify against specific root
auto valid2 = tree.verifyProof("tx2:data2", proof, root);
```

## Proof-of-Authority Consensus

### Header: `<blockit/ledger/poa.hpp>`

PoA consensus engine for block finalization with configurable quorum.

```cpp
namespace blockit {
namespace ledger {

// PoA configuration
struct PoAConfig {
    dp::u32 initial_required_signatures = 1;     // Minimum signatures needed
    dp::u32 signature_timeout_ms = 30000;        // 30 second timeout
    dp::u32 offline_threshold_ms = 60000;        // 1 minute offline threshold
    dp::u32 max_proposals_per_window = 100;      // Rate limiting
    dp::u32 rate_limit_window_ms = 60000;        // 1 minute window

    auto members();
};

// Proposal states
enum class ProposalState : dp::u8 {
    PENDING = 0,    // Waiting for signatures
    READY = 1,      // Has enough signatures
    EXPIRED = 2,    // Timed out
    REJECTED = 3,   // Explicitly rejected
};

// Signature for finalized proposals
struct FinalizedSignature {
    std::string validator_id;      // Key ID
    std::string participant_id;    // Human name
    std::vector<uint8_t> signature;
    dp::i64 signed_at;
};

class PoAConsensus {
public:
    explicit PoAConsensus(const PoAConfig& config = PoAConfig{});

    // === Validator Management ===

    // Add validator with Key identity
    dp::Result<void, dp::Error> addValidator(
        const std::string& participant_id,
        const Key& identity,
        int weight = 1
    );

    // Remove validator
    dp::Result<void, dp::Error> removeValidator(const std::string& validator_id);

    // Get validator
    dp::Result<Validator*, dp::Error> getValidator(const std::string& validator_id);

    // Get all validators
    std::vector<Validator*> getAllValidators();
    std::vector<const Validator*> getAllValidators() const;

    // Get active validator count
    size_t getActiveValidatorCount() const;

    // === Validator Status ===

    // Mark validator online/offline
    void markOnline(const std::string& validator_id);
    void markOffline(const std::string& validator_id);

    // Revoke validator (permanent)
    void revokeValidator(const std::string& validator_id);

    // Update activity timestamp
    void updateValidatorActivity(const std::string& validator_id);

    // === Proposal Management ===

    // Create new proposal
    std::string createProposal(
        const std::string& block_hash,
        const std::string& proposer_id
    );

    // Add signature to proposal
    bool addSignature(
        const std::string& proposal_id,
        const std::string& validator_id,
        const std::vector<uint8_t>& signature
    );

    // Check proposal state
    ProposalState getProposalState(const std::string& proposal_id) const;
    bool isProposalReady(const std::string& proposal_id) const;
    bool isProposalExpired(const std::string& proposal_id) const;

    // Get finalized signatures
    dp::Result<std::vector<FinalizedSignature>, dp::Error> getFinalizedSignatures(
        const std::string& proposal_id
    ) const;

    // Cleanup expired proposals
    void cleanupExpiredProposals();

    // === Rate Limiting ===

    // Check if validator can propose
    dp::Result<bool, dp::Error> canPropose(const std::string& validator_id) const;

    // Record proposal for rate limiting
    void recordProposal(const std::string& validator_id);

    // Get proposal count in current window
    size_t getProposalCount(const std::string& validator_id) const;

    // === Quorum ===

    // Get required signatures (adjusts for offline validators)
    dp::u32 getRequiredSignatures() const;

    // Get current signature count for proposal
    size_t getSignatureCount(const std::string& proposal_id) const;

    // === Configuration ===

    const PoAConfig& getConfig() const;
    void updateConfig(const PoAConfig& new_config);

    // === Serialization ===

    std::vector<uint8_t> serialize() const;
    static dp::Result<PoAConsensus, dp::Error> deserialize(
        const std::vector<uint8_t>& data
    );
};

} // namespace ledger
} // namespace blockit
```

### Consensus Flow

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Create    │────▶│   Collect   │────▶│   Check     │────▶│  Finalize   │
│  Proposal   │     │  Signatures │     │   Quorum    │     │   Block     │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
      │                   │                   │                   │
      ▼                   ▼                   ▼                   ▼
  proposer_id         Validators          quorum >= N        Add block
  creates UUID        sign hash            required           to chain
```

### Example Usage

```cpp
#include <blockit/ledger/poa.hpp>
using namespace blockit;

// Configure consensus (2-of-3 required)
PoAConfig config;
config.initial_required_signatures = 2;
config.signature_timeout_ms = 30000;

PoAConsensus consensus(config);

// Add validators
auto alice_key = Key::generate().value();
auto bob_key = Key::generate().value();
auto charlie_key = Key::generate().value();

consensus.addValidator("Alice", alice_key);
consensus.addValidator("Bob", bob_key);
consensus.addValidator("Charlie", charlie_key);

std::cout << "Active validators: " << consensus.getActiveValidatorCount() << "\n";
std::cout << "Required signatures: " << consensus.getRequiredSignatures() << "\n";

// Create proposal for block
std::string block_hash = "abc123...";
auto proposal_id = consensus.createProposal(block_hash, alice_key.getId());

// Validators sign
auto hash_bytes = std::vector<uint8_t>(block_hash.begin(), block_hash.end());

auto sig_alice = alice_key.sign(hash_bytes).value();
consensus.addSignature(proposal_id, alice_key.getId(), sig_alice);
std::cout << "Quorum: " << (consensus.isProposalReady(proposal_id) ? "yes" : "no") << "\n";

auto sig_bob = bob_key.sign(hash_bytes).value();
bool quorum_reached = consensus.addSignature(proposal_id, bob_key.getId(), sig_bob);

if (quorum_reached) {
    std::cout << "Consensus reached!\n";
    auto signatures = consensus.getFinalizedSignatures(proposal_id).value();
    // Add signatures to block and finalize
}

// Handle offline validator
consensus.markOffline(charlie_key.getId());
std::cout << "Active after Charlie offline: "
          << consensus.getActiveValidatorCount() << "\n";
```

## Validator

### Header: `<blockit/ledger/validator.hpp>`

Validator identity with Key-based authentication.

```cpp
namespace blockit {
namespace ledger {

enum class ValidatorStatus : dp::u8 {
    ACTIVE = 0,   // Can propose and sign blocks
    OFFLINE = 1,  // Not responding (temporary)
    REVOKED = 2,  // Permanently removed
};

class Validator {
public:
    // Create validator with Key identity
    Validator(const std::string& participant_id,
              const Key& identity,
              int weight = 1);

    // === Identity ===

    // Get unique ID (hash of public key)
    std::string getId() const;

    // Get human-readable participant name
    const std::string& getParticipantId() const;

    // Get identity type (always "key")
    std::string getIdentityType() const;

    // Get the Key identity
    const Key& getIdentity() const;

    // === Weight ===

    int getWeight() const;
    void setWeight(int weight);

    // === Status ===

    ValidatorStatus getStatus() const;
    void setStatus(ValidatorStatus status);

    // Check if validator can sign (active + valid identity)
    bool canSign() const;

    // === Cryptographic Operations ===

    // Sign data (delegates to identity)
    dp::Result<std::vector<uint8_t>, dp::Error> sign(
        const std::vector<uint8_t>& data
    ) const;

    // Verify signature (delegates to identity)
    dp::Result<bool, dp::Error> verify(
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& signature
    ) const;

    // === Activity Tracking ===

    // Update last seen timestamp
    void updateActivity();

    // Check if online (based on timeout)
    bool isOnline(int64_t timeout_ms = 60000) const;

    // Get last seen timestamp
    dp::i64 getLastSeen() const;

    // Mark online (sets ACTIVE and updates activity)
    void markOnline();

    // Mark offline
    void markOffline();

    // Revoke (permanent)
    void revokeValidator();

    // === Serialization ===

    std::vector<uint8_t> serialize() const;
    static dp::Result<Validator, dp::Error> deserialize(
        const std::vector<uint8_t>& data
    );
};

} // namespace ledger
} // namespace blockit
```

### Example Usage

```cpp
#include <blockit/ledger/validator.hpp>
using namespace blockit;

// Create validator
auto key = Key::generate().value();
Validator validator("LEADER_ALPHA", key, 1);

std::cout << "ID: " << validator.getId() << "\n";
std::cout << "Participant: " << validator.getParticipantId() << "\n";
std::cout << "Can sign: " << (validator.canSign() ? "yes" : "no") << "\n";

// Sign data
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
auto signature = validator.sign(data).value();

// Verify
auto valid = validator.verify(data, signature).value();
std::cout << "Signature valid: " << (valid ? "yes" : "no") << "\n";

// Status management
validator.markOffline();
std::cout << "Status: " << (int)validator.getStatus() << "\n";  // 1 = OFFLINE

validator.markOnline();
std::cout << "Status: " << (int)validator.getStatus() << "\n";  // 0 = ACTIVE

validator.revokeValidator();
std::cout << "Can sign after revoke: " << (validator.canSign() ? "yes" : "no") << "\n";
```

## Authenticator

### Header: `<blockit/ledger/auth.hpp>`

Access control for participants and capabilities.

```cpp
namespace blockit {
namespace ledger {

class Authenticator {
public:
    Authenticator() = default;

    // === Participant Management ===

    // Register new participant
    dp::Result<void, dp::Error> registerParticipant(
        const std::string& participant_id,
        const std::string& initial_state = "inactive",
        const std::unordered_map<std::string, std::string>& metadata = {}
    );

    // Check if participant is authorized
    bool isParticipantAuthorized(const std::string& participant_id) const;

    // Get/update participant state
    dp::Result<std::string, dp::Error> getParticipantState(
        const std::string& participant_id
    ) const;
    dp::Result<void, dp::Error> updateParticipantState(
        const std::string& participant_id,
        const std::string& new_state
    );

    // === Metadata ===

    dp::Result<std::string, dp::Error> getParticipantMetadata(
        const std::string& participant_id,
        const std::string& key
    ) const;
    dp::Result<void, dp::Error> setParticipantMetadata(
        const std::string& participant_id,
        const std::string& key,
        const std::string& value
    );

    // === Capability Management ===

    // Grant/revoke capabilities
    dp::Result<void, dp::Error> grantCapability(
        const std::string& participant_id,
        const std::string& capability
    );
    dp::Result<void, dp::Error> revokeCapability(
        const std::string& participant_id,
        const std::string& capability
    );

    // Check capability
    bool hasCapability(
        const std::string& participant_id,
        const std::string& capability
    ) const;

    // Get all capabilities
    std::vector<std::string> getParticipantCapabilities(
        const std::string& participant_id
    ) const;

    // === Transaction Tracking ===

    // Check/mark transaction as used (replay protection)
    bool isTransactionUsed(const std::string& tx_id) const;
    dp::Result<void, dp::Error> markTransactionUsed(const std::string& tx_id);

    // === Combined Validation ===

    // Validate participant and record action atomically
    dp::Result<void, dp::Error> validateAndRecordAction(
        const std::string& issuer_participant,
        const std::string& action_description,
        const std::string& tx_id,
        const std::string& required_capability = ""
    );

    // === Query ===

    std::unordered_set<std::string> getAuthorizedParticipants() const;

    // === Debug ===

    void printSystemSummary() const;

    // === Serialization ===

    dp::ByteBuf serialize() const;
    static dp::Result<Authenticator, dp::Error> deserialize(const dp::ByteBuf& data);
};

// Type aliases for domain-specific use
using EntityManager = Authenticator;
using LedgerManager = Authenticator;
using DeviceManager = Authenticator;
using AuthorizationManager = Authenticator;

} // namespace ledger
} // namespace blockit
```

### Example Usage

```cpp
#include <blockit/ledger/auth.hpp>
using namespace blockit;

Authenticator auth;

// Register participants
auth.registerParticipant("LEADER_001", "active", {{"role", "leader"}});
auth.registerParticipant("SCOUT_001", "active", {{"role", "scout"}});
auth.registerParticipant("CARRIER_001", "active", {{"role", "carrier"}});

// Grant capabilities based on role
auth.grantCapability("LEADER_001", "approve_member");
auth.grantCapability("LEADER_001", "assign_task");
auth.grantCapability("LEADER_001", "emergency_shutdown");

auth.grantCapability("SCOUT_001", "report_state");
auth.grantCapability("SCOUT_001", "claim_patrol_task");

auth.grantCapability("CARRIER_001", "report_state");
auth.grantCapability("CARRIER_001", "claim_delivery_task");

// Check permissions
if (auth.hasCapability("LEADER_001", "approve_member")) {
    std::cout << "Leader can approve members\n";
}

if (!auth.hasCapability("CARRIER_001", "approve_member")) {
    std::cout << "Carrier cannot approve members\n";
}

// Validate and record action
auto result = auth.validateAndRecordAction(
    "LEADER_001",              // Who is doing this
    "Assigned task TASK_001",  // Description
    "tx_assign_001",           // Transaction ID
    "assign_task"              // Required capability
);

if (result.is_ok()) {
    std::cout << "Action authorized and recorded\n";
}

// Replay protection
if (auth.isTransactionUsed("tx_assign_001")) {
    std::cout << "Transaction already used - replay attack blocked\n";
}

// Print summary
auth.printSystemSummary();
```

## Serialization

All ledger types support binary serialization via datapod:

```cpp
// Serialize
auto bytes = chain.serialize();
auto block_bytes = block.serialize();
auto tx_bytes = tx.serialize();

// Deserialize
auto chain = Chain<T>::deserialize(bytes).value();
auto block = Block<T>::deserialize(block_bytes).value();
auto tx = Transaction<T>::deserialize(tx_bytes).value();
```

### Custom Type Requirements

```cpp
struct MyData {
    dp::String id;
    dp::i32 value;
    dp::Vector<dp::u8> data;

    // Required: to_string() for transaction signing
    std::string to_string() const {
        return std::string(id.c_str()) + ":" + std::to_string(value);
    }

    // Required: members() for datapod serialization
    auto members() { return std::tie(id, value, data); }
    auto members() const { return std::tie(id, value, data); }
};
```

## Robotics Use Cases

### 1. Multi-Robot Task Assignment

```cpp
struct TaskAssignment {
    std::string task_id;
    std::string robot_id;
    std::string task_type;
    double x, y;
    std::string status;

    std::string to_string() const { return task_id + ":" + robot_id; }
    auto members() { return std::tie(task_id, robot_id, task_type, x, y, status); }
};

// Leader assigns task
if (chain.canParticipantPerform("LEADER_001", "assign_task")) {
    TaskAssignment assignment{
        "TASK_001",
        "SCOUT_003",
        "patrol",
        100.0, 200.0,
        "assigned"
    };

    Transaction<TaskAssignment> tx("assign_001", assignment, 100);
    tx.signTransaction(crypto);

    Block<TaskAssignment> block({tx});
    chain.addBlock(block);
}
```

### 2. Swarm Leader Election via Consensus

```cpp
// Configure 3-of-5 consensus for leader election
PoAConfig config;
config.initial_required_signatures = 3;

PoAConsensus election(config);

// All swarm members participate
for (auto& [id, key] : swarm_members) {
    election.addValidator(id, key);
}

// Propose new leader
std::string proposal = "ELECT_ROBOT_007_AS_LEADER";
auto proposal_id = election.createProposal(proposal, proposer_id);

// Collect votes (signatures)
for (auto& [id, key] : swarm_members) {
    auto vote = key.sign(std::vector<uint8_t>(proposal.begin(), proposal.end()));
    if (election.addSignature(proposal_id, key.getId(), vote.value())) {
        std::cout << "Consensus reached! ROBOT_007 is the new leader.\n";
        break;
    }
}
```

### 3. Sensor Data Integrity Chain

```cpp
struct SensorReading {
    std::string robot_id;
    std::string sensor_type;
    double value;
    uint64_t timestamp;

    std::string to_string() const {
        return robot_id + ":" + sensor_type + ":" + std::to_string(value);
    }
    auto members() { return std::tie(robot_id, sensor_type, value, timestamp); }
};

// Each robot logs sensor readings
Chain<SensorReading> sensor_chain("SensorChain", "genesis", genesis_reading);

// Log reading
SensorReading reading{"ROBOT_042", "lidar", 15.7, timestamp};
Transaction<SensorReading> tx("sensor_042_001", reading, 50);
tx.signTransaction(crypto);

Block<SensorReading> block({tx});
sensor_chain.addBlock(block);

// Verify reading via Merkle proof
auto& latest = sensor_chain.getLatestBlock().value();
auto valid = latest.verifyTransaction(0);
```

### 4. Access Control for Restricted Areas

```cpp
// Setup zone-based access control
Authenticator zone_auth;

zone_auth.registerParticipant("SECURITY_BOT_01", "active");
zone_auth.registerParticipant("MAINTENANCE_BOT_01", "active");
zone_auth.registerParticipant("DELIVERY_BOT_01", "active");

// Grant zone access
zone_auth.grantCapability("SECURITY_BOT_01", "access_zone_restricted");
zone_auth.grantCapability("SECURITY_BOT_01", "access_zone_public");

zone_auth.grantCapability("MAINTENANCE_BOT_01", "access_zone_maintenance");
zone_auth.grantCapability("MAINTENANCE_BOT_01", "access_zone_public");

zone_auth.grantCapability("DELIVERY_BOT_01", "access_zone_public");

// Check access at zone boundary
std::string robot_id = "DELIVERY_BOT_01";
std::string zone = "zone_restricted";

if (zone_auth.hasCapability(robot_id, "access_" + zone)) {
    std::cout << robot_id << " granted access to " << zone << "\n";
} else {
    std::cout << robot_id << " DENIED access to " << zone << "\n";
}
```

### 5. Emergency Shutdown Consensus

```cpp
// Emergency actions require consensus
PoAConfig emergency_config;
emergency_config.initial_required_signatures = 2;  // 2 leaders must agree
emergency_config.signature_timeout_ms = 5000;       // Fast timeout

PoAConsensus emergency_consensus(emergency_config);

// Only leaders can participate
emergency_consensus.addValidator("LEADER_ALPHA", alpha_key);
emergency_consensus.addValidator("LEADER_BETA", beta_key);
emergency_consensus.addValidator("LEADER_GAMMA", gamma_key);

// Initiate emergency shutdown
std::string emergency = "EMERGENCY_SHUTDOWN_ALL_ROBOTS";
auto proposal_id = emergency_consensus.createProposal(emergency, alpha_key.getId());

// Leaders sign
auto sig1 = alpha_key.sign(std::vector<uint8_t>(emergency.begin(), emergency.end())).value();
emergency_consensus.addSignature(proposal_id, alpha_key.getId(), sig1);

auto sig2 = beta_key.sign(std::vector<uint8_t>(emergency.begin(), emergency.end())).value();
if (emergency_consensus.addSignature(proposal_id, beta_key.getId(), sig2)) {
    // Consensus reached - execute emergency shutdown
    broadcast_emergency_shutdown();
}
```
