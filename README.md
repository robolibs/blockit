
<img align="right" width="26%" src="./misc/logo.png">

Blockit
===

A type-safe, template-based blockchain library for C++20 with generic participant authentication and ledger tracking capabilities.


## Features

- **Header-Only Design**: Fully inline functions, no separate compilation required
- **Generic Template Design**: Create blockchains for any data type that has a `to_string()` method
- **Type Safety**: Compile-time enforcement that data types implement required methods
- **Transaction Management**: Create, sign, and validate transactions with custom data types
- **Block Structure**: Organize transactions into blocks with cryptographic hashing
- **Blockchain**: Chain blocks together with hash references and validation
- **Digital Signatures**: RSA-based signing and verification using the Lockey library
- **Timestamp Precision**: Nanosecond-precision timestamps for ordering
- **Priority System**: Transaction priority levels (0-255)
- **SFINAE Type Checking**: Ensures data types have required `to_string()` method at compile time
- **🆕 Generic Authenticator**: Universal participant management for any use case
- **🆕 Capability-Based Authorization**: Flexible permission system for different roles
- **🆕 Metadata Management**: Store and retrieve participant specifications and parameters
- **🆕 State Tracking**: Monitor participant states (active, maintenance, idle, etc.)
- **🆕 Double-Spend Prevention**: Duplicate transaction detection and prevention
- **🆕 Merkle Trees**: Efficient transaction verification with cryptographic proofs
- **🆕 Enhanced Block Validation**: Comprehensive cryptographic integrity checks
- **🆕 Unified Serialization System**: Dual-format serialization supporting both binary and JSON
- **🆕 Binary Serialization**: High-performance, compact binary format with 37% size reduction
- **🆕 JSON Serialization**: Human-readable format for debugging and interoperability
- **🆕 Format Auto-Detection**: Automatic detection and handling of both serialization formats
- **🆕 Persistent Storage**: Complete file I/O for blockchain persistence in both formats
- **🆕 Backward Compatibility**: Existing JSON-based code continues to work unchanged
- **🆕 Performance Optimization**: Binary format offers 3.7x faster serialization speed

## Unified Serialization System

The library features a comprehensive dual-format serialization architecture that provides both high-performance binary and human-readable JSON serialization:

### Key Features
- **🚀 High Performance**: Binary format offers 37% size reduction and 3.7x faster serialization
- **🔄 Dual Format Support**: Choose JSON for readability or binary for performance
- **🤖 Auto-Detection**: Automatic format detection and handling
- **🔧 Type-Aware**: SFINAE detection automatically selects optimal serialization method
- **⬅️ Backward Compatible**: Existing JSON-based code continues to work unchanged
- **🛠️ Endian-Safe**: Cross-platform binary format with proper byte ordering

### Performance Metrics
| Format | Size | Serialization | Deserialization |
|--------|------|---------------|-----------------|
| JSON   | 5,596 bytes | 11 μs | 23 μs |
| Binary | 3,481 bytes | 3 μs | 1 μs |
| **Improvement** | **-37.8%** | **+267%** | **+2300%** |

## Use Cases

This library is designed for **ledger tracking and coordination systems** across various industries:

- **🤖 Robotics**: Multi-robot coordination with command authorization
- **🚜 Agriculture**: Equipment tracking, sensor networks, maintenance logs
- **🏭 Manufacturing**: Production line coordination and quality control
- **🏥 Healthcare**: Medical device coordination and patient data integrity
- **⚡ Energy**: Smart grid management and meter readings
- **🚛 Supply Chain**: Asset tracking and verification
- **🏙️ Smart Cities**: Infrastructure monitoring and management
- **🔬 Research**: Data integrity and experiment tracking

## Quick Start

The library uses templates throughout - `Transaction<T>`, `Block<T>`, and `Chain<T>` where `T` must have a `to_string()` method.

```cpp
#include "blokit/blokit.hpp"
#include <memory>

// First, create a wrapper for types that don't have to_string()
class StringWrapper {
private:
    std::string value_;
public:
    StringWrapper(const std::string& str) : value_(str) {}
    StringWrapper(const char* str) : value_(str) {}
    std::string to_string() const { return value_; }
};

// Create a crypto instance for signing
auto privateKey = std::make_shared<chain::Crypto>("key_file");

// Create a blockchain using StringWrapper
chain::Chain<StringWrapper> blockchain("my-chain", "genesis-tx", StringWrapper("genesis_data"), privateKey);

// Register participants in the blockchain
blockchain.registerParticipant("device-001", "active", {{"type", "sensor"}, {"location", "field_A"}});
blockchain.registerParticipant("device-002", "active", {{"type", "actuator"}, {"location", "field_B"}});

// Grant capabilities to participants
blockchain.grantCapability("device-001", "READ_DATA");
blockchain.grantCapability("device-002", "WRITE_DATA");

// Create a transaction
chain::Transaction<StringWrapper> tx("tx-001", StringWrapper("transfer_funds"), 100);
tx.signTransaction(privateKey);

// Create a block with transactions
std::vector<chain::Transaction<StringWrapper>> transactions = {tx};
chain::Block<StringWrapper> block(transactions);

// Add block to blockchain (with duplicate detection)
blockchain.addBlock(block);

// Save blockchain to file
blockchain.saveToFile("my_blockchain.json");

// Load blockchain from file
chain::Chain<StringWrapper> loadedChain;
loadedChain.loadFromFile("my_blockchain.json");

// Validate the chain (with enhanced cryptographic checks)
bool isValid = blockchain.isValid();
```

### Using Custom Data Types

Any type can be used as long as it implements a `to_string()` method. For enhanced serialization capabilities, optionally implement both `serialize()`/`deserialize()` methods for JSON and `serializeBinary()`/`deserializeBinary()` methods for binary format:

```cpp
// Custom data type for payments with dual serialization support
struct PaymentData {
    std::string from, to;
    double amount;
    
    // Required: must have to_string() method
    std::string to_string() const {
        return "Payment{from:" + from + ",to:" + to + ",amount:" + std::to_string(amount) + "}";
    }
    
    // Optional: JSON serialization for human-readable format
    std::string serialize() const {
        return R"({"from": ")" + from + R"(", "to": ")" + to + 
               R"(", "amount": )" + std::to_string(amount) + "}";
    }
    
    static PaymentData deserialize(const std::string& data) {
        // Parse JSON data and return PaymentData instance
        PaymentData result;
        // ... JSON parsing implementation ...
        return result;
    }
    
    // Optional: Binary serialization for high-performance format
    std::vector<uint8_t> serializeBinary() const {
        std::vector<uint8_t> buffer;
        chain::BinarySerializer::writeString(buffer, from);
        chain::BinarySerializer::writeString(buffer, to);
        chain::BinarySerializer::writeDouble(buffer, amount);
        return buffer;
    }
    
    static PaymentData deserializeBinary(const std::vector<uint8_t>& data) {
        PaymentData result;
        size_t offset = 0;
        result.from = chain::BinarySerializer::readString(data, offset);
        result.to = chain::BinarySerializer::readString(data, offset);
        result.amount = chain::BinarySerializer::readDouble(data, offset);
        return result;
    }
};

// Create a blockchain for custom payment data
PaymentData payment{"Alice", "Bob", 100.50};
chain::Chain<PaymentData> paymentChain("payment-chain", "pay-genesis", payment, privateKey);

// Create transaction with custom data
chain::Transaction<PaymentData> payTx("pay-001", PaymentData{"Bob", "Charlie", 50.25}, 150);
payTx.signTransaction(privateKey);

// Save using JSON format (default, backward compatible)
paymentChain.saveToFile("payments.json");

// Transactions and blocks can use either format:
std::string jsonTx = payTx.serialize();              // JSON format
std::vector<uint8_t> binaryTx = payTx.serializeBinary(); // Binary format

// Auto-detection works with both formats
auto deserializedTx = chain::Transaction<PaymentData>::deserializeAuto(binaryTx);
```

### Type Requirements

The template system uses SFINAE to ensure your type has a `to_string()` method:

```cpp
// ✅ This works - has to_string() method
struct ValidType {
    int value;
    std::string to_string() const { return std::to_string(value); }
};

// ❌ This fails at compile time - no to_string() method
struct InvalidType {
    int value;
    // Missing to_string() method!
};

// Compile-time error with helpful message:
// "Type T must have a to_string() method"
chain::Transaction<InvalidType> tx; // Won't compile!
```

## Unified Serialization System

The library provides a comprehensive dual-format serialization system supporting both high-performance binary and human-readable JSON formats:

### Dual Format Support

```cpp
// Create and populate a blockchain
chain::Chain<StringWrapper> blockchain("my-chain", "genesis", StringWrapper("data"), privateKey);

// Add some transactions and blocks
// ... blockchain operations ...

// Traditional JSON serialization (backward compatible)
bool saved = blockchain.saveToFile("blockchain.json");

// Individual components support both formats:
std::string jsonData = transaction.serialize();              // JSON (default)
std::string explicitJson = transaction.serializeJson();     // Explicit JSON
std::vector<uint8_t> binaryData = transaction.serializeBinary(); // Binary

// Auto-detection handles both formats seamlessly
auto tx1 = chain::Transaction<T>::deserializeAuto(binaryData);    // Detects binary
auto tx2 = chain::Transaction<T>::deserializeAuto(jsonAsBytes);   // Detects JSON
```

### Performance Comparison

The binary format offers significant performance advantages:

| Metric | JSON Format | Binary Format | Improvement |
|--------|-------------|---------------|-------------|
| **File Size** | 5,596 bytes | 3,481 bytes | **37.8% smaller** |
| **Serialization Speed** | 11 μs | 3 μs | **3.7x faster** |
| **Deserialization Speed** | 23 μs | 1 μs | **23x faster** |

### Format Selection Guide

**Choose Binary Format When:**
- ✅ Performance is critical
- ✅ Storage space is limited
- ✅ High-frequency operations
- ✅ Network transmission efficiency matters

**Choose JSON Format When:**
- ✅ Human readability is important
- ✅ Debugging and inspection needed
- ✅ Interoperability with other systems
- ✅ Legacy system compatibility

### Advanced Serialization API

```cpp
// Type-aware serialization system
chain::TypeSerializer<PaymentData> serializer;

// Check format capabilities at runtime
bool supportsBinary = serializer.supportsBinary();  // true if serializeBinary() exists
bool supportsJson = serializer.supportsJson();      // true if serialize() exists

// Serialize using the optimal format for the type
if (supportsBinary) {
    auto binaryData = serializer.serializeBinary(payment);
    // Binary format: ~37% smaller, much faster
} else {
    auto jsonData = serializer.serializeJson(payment);
    // Fallback to JSON format
}

// Format auto-detection with error handling
try {
    auto payment = chain::Transaction<PaymentData>::deserializeAuto(data);
    // Automatically detects and handles both binary and JSON formats
} catch (const std::exception& e) {
    // Handle unsupported or corrupted format
}
```

### Basic File Operations

```cpp
// Load blockchain from file (auto-detects format)
chain::Chain<StringWrapper> loadedChain;
bool loaded = loadedChain.loadFromFile("blockchain.json");

// Verify data integrity
bool isValid = loadedChain.isValid();
```

### Binary Serialization API

All blockchain components support both serialization formats:

```cpp
// JSON serialization (default for backward compatibility)
std::string txJson = transaction.serialize();
std::string blockJson = block.serialize();
std::string chainJson = blockchain.serialize();

// Binary serialization (new high-performance format)
std::vector<uint8_t> txBinary = transaction.serializeBinary();
std::vector<uint8_t> blockBinary = block.serializeBinary();

// Mixed deserialization with auto-detection
auto txFromJson = chain::Transaction<T>::deserialize(txJson);
auto txFromBinary = chain::Transaction<T>::deserializeBinary(txBinary);
auto txAutoDetected = chain::Transaction<T>::deserializeAuto(binaryData);

// Both formats produce identical results
assert(txFromJson.uuid_ == txFromBinary.uuid_);
```

### Custom Type Serialization

For optimal performance and flexibility, implement both serialization methods in your custom types:

```cpp
struct SensorData {
    std::string sensor_id;
    double temperature;
    int64_t timestamp;
    
    // Required for basic blockchain operations
    std::string to_string() const {
        return "Sensor{" + sensor_id + ":" + std::to_string(temperature) + "}";
    }
    
    // JSON serialization (human-readable, interoperable)
    std::string serialize() const {
        return R"({"sensor_id": ")" + sensor_id + 
               R"(", "temperature": )" + std::to_string(temperature) + 
               R"(, "timestamp": )" + std::to_string(timestamp) + "}";
    }
    
    static SensorData deserialize(const std::string& data) {
        SensorData result;
        // Parse JSON and populate fields
        // ... JSON parsing implementation ...
        return result;
    }
    
    // Binary serialization (high-performance, compact)
    std::vector<uint8_t> serializeBinary() const {
        std::vector<uint8_t> buffer;
        chain::BinarySerializer::writeString(buffer, sensor_id);
        chain::BinarySerializer::writeDouble(buffer, temperature);
        chain::BinarySerializer::writeInt64(buffer, timestamp);
        return buffer;
    }
    
    static SensorData deserializeBinary(const std::vector<uint8_t>& data) {
        SensorData result;
        size_t offset = 0;
        result.sensor_id = chain::BinarySerializer::readString(data, offset);
        result.temperature = chain::BinarySerializer::readDouble(data, offset);
        result.timestamp = chain::BinarySerializer::readInt64(data, offset);
        return result;
    }
};
```

### BinarySerializer Utility

The library provides a comprehensive `BinarySerializer` class for endian-safe binary operations:

```cpp
// Writing data
std::vector<uint8_t> buffer;
chain::BinarySerializer::writeString(buffer, "hello");      // Strings with length prefix
chain::BinarySerializer::writeDouble(buffer, 3.14159);      // IEEE 754 double
chain::BinarySerializer::writeUInt32(buffer, 42);          // 32-bit unsigned integer
chain::BinarySerializer::writeBytes(buffer, binaryData);    // Raw byte arrays

// Reading data
size_t offset = 0;
std::string str = chain::BinarySerializer::readString(buffer, offset);
double value = chain::BinarySerializer::readDouble(buffer, offset);
uint32_t number = chain::BinarySerializer::readUInt32(buffer, offset);
auto bytes = chain::BinarySerializer::readBytes(buffer, offset, length);
```

### Storage Features

- **Dual Format Support**: Choose between JSON (human-readable) and binary (high-performance)
- **Format Auto-Detection**: Automatic detection and handling of both serialization formats
- **Backward Compatibility**: Existing JSON-based code continues to work unchanged
- **Performance Optimization**: Binary format offers 37% size reduction and 3.7x speed improvement
- **Endian Safety**: Cross-platform binary format with proper byte ordering
- **Type Safety**: SFINAE detection automatically selects appropriate serialization methods
- **Binary Signatures**: Base64 encoding for cryptographic signatures in JSON format
- **Data Integrity**: Validation on load to ensure blockchain consistency
- **Error Handling**: Comprehensive error reporting for file operations
- **Memory Efficient**: Streaming approach for large blockchain files
- **Cross-Platform**: Compatible with all major operating systems

### Testing Serialization

The library includes comprehensive serialization tests:

```bash
cd build
./test_storage    # Run all serialization and storage tests
```

The serialization test suite covers:
- **Binary vs JSON Transaction Serialization**: Format comparison and data integrity
- **Binary vs JSON Block Serialization**: Complete block state preservation
- **Performance Comparison**: Speed and size benchmarks between formats
- **Format Auto-Detection**: Automatic format identification and handling
- **Unified Serialization System Integration**: End-to-end testing with type detection
- Basic serialization/deserialization
- File save and load operations
- Database integration simulation
- Block pruning and archival
- State snapshot management

### Real-World Examples

#### Agricultural Equipment Tracking
```cpp
// Farming operation data structure with dual serialization
struct FarmingOperation {
    std::string equipment_id, operation_type, field_location, crop_type;
    double area_covered;
    
    std::string to_string() const { /* implementation */ }
    
    // JSON serialization for human-readable logs
    std::string serialize() const { /* JSON implementation */ }
    static FarmingOperation deserialize(const std::string& data) { /* implementation */ }
    
    // Binary serialization for high-frequency sensor data
    std::vector<uint8_t> serializeBinary() const {
        std::vector<uint8_t> buffer;
        chain::BinarySerializer::writeString(buffer, equipment_id);
        chain::BinarySerializer::writeString(buffer, operation_type);
        chain::BinarySerializer::writeString(buffer, field_location);
        chain::BinarySerializer::writeString(buffer, crop_type);
        chain::BinarySerializer::writeDouble(buffer, area_covered);
        return buffer;
    }
    
    static FarmingOperation deserializeBinary(const std::vector<uint8_t>& data) {
        FarmingOperation result;
        size_t offset = 0;
        result.equipment_id = chain::BinarySerializer::readString(data, offset);
        result.operation_type = chain::BinarySerializer::readString(data, offset);
        result.field_location = chain::BinarySerializer::readString(data, offset);
        result.crop_type = chain::BinarySerializer::readString(data, offset);
        result.area_covered = chain::BinarySerializer::readDouble(data, offset);
        return result;
    }
};

// Create blockchain for farming operations
chain::Chain<FarmingOperation> farmChain("farm-ops", "genesis", genesis_op, privateKey);

// Register equipment with metadata
farmChain.registerParticipant("tractor-001", "ready", {
    {"model", "John_Deere_8370R"}, {"fuel_capacity", "680L"}
});

// Grant capabilities
farmChain.grantCapability("tractor-001", "TILLAGE");
farmChain.grantCapability("tractor-001", "SEEDING");

// Save using appropriate format based on use case
farmChain.saveToFile("farm_operations.json");  // JSON for reports
// Binary format for high-frequency sensor data would be handled at transaction level
```

#### IoT Sensor Network
```cpp
// Sensor reading data structure with optimized dual serialization
struct SensorReading {
    std::string sensor_id, sensor_type, location;
    double value;
    std::string unit;
    int64_t timestamp;
    
    std::string to_string() const { /* implementation */ }
    
    // JSON serialization for configuration and debugging
    std::string serialize() const {
        return R"({"sensor_id": ")" + sensor_id + 
               R"(", "sensor_type": ")" + sensor_type +
               R"(", "location": ")" + location +
               R"(", "value": )" + std::to_string(value) +
               R"(, "unit": ")" + unit + 
               R"(", "timestamp": )" + std::to_string(timestamp) + R"("})";
    }
    
    static SensorReading deserialize(const std::string& data) { /* implementation */ }
    
    // Binary serialization for high-frequency data logging (37% smaller, 23x faster)
    std::vector<uint8_t> serializeBinary() const {
        std::vector<uint8_t> buffer;
        chain::BinarySerializer::writeString(buffer, sensor_id);
        chain::BinarySerializer::writeString(buffer, sensor_type);
        chain::BinarySerializer::writeString(buffer, location);
        chain::BinarySerializer::writeDouble(buffer, value);
        chain::BinarySerializer::writeString(buffer, unit);
        chain::BinarySerializer::writeInt64(buffer, timestamp);
        return buffer;
    }
    
    static SensorReading deserializeBinary(const std::vector<uint8_t>& data) {
        SensorReading result;
        size_t offset = 0;
        result.sensor_id = chain::BinarySerializer::readString(data, offset);
        result.sensor_type = chain::BinarySerializer::readString(data, offset);
        result.location = chain::BinarySerializer::readString(data, offset);
        result.value = chain::BinarySerializer::readDouble(data, offset);
        result.unit = chain::BinarySerializer::readString(data, offset);
        result.timestamp = chain::BinarySerializer::readInt64(data, offset);
        return result;
    }
};

// Create blockchain for sensor data
chain::Chain<SensorReading> sensorChain("sensors", "init", init_reading, privateKey);

// Register sensors with metadata
sensorChain.registerParticipant("soil-sensor-001", "active", {
    {"type", "soil_moisture"}, {"field", "Field_A"}, {"depth", "15cm"}
});

sensorChain.grantCapability("soil-sensor-001", "READ_SOIL_MOISTURE");

// Flexible serialization based on use case:
// JSON for configuration files and human-readable reports
sensorChain.saveToFile("sensor_config.json");

// Binary format for high-frequency sensor data transactions
chain::Transaction<SensorReading> sensorTx("reading-001", highFrequencyReading, 100);
std::vector<uint8_t> compactData = sensorTx.serializeBinary(); // 37% smaller, 23x faster
```

#### Robot Coordination
```cpp
// Robot command data structure
struct RobotCommand {
    std::string issuer_robot, target_robot, command;
    int priority_level;
    std::string to_string() const { /* implementation */ }
};

// Create blockchain for robot coordination
chain::Chain<RobotCommand> robotChain("robots", "init", init_cmd, privateKey);

// Register robots and grant capabilities
robotChain.registerParticipant("robot-001", "idle");
robotChain.grantCapability("robot-001", "MOVE");
robotChain.grantCapability("robot-001", "PICK");

// Save coordination state for system recovery
robotChain.saveToFile("robot_coordination_state.json");
```

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Running Examples

```bash
cd build
./main              # Original basic demo with persistent storage
./enhanced_demo     # Robot coordination demo  
./farming_demo      # Agricultural industry demo
./test_storage      # Comprehensive storage functionality tests
```

The **original example** demonstrates:
- Generic template usage with `StringWrapper` 
- Transaction creation and signing with custom data types
- Block creation and validation
- Blockchain construction and management
- Cryptographic operations
- **Persistent Storage**: Save and load blockchain to/from JSON files
- **Data Serialization**: Complete serialize/deserialize functionality
- Various advanced scenarios
- Type safety and compile-time checking

The **enhanced example** demonstrates:
- **Robot Coordination**: Multiple robots working together through blockchain authorization
- **Entity Management**: Registration, permissions, and state tracking
- **Ledger Tracking**: Immutable logging for sensors, actuators, and controllers
- **Double-Spend Prevention**: Automatic detection and rejection of duplicate commands
- **Merkle Trees**: Efficient verification of individual transactions in large blocks

The **farming example** demonstrates:
- **Equipment Operations**: Tractors, sprayers, harvesters with metadata and capabilities
- **Sensor Networks**: Soil moisture, weather stations, drone data collection
- **Maintenance Ledger**: Technician records and equipment service tracking
- **Generic Authenticator**: Same system adapted for different agricultural use cases
- **Capability-Based Authorization**: Role-based permissions for different equipment types
- **Data Persistence**: Long-term storage of agricultural operation records

The **storage tests** demonstrate:
- **Binary vs JSON Transaction Serialization**: Complete comparison of both formats with data integrity verification
- **Binary vs JSON Block Serialization**: Full block state preservation in both formats
- **Performance Comparison**: Comprehensive benchmarks showing 37% size reduction and 3.7x speed improvement with binary format
- **Format Auto-Detection**: Automatic format identification and seamless handling of both formats
- **Unified Serialization System Integration**: End-to-end testing of the complete dual-format system
- **Type Capability Detection**: SFINAE-based detection of serialization capabilities
- **Transaction Serialization**: Individual transaction save/load with signature preservation
- **Block Serialization**: Complete block state persistence including Merkle trees
- **Blockchain Serialization**: Full chain state with participant data and metadata
- **File I/O Operations**: Robust error handling and data validation
- **Database Simulation**: Integration patterns for production database systems
- **Archival Systems**: Block pruning and long-term storage strategies
- **State Snapshots**: Point-in-time blockchain state capture and restoration

## Current Limitations

This implementation is **still not** production ready and lacks several critical features for production use:

- ❌ No Proof of Work or consensus mechanism
- ❌ No network layer for distributed operation
- ✅ ~~No persistent storage~~ **Complete unified serialization system with both binary and JSON formats implemented**
- ✅ ~~Limited transaction validation~~ **Enhanced transaction validation with entity permissions**
- ✅ ~~No Merkle trees for efficient verification~~ **Merkle trees implemented for efficient verification**
- ✅ ~~No protection against double spending~~ **Double-spend prevention implemented**
- ❌ No difficulty adjustment

## Not gonna be part of this library
- Fork resolution

## SQLite Integration for Fast Queries

Blockit provides a powerful SQLite integration layer that combines blockchain immutability with fast query capabilities. The library manages core ledger tables (blocks, transactions, anchors) while allowing you to define your own domain-specific schemas.

### Key Features

- **Fast Queries**: Millisecond-speed queries on large datasets using SQLite indexes
- **Blockchain Anchoring**: Link your data to on-chain transactions with cryptographic hashes
- **Schema Extensions**: Define your own tables and indexes that reference the core ledger
- **Verification**: Verify data integrity by comparing against on-chain hashes
- **Flexible**: Use any schema design that fits your application needs

### Quick Start

```cpp
#include <blockit/storage/sqlite_store.hpp>
#include <blockit/storage/sqlite_store_impl.hpp>

using namespace blockit::storage;

// 1. Define your domain data
struct UserRecord {
    std::string user_id;
    std::string name;
    std::string email;
    
    std::vector<uint8_t> toBytes() const {
        std::string data = user_id + "|" + name + "|" + email;
        return std::vector<uint8_t>(data.begin(), data.end());
    }
};

// 2. Define your custom schema extension
class UserSchemaExtension : public ISchemaExtension {
public:
    std::vector<std::string> getCreateTableStatements() const override {
        return {
            R"(CREATE TABLE IF NOT EXISTS users (
                user_id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                email TEXT NOT NULL,
                content_hash BLOB,
                FOREIGN KEY(user_id) REFERENCES anchors(content_id) ON DELETE CASCADE
            ))"
        };
    }
    
    std::vector<std::string> getCreateIndexStatements() const override {
        return {
            "CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)"
        };
    }
};

// 3. Use the storage layer
SqliteStore store;
store.open("myapp.db");
store.initializeCoreSchema();

// 4. Register your schema extension
UserSchemaExtension user_schema;
store.registerExtension(user_schema);

// 5. Store data and create anchor
UserRecord user{"user_001", "Alice", "alice@example.com"};
auto content_hash = computeSHA256(user.toBytes());

// Insert into your custom table
store.executeSql("INSERT INTO users (user_id, name, email, content_hash) VALUES (...)");

// 6. When blockchain transaction is confirmed, create anchor
TxRef tx_ref("tx_abc123", 42, merkle_root);
store.createAnchor("user_001", content_hash, tx_ref);

// 7. Verify data integrity at any time
if (store.verifyAnchor("user_001", user.toBytes())) {
    std::cout << "User data verified against blockchain!\n";
}
```

### Core Ledger Schema

The library automatically manages these tables:

```sql
-- Blocks table (indexed by height and hash)
CREATE TABLE blocks (
    height INTEGER PRIMARY KEY,
    hash TEXT NOT NULL UNIQUE,
    previous_hash TEXT NOT NULL,
    merkle_root TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    nonce INTEGER NOT NULL
);

-- Transactions table (indexed by block and timestamp)
CREATE TABLE transactions (
    tx_id TEXT PRIMARY KEY,
    block_height INTEGER NOT NULL,
    timestamp INTEGER NOT NULL,
    priority INTEGER NOT NULL,
    payload BLOB NOT NULL,
    FOREIGN KEY(block_height) REFERENCES blocks(height)
);

-- Anchors table (links your data to blockchain)
CREATE TABLE anchors (
    content_id TEXT PRIMARY KEY,
    content_hash BLOB NOT NULL,
    tx_id TEXT NOT NULL,
    block_height INTEGER NOT NULL,
    merkle_root BLOB NOT NULL,
    anchored_at INTEGER NOT NULL,
    FOREIGN KEY(tx_id) REFERENCES transactions(tx_id),
    FOREIGN KEY(block_height) REFERENCES blocks(height)
);
```

### Your Schema Extension

You control your own tables:

```cpp
class MyAppSchema : public ISchemaExtension {
public:
    std::vector<std::string> getCreateTableStatements() const override {
        return {
            // Your domain tables can reference core ledger via FKs
            R"(CREATE TABLE IF NOT EXISTS documents (
                doc_id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                content TEXT NOT NULL,
                author TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                FOREIGN KEY(doc_id) REFERENCES anchors(content_id)
            ))",
            
            // Full-text search (optional)
            R"(CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts 
               USING fts5(doc_id UNINDEXED, title, content))"
        };
    }
    
    std::vector<std::string> getCreateIndexStatements() const override {
        return {
            "CREATE INDEX IF NOT EXISTS idx_docs_author ON documents(author)",
            "CREATE INDEX IF NOT EXISTS idx_docs_created ON documents(created_at)"
        };
    }
    
    // Optional: schema migrations for version upgrades
    std::vector<std::pair<int32_t, std::vector<std::string>>> getMigrations() const override {
        return {
            {2, {"ALTER TABLE documents ADD COLUMN tags TEXT"}}
        };
    }
};
```

### Common Patterns

**Pattern 1: Fast Queries + Blockchain Verification**

```cpp
// Query with SQLite speed
store.executeQuery("SELECT * FROM documents WHERE author = 'Alice' ORDER BY created_at DESC",
    [](const std::vector<std::string>& row) {
        std::cout << "Doc: " << row[0] << "\n";
    });

// Verify specific document against blockchain
std::vector<uint8_t> current_content = loadDocumentContent("doc_123");
if (store.verifyAnchor("doc_123", current_content)) {
    std::cout << "Document integrity verified!\n";
}
```

**Pattern 2: Range Queries on Blockchain Data**

```cpp
// Query transactions in a block range
LedgerQuery query;
query.block_height_min = 100;
query.block_height_max = 200;
auto tx_ids = store.queryTransactions(query);

// Get all anchors for those blocks
auto anchors = store.getAnchorsInRange(100, 200);
```

**Pattern 3: Full-Text Search with Blockchain Backing**

```cpp
// Fast FTS query
store.executeQuery("SELECT doc_id FROM documents_fts WHERE documents_fts MATCH 'blockchain'",
    [&](const std::vector<std::string>& row) {
        // Each result is cryptographically anchored
        auto anchor = store.getAnchor(row[0]);
        std::cout << "Found doc " << row[0] << " at block " << anchor->block_height << "\n";
    });
```

### Configuration

```cpp
OpenOptions opts;
opts.enable_wal = true;                           // Write-Ahead Logging for performance
opts.enable_foreign_keys = true;                  // Enforce referential integrity
opts.sync_mode = OpenOptions::Synchronous::NORMAL; // Balance safety and speed
opts.busy_timeout_ms = 5000;                      // Concurrent access timeout
opts.cache_size_kb = 20000;                       // ~20MB cache

store.open("myapp.db", opts);
```

### Examples

See `examples/sqlite_integration_demo.cpp` for a complete example with:
- Custom schema definition
- User creation and anchoring workflow
- Verification and integrity checking
- Range queries and statistics
- Full-text search integration

Run the example:
```bash
cd build
./sqlite_integration_demo
```

### Testing

Comprehensive test suite in `test/test_sqlite_store.cpp`:
```bash
cd build
./test_sqlite_store
```

Tests cover:
- Core database operations
- Block and transaction storage
- Anchor creation and verification
- Schema extensions
- Transaction guards (RAII)
- Query operations
- Chain continuity verification
- Diagnostics and statistics

## Dependencies

- **Lockey**: Cryptographic library for signing and verification
- **SQLite3**: Core SQLite library (system)
- **SQLiteCpp**: C++ wrapper for SQLite (fetched automatically)
- **C++20**: Modern C++ features
- **CMake 3.15+**: Build system

## License

This project is open source. See the license file for details.

---
