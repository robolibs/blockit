# Identity Module - W3C DID & Verifiable Credentials

The Identity module provides W3C-compliant Decentralized Identifiers (DIDs) and Verifiable Credentials (VCs) for robot swarms. This enables robots to have self-sovereign identities that can be verified without central authority.

## Table of Contents

- [Overview](#overview)
- [Core Concepts](#core-concepts)
- [DID (Decentralized Identifier)](#did-decentralized-identifier)
- [DID Document](#did-document)
- [DID Registry](#did-registry)
- [Verification Method](#verification-method)
- [Service Endpoints](#service-endpoints)
- [Verifiable Credentials](#verifiable-credentials)
- [Verifiable Presentations](#verifiable-presentations)
- [Credential Issuer](#credential-issuer)
- [Credential Status](#credential-status)
- [Integration with Blockit](#integration-with-blockit)
- [Robotics Use Cases](#robotics-use-cases)

## Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           IDENTITY MODULE                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────┐     ┌─────────────────┐     ┌──────────────────────────┐   │
│  │     DID     │────▶│   DID Document  │────▶│  Verification Methods    │   │
│  │ did:blockit:│     │  (JSON-LD-like) │     │  - Ed25519VerificationKey│   │
│  │   <hash>    │     │                 │     │  - services              │   │
│  └─────────────┘     └─────────────────┘     └──────────────────────────┘   │
│         │                                                                    │
│         ▼                                                                    │
│  ┌─────────────┐     ┌─────────────────┐     ┌──────────────────────────┐   │
│  │ DID Registry│────▶│ Credential      │────▶│ Verifiable Presentation  │   │
│  │  (CRUD ops) │     │ Issuer          │     │  (bundle for verifier)   │   │
│  └─────────────┘     └─────────────────┘     └──────────────────────────┘   │
│                             │                                                │
│                             ▼                                                │
│                      ┌─────────────────┐                                     │
│                      │ Credential      │                                     │
│                      │ Status List     │                                     │
│                      │ (revocation)    │                                     │
│                      └─────────────────┘                                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Core Concepts

### What is a DID?

A Decentralized Identifier (DID) is a globally unique identifier that does not require a centralized registration authority. In Blockit, DIDs are derived from Ed25519 public keys:

```
did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069
└─┬─┘└──┬──┘└────────────────────────┬───────────────────────────────────────┘
scheme  method                  method-specific-id (SHA256 of public key)
```

### What is a Verifiable Credential?

A Verifiable Credential is a tamper-proof digital certificate issued by an authority (issuer) to a subject, containing claims that can be cryptographically verified. In robotics:

- **Robot Authorization**: "This robot is authorized to operate in the fleet"
- **Zone Access**: "This robot can access warehouse zone A"
- **Sensor Calibration**: "This robot's LIDAR was calibrated on 2024-01-15"
- **Swarm Membership**: "This robot is a member of swarm-alpha with role: leader"

## DID (Decentralized Identifier)

### Header: `<blockit/identity/did.hpp>`

```cpp
namespace blockit {

class DID {
public:
    // Create DID from Ed25519 key (derives method-specific-id from public key hash)
    static DID fromKey(const Key& key);

    // Parse DID from string
    static dp::Result<DID, dp::Error> parse(const std::string& did_string);

    // Get components
    std::string getScheme() const;      // Always "did"
    std::string getMethod() const;      // Always "blockit"
    std::string getMethodSpecificId() const;  // SHA256 hash of public key

    // Serialize to string
    std::string toString() const;       // "did:blockit:abc123..."

    // Create DID URL with fragment
    std::string withFragment(const std::string& fragment) const;
    // "did:blockit:abc123#key-1"

    // Comparison
    bool operator==(const DID& other) const;
    bool operator!=(const DID& other) const;
};

}
```

### Example Usage

```cpp
#include <blockit/identity/did.hpp>
using namespace blockit;

// Create a DID from a key
auto key = Key::generate().value();
auto did = DID::fromKey(key);

std::cout << "DID: " << did.toString() << "\n";
// did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069

std::cout << "Method: " << did.getMethod() << "\n";  // blockit
std::cout << "ID: " << did.getMethodSpecificId() << "\n";  // 7f83b1657ff1...

// Create DID URL with fragment
std::string key_url = did.withFragment("key-1");
// did:blockit:7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069#key-1

// Parse existing DID
auto parsed = DID::parse("did:blockit:abc123").value();
```

## DID Document

### Header: `<blockit/identity/did_document.hpp>`

A DID Document contains the public keys and service endpoints associated with a DID.

```cpp
namespace blockit {

class DIDDocument {
public:
    // Create document with controller key
    static DIDDocument create(const DID& did, const Key& controller_key);

    // Get DID
    DID getId() const;

    // Verification methods (public keys)
    std::vector<VerificationMethod> getVerificationMethods() const;
    dp::Result<VerificationMethod, dp::Error> getVerificationMethod(const std::string& id) const;
    void addVerificationMethod(const VerificationMethod& vm);
    void removeVerificationMethod(const std::string& id);

    // Services (endpoints)
    std::vector<Service> getServices() const;
    dp::Result<Service, dp::Error> getService(const std::string& id) const;
    void addService(const Service& service);
    void removeService(const std::string& id);

    // Authentication methods (references to verification methods)
    std::vector<std::string> getAuthentication() const;
    void addAuthentication(const std::string& method_id);

    // Assertion methods (for signing credentials)
    std::vector<std::string> getAssertionMethod() const;
    void addAssertionMethod(const std::string& method_id);

    // Controller (who can update this document)
    std::vector<DID> getController() const;
    void setController(const std::vector<DID>& controllers);

    // Lifecycle
    bool isActive() const;
    void deactivate();

    // Versioning
    int getVersion() const;
    dp::i64 getCreated() const;
    dp::i64 getUpdated() const;

    // Serialization
    dp::ByteBuf serialize() const;
    static dp::Result<DIDDocument, dp::Error> deserialize(const dp::ByteBuf& data);
};

}
```

### Example Usage

```cpp
#include <blockit/identity/did_document.hpp>
using namespace blockit;

// Create document
auto key = Key::generate().value();
auto did = DID::fromKey(key);
auto doc = DIDDocument::create(did, key);

// Document automatically has a verification method
auto vms = doc.getVerificationMethods();
std::cout << "Verification methods: " << vms.size() << "\n";  // 1

// Add a service endpoint
doc.addService(Service(
    did.withFragment("sensors"),
    ServiceType::SensorDataEndpoint,
    "https://robot.example.com/sensors"
));

doc.addService(Service(
    did.withFragment("tasks"),
    ServiceType::TaskQueue,
    "https://robot.example.com/tasks"
));

// Check services
for (const auto& svc : doc.getServices()) {
    std::cout << svc.getId() << " -> " << svc.getServiceEndpoint() << "\n";
}
```

## DID Registry

### Header: `<blockit/identity/did_registry.hpp>`

The DID Registry manages the lifecycle of DIDs (create, resolve, update, deactivate).

```cpp
namespace blockit {

// Operation types for audit trail
enum class DIDOperationType : dp::u8 {
    Create = 0,
    Update = 1,
    Deactivate = 2,
    AddVerificationMethod = 3,
    RemoveVerificationMethod = 4,
    AddService = 5,
    RemoveService = 6,
};

// Operation record (stored in blockchain transactions)
struct DIDOperation {
    DIDOperationType operation_type;
    std::string did;
    dp::i64 timestamp;
    std::vector<uint8_t> document_data;  // Serialized document (for Create/Update)

    DIDOperationType getOperationType() const;
    std::string getDID() const;

    // Serialize for blockchain
    std::vector<uint8_t> toBytes() const;
    static dp::Result<DIDOperation, dp::Error> fromBytes(const std::vector<uint8_t>& data);
};

class DIDRegistry {
public:
    DIDRegistry() = default;

    // Create a new DID (returns document and operation for blockchain)
    dp::Result<std::pair<DIDDocument, DIDOperation>, dp::Error> create(const Key& key);

    // Resolve DID to document
    dp::Result<DIDDocument, dp::Error> resolve(const DID& did) const;

    // Update document (must be signed by controller)
    dp::Result<DIDOperation, dp::Error> update(const DID& did,
                                                const DIDDocument& new_doc,
                                                const Key& controller_key);

    // Deactivate DID (permanent)
    dp::Result<DIDOperation, dp::Error> deactivate(const DID& did,
                                                    const Key& controller_key);

    // Check if DID exists
    bool exists(const DID& did) const;

    // Check if DID is active
    bool isActive(const DID& did) const;

    // Get document history (all versions)
    std::vector<DIDDocument> getHistory(const DID& did) const;

    // Apply operation from blockchain (for syncing)
    dp::Result<void, dp::Error> applyOperation(const DIDOperation& op);

    // Get all DIDs
    std::vector<DID> getAllDIDs() const;

    // Get count
    size_t size() const;
};

}
```

### Example Usage

```cpp
#include <blockit/identity/did_registry.hpp>
using namespace blockit;

DIDRegistry registry;

// Create a DID
auto key = Key::generate().value();
auto [doc, create_op] = registry.create(key).value();
std::cout << "Created: " << doc.getId().toString() << "\n";

// Resolve DID
auto resolved = registry.resolve(doc.getId()).value();
std::cout << "Active: " << (resolved.isActive() ? "yes" : "no") << "\n";

// Update document (add service)
auto updated_doc = resolved;
updated_doc.addService(Service(
    doc.getId().withFragment("api"),
    ServiceType::APIEndpoint,
    "https://robot.local/api"
));
auto update_op = registry.update(doc.getId(), updated_doc, key).value();

// Check history
auto history = registry.getHistory(doc.getId());
std::cout << "Document versions: " << history.size() << "\n";  // 2

// Deactivate
auto deactivate_op = registry.deactivate(doc.getId(), key).value();
std::cout << "Still active: " << registry.isActive(doc.getId()) << "\n";  // false
```

## Verification Method

### Header: `<blockit/identity/verification_method.hpp>`

Represents a cryptographic public key in a DID Document.

```cpp
namespace blockit {

enum class VerificationMethodType : dp::u8 {
    Ed25519VerificationKey2020 = 0,
    JsonWebKey2020 = 1,
    Multikey = 2,
};

class VerificationMethod {
public:
    // Create from Key
    static VerificationMethod fromKey(const DID& controller,
                                       const Key& key,
                                       const std::string& fragment = "key-1");

    // Getters
    std::string getId() const;          // did:blockit:abc#key-1
    DID getController() const;          // did:blockit:abc
    VerificationMethodType getType() const;
    std::string getTypeString() const;  // "Ed25519VerificationKey2020"
    std::vector<uint8_t> getPublicKeyBytes() const;

    // Convert back to Key (for verification)
    dp::Result<Key, dp::Error> toKey() const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static dp::Result<VerificationMethod, dp::Error> deserialize(
        const std::vector<uint8_t>& data);
};

}
```

### Example Usage

```cpp
#include <blockit/identity/verification_method.hpp>
using namespace blockit;

auto key = Key::generate().value();
auto did = DID::fromKey(key);

// Create verification method
auto vm = VerificationMethod::fromKey(did, key, "key-1");

std::cout << "ID: " << vm.getId() << "\n";
// did:blockit:7f83b1657ff1...#key-1

std::cout << "Type: " << vm.getTypeString() << "\n";
// Ed25519VerificationKey2020

// Convert back to key for verification
auto recovered_key = vm.toKey().value();
```

## Service Endpoints

### Header: `<blockit/identity/service.hpp>`

Service endpoints in DID Documents define how to interact with the DID subject.

```cpp
namespace blockit {

enum class ServiceType : dp::u8 {
    DIDCommMessaging = 0,      // DIDComm v2 messaging
    LinkedDomains = 1,         // Web domain verification

    // Robotics-specific service types
    SensorDataEndpoint = 10,   // Real-time sensor data stream
    CommandEndpoint = 11,      // Robot command interface
    StatusEndpoint = 12,       // Robot status polling
    TaskQueue = 13,            // Task assignment queue
    TelemetryStream = 14,      // Telemetry data stream
    DiagnosticsEndpoint = 15,  // Diagnostics and health
    FirmwareUpdate = 16,       // OTA update endpoint
    APIEndpoint = 17,          // General REST API

    Custom = 255,              // Custom service type
};

class Service {
public:
    Service(const std::string& id, ServiceType type, const std::string& endpoint);

    std::string getId() const;
    ServiceType getType() const;
    std::string getTypeString() const;
    std::string getServiceEndpoint() const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static dp::Result<Service, dp::Error> deserialize(const std::vector<uint8_t>& data);
};

}
```

### Example Usage

```cpp
#include <blockit/identity/service.hpp>
using namespace blockit;

auto did = DID::fromKey(Key::generate().value());

// Create robotics services
Service sensor_svc(
    did.withFragment("sensors"),
    ServiceType::SensorDataEndpoint,
    "wss://robot.local:8080/sensors"
);

Service cmd_svc(
    did.withFragment("commands"),
    ServiceType::CommandEndpoint,
    "https://robot.local/api/commands"
);

Service status_svc(
    did.withFragment("status"),
    ServiceType::StatusEndpoint,
    "https://robot.local/api/status"
);

std::cout << sensor_svc.getTypeString() << "\n";  // SensorDataEndpoint
```

## Verifiable Credentials

### Header: `<blockit/identity/verifiable_credential.hpp>`

W3C Verifiable Credentials contain claims about a subject, signed by an issuer.

```cpp
namespace blockit {

enum class CredentialType : dp::u8 {
    VerifiableCredential = 0,
    RobotAuthorization = 1,
    SwarmMembership = 2,
    ZoneAccess = 3,
    SensorCalibration = 4,
    CapabilityGrant = 5,
    MaintenanceRecord = 6,
    Custom = 255,
};

enum class CredentialStatus : dp::u8 {
    Active = 0,
    Suspended = 1,
    Revoked = 2,
    Expired = 3,
};

class VerifiableCredential {
public:
    // Create credential
    static VerifiableCredential create(
        const DID& issuer,
        const DID& subject,
        CredentialType type,
        const std::map<std::string, std::string>& claims
    );

    // Getters
    std::string getId() const;          // urn:uuid:...
    DID getIssuer() const;
    DID getSubject() const;
    CredentialType getType() const;
    std::vector<std::string> getTypeStrings() const;

    // Claims
    dp::Result<std::string, dp::Error> getClaim(const std::string& key) const;
    std::map<std::string, std::string> getAllClaims() const;
    void setClaim(const std::string& key, const std::string& value);

    // Validity period
    dp::i64 getIssuanceDate() const;
    dp::i64 getExpirationDate() const;
    void setExpirationDate(dp::i64 timestamp);
    bool isExpired() const;

    // Proof
    void setProof(const std::vector<uint8_t>& signature,
                  const std::string& verification_method);
    std::vector<uint8_t> getProofValue() const;
    std::string getProofVerificationMethod() const;

    // Sign/Verify
    dp::Result<void, dp::Error> sign(const Key& issuer_key,
                                      const std::string& verification_method);
    dp::Result<bool, dp::Error> verify(const Key& issuer_key) const;

    // Serialization
    dp::ByteBuf serialize() const;
    static dp::Result<VerifiableCredential, dp::Error> deserialize(
        const dp::ByteBuf& data);
};

}
```

### Example Usage

```cpp
#include <blockit/identity/verifiable_credential.hpp>
using namespace blockit;

// Create issuer (fleet manager) and subject (robot)
auto issuer_key = Key::generate().value();
auto issuer_did = DID::fromKey(issuer_key);

auto robot_key = Key::generate().value();
auto robot_did = DID::fromKey(robot_key);

// Issue robot authorization credential
auto auth_cred = VerifiableCredential::create(
    issuer_did,
    robot_did,
    CredentialType::RobotAuthorization,
    {
        {"robot_id", "DELIVERY_BOT_001"},
        {"capabilities", "navigation,pickup,delivery,charging"},
        {"fleet", "warehouse-fleet-1"},
    }
);

// Set expiration (24 hours)
auto expiration = std::chrono::system_clock::now() + std::chrono::hours(24);
auth_cred.setExpirationDate(
    std::chrono::duration_cast<std::chrono::milliseconds>(
        expiration.time_since_epoch()
    ).count()
);

// Sign the credential
auth_cred.sign(issuer_key, issuer_did.withFragment("key-1")).value();

// Access claims
std::cout << "Robot ID: " << auth_cred.getClaim("robot_id").value() << "\n";
std::cout << "Capabilities: " << auth_cred.getClaim("capabilities").value() << "\n";
std::cout << "Expired: " << (auth_cred.isExpired() ? "yes" : "no") << "\n";

// Verify the credential
auto verify_result = auth_cred.verify(issuer_key);
if (verify_result.is_ok() && verify_result.value()) {
    std::cout << "Credential verified!\n";
}
```

### Common Credential Types for Robotics

```cpp
// Robot Authorization - grants robot permission to operate
VerifiableCredential::create(issuer, robot_did, CredentialType::RobotAuthorization, {
    {"robot_id", "PATROL_BOT_003"},
    {"capabilities", "patrol,report,emergency_stop"},
    {"max_speed", "1.5"},
    {"operational_area", "building_a"},
});

// Zone Access - time-limited area access
VerifiableCredential::create(issuer, robot_did, CredentialType::ZoneAccess, {
    {"zone_id", "warehouse_zone_a"},
    {"access_level", "full"},
    {"valid_hours", "06:00-22:00"},
});

// Sensor Calibration - proves sensor accuracy
VerifiableCredential::create(issuer, robot_did, CredentialType::SensorCalibration, {
    {"sensor_type", "lidar"},
    {"sensor_id", "LIDAR_001"},
    {"calibration_date", "2024-01-15"},
    {"accuracy", "0.01"},
    {"technician", "TECH_042"},
});

// Swarm Membership - proves membership in a swarm
VerifiableCredential::create(issuer, robot_did, CredentialType::SwarmMembership, {
    {"swarm_id", "swarm-alpha"},
    {"role", "leader"},
    {"joined_at", "2024-01-10T08:00:00Z"},
});

// Capability Grant - grants specific capabilities
VerifiableCredential::create(issuer, robot_did, CredentialType::CapabilityGrant, {
    {"capability", "emergency_override"},
    {"resource", "/system/emergency/*"},
    {"granted_by", "ADMIN_001"},
});
```

## Verifiable Presentations

### Header: `<blockit/identity/verifiable_presentation.hpp>`

A Verifiable Presentation bundles multiple credentials for presentation to a verifier.

```cpp
namespace blockit {

class VerifiablePresentation {
public:
    // Create empty presentation
    static VerifiablePresentation create(const DID& holder);

    // Holder (the entity presenting)
    DID getHolder() const;

    // Credentials
    void addCredential(const VerifiableCredential& credential);
    std::vector<VerifiableCredential> getCredentials() const;
    size_t getCredentialCount() const;

    // Challenge-response authentication
    void setChallenge(const std::string& challenge);
    std::string getChallenge() const;
    bool hasChallenge() const;

    // Domain binding (restrict to specific verifier)
    void setDomain(const std::string& domain);
    std::string getDomain() const;

    // Sign/Verify (proves holder controls the DID)
    dp::Result<void, dp::Error> sign(const Key& holder_key,
                                      const std::string& verification_method);
    dp::Result<bool, dp::Error> verifyWithKey(const Key& holder_key) const;

    // Timestamps
    dp::i64 getCreated() const;

    // Serialization
    dp::ByteBuf serialize() const;
    static dp::Result<VerifiablePresentation, dp::Error> deserialize(
        const dp::ByteBuf& data);
};

}
```

### Example Usage

```cpp
#include <blockit/identity/verifiable_presentation.hpp>
using namespace blockit;

auto robot_key = Key::generate().value();
auto robot_did = DID::fromKey(robot_key);

// Create presentation
auto presentation = VerifiablePresentation::create(robot_did);

// Add credentials
presentation.addCredential(auth_credential);
presentation.addCredential(zone_access_credential);
presentation.addCredential(calibration_credential);

// Set challenge (from verifier)
presentation.setChallenge("verify-request-" + std::to_string(std::time(nullptr)));

// Optionally bind to specific domain
presentation.setDomain("checkpoint-alpha.warehouse.local");

// Sign the presentation
presentation.sign(robot_key, robot_did.withFragment("key-1")).value();

// Verifier checks the presentation
auto vp_verify = presentation.verifyWithKey(robot_key);
if (vp_verify.is_ok() && vp_verify.value()) {
    std::cout << "Presentation signature valid\n";

    // Verify each credential individually
    for (const auto& cred : presentation.getCredentials()) {
        // Issuer's public key needed for each credential
        auto cred_verify = cred.verify(issuer_key);
        std::cout << "Credential " << cred.getId() << ": "
                  << (cred_verify.is_ok() && cred_verify.value() ? "VALID" : "INVALID")
                  << "\n";
    }
}
```

## Credential Issuer

### Header: `<blockit/identity/credential_issuer.hpp>`

High-level API for issuing common credential types.

```cpp
namespace blockit {

class CredentialIssuer {
public:
    CredentialIssuer(const DID& issuer_did, const Key& issuer_key);

    // Issue robot authorization
    dp::Result<VerifiableCredential, dp::Error> issueRobotAuthorization(
        const DID& robot_did,
        const std::string& robot_id,
        const std::vector<std::string>& capabilities,
        std::chrono::duration<int64_t> validity = std::chrono::hours(24 * 365)
    );

    // Issue swarm membership
    dp::Result<VerifiableCredential, dp::Error> issueSwarmMembership(
        const DID& member_did,
        const std::string& swarm_id,
        const std::string& role,  // "leader", "member", "observer"
        std::chrono::duration<int64_t> validity = std::chrono::hours(24 * 365)
    );

    // Issue zone access
    dp::Result<VerifiableCredential, dp::Error> issueZoneAccess(
        const DID& robot_did,
        const std::string& zone_id,
        const std::string& access_level,  // "read", "write", "full"
        std::chrono::duration<int64_t> validity = std::chrono::hours(8)
    );

    // Issue sensor calibration certificate
    dp::Result<VerifiableCredential, dp::Error> issueSensorCalibration(
        const DID& robot_did,
        const std::string& sensor_type,
        const std::string& sensor_id,
        const std::map<std::string, std::string>& calibration_data,
        std::chrono::duration<int64_t> validity = std::chrono::hours(24 * 30)
    );

    // Issue capability grant
    dp::Result<VerifiableCredential, dp::Error> issueCapabilityGrant(
        const DID& subject_did,
        const std::string& capability,
        const std::string& resource,
        std::chrono::duration<int64_t> validity = std::chrono::hours(24)
    );

    // Issue custom credential
    dp::Result<VerifiableCredential, dp::Error> issueCustomCredential(
        const DID& subject_did,
        CredentialType type,
        const std::map<std::string, std::string>& claims,
        std::chrono::duration<int64_t> validity = std::chrono::hours(24 * 365)
    );

    // Get issuer DID
    DID getIssuerDID() const;
};

}
```

### Example Usage

```cpp
#include <blockit/identity/credential_issuer.hpp>
using namespace blockit;

// Create credential issuer (fleet manager)
auto fleet_mgr_key = Key::generate().value();
auto fleet_mgr_did = DID::fromKey(fleet_mgr_key);
CredentialIssuer issuer(fleet_mgr_did, fleet_mgr_key);

// Robot to credential
auto robot_key = Key::generate().value();
auto robot_did = DID::fromKey(robot_key);

// Issue various credentials
auto auth = issuer.issueRobotAuthorization(
    robot_did,
    "DELIVERY_BOT_001",
    {"navigation", "pickup", "delivery"}
).value();

auto membership = issuer.issueSwarmMembership(
    robot_did,
    "warehouse-swarm-1",
    "member"
).value();

auto zone = issuer.issueZoneAccess(
    robot_did,
    "loading_dock_a",
    "full",
    std::chrono::hours(8)  // 8-hour shift
).value();

auto calibration = issuer.issueSensorCalibration(
    robot_did,
    "lidar",
    "LIDAR_001",
    {{"accuracy", "0.01"}, {"range", "30m"}}
).value();
```

## Credential Status

### Header: `<blockit/identity/credential_status.hpp>`

Tracks credential status for revocation and suspension.

```cpp
namespace blockit {

enum class CredentialOperationType : dp::u8 {
    Issue = 0,
    Revoke = 1,
    Suspend = 2,
    Unsuspend = 3,
};

// Operation record for blockchain storage
struct CredentialOperation {
    CredentialOperationType operation_type;
    std::string credential_id;
    std::string issuer_did;
    dp::i64 timestamp;
    std::string reason;

    // For Issue operations, includes serialized credential
    std::vector<uint8_t> credential_data;

    static CredentialOperation createIssue(const VerifiableCredential& credential);
    static CredentialOperation createRevoke(const std::string& cred_id,
                                             const std::string& issuer,
                                             const std::string& reason = "");
    static CredentialOperation createSuspend(const std::string& cred_id,
                                              const std::string& issuer,
                                              const std::string& reason = "");
    static CredentialOperation createUnsuspend(const std::string& cred_id,
                                                const std::string& issuer);
};

class CredentialStatusList {
public:
    CredentialStatusList() = default;

    // Record operations
    void recordIssue(const std::string& credential_id, const std::string& issuer_did);
    dp::Result<void, dp::Error> recordRevoke(const std::string& credential_id,
                                              const std::string& issuer_did,
                                              const std::string& reason = "");
    dp::Result<void, dp::Error> recordSuspend(const std::string& credential_id,
                                               const std::string& issuer_did,
                                               const std::string& reason = "");
    dp::Result<void, dp::Error> recordUnsuspend(const std::string& credential_id,
                                                 const std::string& issuer_did);

    // Apply operation from blockchain
    dp::Result<void, dp::Error> applyOperation(const CredentialOperation& op);

    // Query status
    bool exists(const std::string& credential_id) const;
    bool isActive(const std::string& credential_id) const;
    bool isRevoked(const std::string& credential_id) const;
    bool isSuspended(const std::string& credential_id) const;
    dp::Result<CredentialStatus, dp::Error> getStatus(const std::string& credential_id) const;

    // Query by issuer
    std::vector<std::string> getCredentialsByIssuer(const std::string& issuer_did) const;
    std::vector<std::string> getRevokedCredentials() const;
    std::vector<std::string> getSuspendedCredentials() const;

    // Count
    size_t size() const;
};

}
```

### Example Usage

```cpp
#include <blockit/identity/credential_status.hpp>
using namespace blockit;

CredentialStatusList status_list;

// Record credential issuance
status_list.recordIssue(credential.getId(), issuer_did.toString());

// Check status
if (status_list.isActive(credential.getId())) {
    std::cout << "Credential is active\n";
}

// Suspend credential (temporary)
status_list.recordSuspend(
    credential.getId(),
    issuer_did.toString(),
    "Robot under maintenance"
).value();

// Unsuspend when back online
status_list.recordUnsuspend(
    credential.getId(),
    issuer_did.toString()
).value();

// Revoke permanently
status_list.recordRevoke(
    credential.getId(),
    issuer_did.toString(),
    "Robot decommissioned"
).value();

// Cannot unsuspend after revocation
auto result = status_list.recordUnsuspend(credential.getId(), issuer_did.toString());
// result.is_err() == true
```

## Integration with Blockit

### Header: `<blockit/storage/blockit_store.hpp>`

The `Blockit<T>` class provides integrated DID support.

```cpp
// Initialize with DID support
Blockit<RobotTask> blockit;
blockit.initialize("./data", "chain", "genesis", genesis_data, crypto);
blockit.initializeDID();

// DID operations
auto [doc, op] = blockit.createDID(key).value();
auto resolved = blockit.resolveDID(did).value();

// Robot identity (creates DID + authorization credential)
auto [robot_doc, auth_cred] = blockit.createRobotIdentity(
    robot_key,
    "ROBOT_001",
    {"cap1", "cap2"},
    issuer_key
).value();

// Issue credentials
auto cred = blockit.issueCredential(
    issuer_key,
    subject_did,
    CredentialType::ZoneAccess,
    {{"zone", "a"}, {"level", "full"}},
    std::chrono::hours(8)
).value();

// Access registry and status list
auto registry = blockit.getDIDRegistry();
auto status_list = blockit.getCredentialStatusList();
```

## Robotics Use Cases

### 1. Robot Fleet Onboarding

```cpp
// Fleet manager creates its identity
auto fleet_mgr_key = Key::generate().value();
auto [fleet_doc, _] = blockit.createDID(fleet_mgr_key).value();

// New robot joins fleet
auto robot_key = Key::generate().value();
auto [robot_doc, auth_cred] = blockit.createRobotIdentity(
    robot_key,
    "DELIVERY_BOT_042",
    {"navigation", "pickup", "delivery", "charging"},
    fleet_mgr_key
).value();

// Robot now has:
// - A DID (did:blockit:abc123...)
// - A DID Document (with public key and services)
// - An authorization credential (signed by fleet manager)
```

### 2. Zone Access Control

```cpp
// Issue shift-based zone access
auto zone_cred = blockit.issueCredential(
    fleet_mgr_key,
    robot_did,
    CredentialType::ZoneAccess,
    {{"zone_id", "warehouse_a"}, {"access_level", "full"}},
    std::chrono::hours(8)
).value();

// Robot presents credentials at checkpoint
auto presentation = VerifiablePresentation::create(robot_did);
presentation.addCredential(auth_cred);
presentation.addCredential(zone_cred);
presentation.setChallenge(checkpoint_challenge);
presentation.sign(robot_key, robot_did.withFragment("key-1"));

// Checkpoint verifies:
// 1. Presentation signature (proves robot identity)
// 2. Auth credential (robot is authorized)
// 3. Zone credential (robot has zone access)
// 4. Credential status (not revoked)

// End of shift - revoke access
status_list->recordRevoke(zone_cred.getId(), fleet_did.toString(), "Shift ended");
```

### 3. Multi-Authority Credential Chain

```cpp
// Different authorities issue different credentials

// Fleet manager: robot authorization
auto auth_cred = fleet_issuer.issueRobotAuthorization(robot_did, "BOT_001", caps);

// Safety authority: safety certification
auto safety_cred = safety_issuer.issueCustomCredential(
    robot_did,
    CredentialType::Custom,
    {{"cert_type", "safety"}, {"rating", "A"}}
);

// Maintenance: calibration certificate
auto cal_cred = maintenance_issuer.issueSensorCalibration(
    robot_did,
    "lidar",
    "LIDAR_001",
    {{"accuracy", "0.01"}}
);

// Robot bundles all for verification
presentation.addCredential(auth_cred);      // Verify with fleet_mgr_key
presentation.addCredential(safety_cred);    // Verify with safety_key
presentation.addCredential(cal_cred);       // Verify with maintenance_key
```

### 4. Swarm Coordination

```cpp
// Coordinator issues membership credentials
for (auto& [member_key, member_did] : swarm_members) {
    std::string role = (first_member ? "leader" : "member");
    auto membership = coordinator.issueSwarmMembership(
        member_did,
        "swarm-alpha",
        role
    ).value();

    status_list->recordIssue(membership.getId(), coordinator_did.toString());
}

// Members can prove swarm membership to each other
// by presenting their membership credentials
```

### 5. Challenge-Response Authentication

```cpp
// Verifier generates challenge
std::vector<uint8_t> challenge = generateRandomBytes(32);

// Robot signs challenge with its DID key
auto signature = robot_key.sign(challenge).value();

// Verifier resolves robot's DID
auto doc = registry.resolve(robot_did).value();
auto vm = doc.getVerificationMethods()[0];
auto resolved_key = vm.toKey().value();

// Verify signature
auto valid = resolved_key.verify(challenge, signature).value();
// valid == true means robot controls the DID
```
