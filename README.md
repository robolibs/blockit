<img align="right" width="26%" src="./misc/logo.png">

# Blockit

A modern C++20 header-only blockchain library with Proof-of-Authority consensus for building trusted robot swarms.

## Development Status

See [TODO.md](./TODO.md) for the complete development plan and current progress.

## Overview

Blockit is a comprehensive blockchain framework designed for robot swarms requiring decentralized trust without central coordination. Built on Proof-of-Authority (PoA) consensus, it enables autonomous robots to establish trust, share verified state, and coordinate actions through cryptographically secured consensus.

The library provides a complete blockchain implementation with generic templated types, allowing robot state, sensor data, commands, and any data structure to be stored on-chain. It combines cryptographic security through Ed25519 signatures with practical features like content anchoring, swarm member authorization, and persistent file-based storage. The unified `Blockit<T>` API manages blockchain operations and storage atomically, making it straightforward to build production swarm coordination systems.

Key design principles include type safety through C++20 templates, thread-safe concurrent access, zero-copy serialization with datapod, and modular architecture that allows using individual components independently. Whether you need simple state synchronization or full-featured swarm consensus, Blockit provides the building blocks for robots that trust each other.

### Architecture Diagrams

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              BLOCKIT LIBRARY                                │
├────────────────────────────────┬────────────────────────────────────────────┤
│           LEDGER MODULE        │            STORAGE MODULE                  │
│                                │                                            │
│  ┌──────────┐   ┌──────────┐   │   ┌────────────────┐   ┌───────────────┐   │
│  │ Chain<T> │◄──│ Block<T> │   │   │ BlockitStore<T>│◄──│  FileStore    │   │
│  └────┬─────┘   └────┬─────┘   │   └───────┬────────┘   └───────────────┘   │
│       │              │         │           │                                │
│  ┌────▼─────┐   ┌────▼─────┐   │   ┌───────▼────────┐                       │
│  │Transaction│   │ Merkle   │   │   │ Content Anchor │                      │
│  │   <T>    │   │  Tree    │   │   │   Management   │                       │
│  └──────────┘   └──────────┘   │   └────────────────┘                       │
│                                │                                            │
├────────────────────────────────┼────────────────────────────────────────────┤
│      CONSENSUS MODULE          │         IDENTITY MODULE                    │
│                                │                                            │
│  ┌──────────────────────────┐  │   ┌──────────┐   ┌──────────────────────┐  │
│  │     PoA Consensus        │  │   │   Key    │◄──│    Validator         │  │
│  │  ┌────────┐ ┌─────────┐  │  │   │ (Ed25519)│   │  (Identity + State)  │  │
│  │  │Proposal│ │ Quorum  │  │  │   └────┬─────┘   └──────────────────────┘  │
│  │  │Manager │ │ Engine  │  │  │        │                                   │
│  │  └────────┘ └─────────┘  │  │   ┌────▼─────┐                             │
│  └──────────────────────────┘  │   │  Signer  │                             │
│                                │   │ (Crypto) │                             │
│  ┌──────────────────────────┐  │   └──────────┘                             │
│  │    Authenticator         │  │                                            │
│  │  (Access Control)        │  │                                            │
│  └──────────────────────────┘  │                                            │
└────────────────────────────────┴────────────────────────────────────────────┘
                    │                           │
                    └───────────────────────────┘
                                │
                        ┌───────▼────────┐
                        │  Robot Swarm   │
                        └────────────────┘
```

**Data Flow for Block Finalization:**

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Transaction │────▶│   Block     │────▶│   PoA       │────▶│   Chain     │
│  Creation   │     │  Proposal   │     │  Consensus  │     │  Addition   │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
       │                   │                   │                   │
       ▼                   ▼                   ▼                   ▼
   Sign with          Calculate          Collect N           Update Merkle
   Ed25519 Key        Block Hash         Signatures          Root & Store
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

make config   # Configure build
make build    # Build library
make test     # Run tests
```

## Usage

### 1. Robot Joins a Trusted Swarm

A new robot generates its identity key and requests to join the swarm. Existing trusted robots vote to accept or reject the newcomer.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

// Data structure for membership requests
struct MembershipRequest {
    std::string robot_id;
    std::string public_key;
    std::string capabilities;    // "scout", "carrier", "leader"
    uint64_t timestamp;

    auto toBytes() const -> std::vector<uint8_t> {
        return dp::serialize(*this);
    }
};

// Initialize the swarm's trust ledger
MembershipRequest genesis{"SWARM_FOUNDER", "key_0", "leader", 0};
Chain<MembershipRequest> trust_chain("swarm_trust", "genesis", genesis);

// New robot generates its identity
auto new_robot_key = Key::generate().value();
std::string new_robot_id = "ROBOT_007";

// Robot submits join request to the swarm
MembershipRequest join_request{
    new_robot_id,
    new_robot_key.getId(),
    "scout",
    std::chrono::system_clock::now().time_since_epoch().count()
};

Transaction<MembershipRequest> tx("join_007", join_request, 100);
tx.signTransaction(new_robot_key);

// Existing swarm members validate and add to chain
Block<MembershipRequest> block({tx});
trust_chain.addBlock(block);

// Now ROBOT_007 is part of the trusted swarm!
std::cout << "Robot " << new_robot_id << " joined the swarm\n";
```

### 2. Swarm Task Distribution

Robots publish tasks to the shared ledger. Any swarm member can claim and execute tasks, with full auditability.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

struct SwarmTask {
    std::string task_id;
    std::string task_type;       // "patrol", "deliver", "inspect", "charge"
    double target_x, target_y;
    std::string assigned_to;     // Empty = unclaimed
    std::string status;          // "pending", "claimed", "completed", "failed"
    uint64_t priority;
    uint64_t deadline;

    auto toBytes() const -> std::vector<uint8_t> {
        return dp::serialize(*this);
    }
};

// Initialize task ledger
SwarmTask genesis_task{"TASK_INIT", "system", 0, 0, "system", "completed", 0, 0};
Chain<SwarmTask> task_chain("swarm_tasks", "genesis", genesis_task);

// Leader robot publishes a new patrol task
auto leader_key = Key::generate().value();
SwarmTask patrol_task{
    "TASK_001",
    "patrol",
    150.0, 200.0,                 // Target coordinates
    "",                           // Unclaimed
    "pending",
    10,                           // High priority
    std::chrono::system_clock::now().time_since_epoch().count() + 3600000
};

Transaction<SwarmTask> publish_tx("pub_001", patrol_task, patrol_task.priority);
publish_tx.signTransaction(leader_key);

// Scout robot claims the task
auto scout_key = Key::generate().value();
patrol_task.assigned_to = "SCOUT_003";
patrol_task.status = "claimed";

Transaction<SwarmTask> claim_tx("claim_001", patrol_task, patrol_task.priority);
claim_tx.signTransaction(scout_key);

// Add both transactions to chain
Block<SwarmTask> block({publish_tx, claim_tx});
task_chain.addBlock(block);

// Scout completes the task
patrol_task.status = "completed";
Transaction<SwarmTask> complete_tx("done_001", patrol_task, patrol_task.priority);
complete_tx.signTransaction(scout_key);

Block<SwarmTask> completion_block({complete_tx});
task_chain.addBlock(completion_block);

std::cout << "Task " << patrol_task.task_id << " completed by " << patrol_task.assigned_to << "\n";
```

### 3. Consensus for Critical Swarm Decisions

Important decisions (new member approval, task priority changes, emergency protocols) require multiple leader robots to agree.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

// Configure consensus: 2-of-3 leaders must agree
PoAConfig config;
config.initial_required_signatures = 2;
config.signature_timeout_ms = 10000;  // 10 second timeout for robot networks

PoAConsensus swarm_consensus(config);

// Register the 3 leader robots
auto leader_alpha = Key::generate().value();
auto leader_beta = Key::generate().value();
auto leader_gamma = Key::generate().value();

swarm_consensus.addValidator("LEADER_ALPHA", leader_alpha);
swarm_consensus.addValidator("LEADER_BETA", leader_beta);
swarm_consensus.addValidator("LEADER_GAMMA", leader_gamma);

// Critical decision: Accept new robot into swarm
std::string decision = "ACCEPT_ROBOT_007_INTO_SWARM";
auto decision_hash = std::vector<uint8_t>(decision.begin(), decision.end());

// Leader Alpha proposes
auto proposal_id = swarm_consensus.createProposal(decision, "LEADER_ALPHA");

// Leaders sign the decision
auto sig_alpha = leader_alpha.sign(decision_hash).value();
swarm_consensus.addSignature(proposal_id, leader_alpha.getId(), sig_alpha);

auto sig_beta = leader_beta.sign(decision_hash).value();
bool consensus_reached = swarm_consensus.addSignature(proposal_id, leader_beta.getId(), sig_beta);

if (consensus_reached) {
    std::cout << "Consensus reached! Robot 007 is now trusted.\n";
    auto final_sigs = swarm_consensus.getFinalizedSignatures(proposal_id);
    // Record decision on-chain with all signatures as proof
}
```

### 4. Robot State Broadcasting

Every robot periodically broadcasts its state. Other robots can verify and trust this information because it's cryptographically signed.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

struct RobotState {
    std::string robot_id;
    double x, y, z;              // Position
    double vx, vy, vz;           // Velocity
    double battery_percent;
    std::string current_task;
    uint64_t timestamp;
    std::vector<std::string> nearby_robots;

    auto toBytes() const -> std::vector<uint8_t> {
        return dp::serialize(*this);
    }
};

// Each robot maintains the shared state ledger
RobotState genesis{"SWARM_ORIGIN", 0, 0, 0, 0, 0, 0, 100, "init", 0, {}};
Chain<RobotState> state_chain("swarm_state", "genesis", genesis);

// Robot broadcasts its current state
auto robot_key = Key::generate().value();
RobotState my_state{
    "ROBOT_042",
    125.5, 340.2, 0.0,           // Position
    1.2, 0.5, 0.0,               // Velocity
    78.5,                        // Battery
    "patrolling_sector_7",
    std::chrono::system_clock::now().time_since_epoch().count(),
    {"ROBOT_041", "ROBOT_043"}   // Nearby robots detected
};

Transaction<RobotState> state_tx("state_042_1001", my_state, 50);
state_tx.signTransaction(robot_key);

Block<RobotState> block({state_tx});
state_chain.addBlock(block);

// Other robots can now trust ROBOT_042's reported position
// because it's signed with its verified identity key
```

### 5. Swarm Access Control

Control which robots can perform which actions. Scouts can report, carriers can claim delivery tasks, only leaders can approve new members.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

struct SwarmAction {
    std::string action_type;
    std::string data;
    auto toBytes() const -> std::vector<uint8_t> {
        return dp::serialize(*this);
    }
};

SwarmAction genesis{"init", "swarm_created"};
Chain<SwarmAction> swarm_chain("swarm_actions", "genesis", genesis);

// Register robots with their roles
swarm_chain.registerParticipant("LEADER_001", "active");
swarm_chain.registerParticipant("SCOUT_001", "active");
swarm_chain.registerParticipant("CARRIER_001", "active");
swarm_chain.registerParticipant("NEW_ROBOT", "pending");  // Not yet trusted

// Grant role-based capabilities
swarm_chain.grantCapability("LEADER_001", "approve_member");
swarm_chain.grantCapability("LEADER_001", "assign_task");
swarm_chain.grantCapability("LEADER_001", "emergency_shutdown");

swarm_chain.grantCapability("SCOUT_001", "report_state");
swarm_chain.grantCapability("SCOUT_001", "claim_patrol_task");

swarm_chain.grantCapability("CARRIER_001", "report_state");
swarm_chain.grantCapability("CARRIER_001", "claim_delivery_task");

// Check permissions before allowing actions
if (swarm_chain.canParticipantPerform("SCOUT_001", "claim_patrol_task")) {
    std::cout << "Scout can claim patrol tasks\n";
}

if (!swarm_chain.canParticipantPerform("CARRIER_001", "approve_member")) {
    std::cout << "Carriers cannot approve new members - only leaders can\n";
}

// Promote a robot to leader
swarm_chain.grantCapability("SCOUT_001", "approve_member");
swarm_chain.grantCapability("SCOUT_001", "assign_task");

// Remove a compromised robot from the swarm
swarm_chain.updateParticipantState("CARRIER_001", "suspended");
swarm_chain.revokeCapability("CARRIER_001", "claim_delivery_task");
```

### 6. Sensor Data Anchoring

Anchor sensor readings (camera snapshots, LIDAR scans) to the blockchain for tamper-proof evidence.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

struct SensorRecord {
    std::string robot_id;
    std::string sensor_type;     // "camera", "lidar", "thermal"
    std::string data_hash;       // SHA256 of actual sensor data
    double x, y, z;              // Where the reading was taken
    uint64_t timestamp;

    auto toBytes() const -> std::vector<uint8_t> {
        return dp::serialize(*this);
    }
};

// Initialize sensor data ledger with storage
Blockit<SensorRecord> sensor_store;
auto crypto = std::make_shared<Crypto>("swarm_secret_key");

SensorRecord genesis{"SYSTEM", "init", "", 0, 0, 0, 0};
sensor_store.initialize("./swarm_data", "sensor_chain", "genesis", genesis, crypto);

// Robot captures a camera frame and anchors it
std::vector<uint8_t> camera_frame = capture_camera();  // Your camera API
std::string frame_hash = compute_sha256(camera_frame);

SensorRecord record{
    "SCOUT_003",
    "camera",
    frame_hash,
    125.5, 340.2, 1.5,
    std::chrono::system_clock::now().time_since_epoch().count()
};

// Anchor the actual image data to the blockchain
sensor_store.createTransaction(
    "sensor_003_1001",
    record,
    "frame_003_1001",            // Anchor ID
    camera_frame                 // Actual content to anchor
);

// Finalize into a block
auto pending = sensor_store.getPendingTransactions();
sensor_store.addBlock(pending);

// Later: verify the image hasn't been tampered with
bool authentic = sensor_store.verifyContent("frame_003_1001", camera_frame).value();
if (authentic) {
    std::cout << "Sensor data verified - matches blockchain record\n";
}
```

### 7. Persistent Swarm Memory

Save and restore the entire swarm state across restarts. New robots can sync the full history.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;

struct SwarmEvent {
    std::string event_type;
    std::string robot_id;
    std::string details;
    uint64_t timestamp;

    auto toBytes() const -> std::vector<uint8_t> {
        return dp::serialize(*this);
    }
};

SwarmEvent genesis{"swarm_created", "FOUNDER", "Initial swarm formation", 0};
Chain<SwarmEvent> event_chain("swarm_history", "genesis", genesis);

// ... robots join, tasks happen, state changes ...

// Save swarm history to disk
auto save_result = event_chain.saveToFile("/swarm_data/history.chain");
if (save_result.is_err()) {
    std::cerr << "Failed to save: " << save_result.error().message << "\n";
}

// New robot joins and syncs history
Chain<SwarmEvent> synced_chain;
auto load_result = synced_chain.loadFromFile("/swarm_data/history.chain");
if (load_result.is_ok()) {
    std::cout << "Synced " << synced_chain.getBlockCount() << " blocks of swarm history\n";
    // New robot now has full trusted history of the swarm
}
```

### 8. Robot Identity with Expiration

Generate robot identity keys that automatically expire, forcing periodic re-authentication.

```cpp
#include <blockit/blockit.hpp>
using namespace blockit;
using namespace std::chrono;

// Generate key valid for 24 hours (mission duration)
auto mission_end = system_clock::now() + hours(24);
auto robot_key = Key::generateWithExpiration(mission_end).value();

std::cout << "Robot ID: " << robot_key.getId() << "\n";

// Sign a message to prove identity
std::vector<uint8_t> message = {'H', 'E', 'L', 'L', 'O'};
auto signature = robot_key.sign(message).value();

// Other robots verify the signature
bool valid = robot_key.verify(message, signature);

// Store key for persistence across reboots
auto serialized = robot_key.serialize();
// Save serialized to file...

// Restore key
auto restored = Key::deserialize(serialized).value();

// Check if robot's credentials have expired
if (robot_key.isExpired()) {
    std::cout << "Robot key expired - must re-authenticate with swarm\n";
}
```

## Features

- **Swarm Trust** - Robots establish cryptographic trust without central authority
  ```cpp
  Chain<MembershipRequest> trust_chain("swarm_trust", "genesis", genesis);
  ```

- **Proof-of-Authority Consensus** - Multiple leader robots must agree on critical decisions
  - Configurable quorum (e.g., 2-of-3 leaders)
  - Automatic detection of offline robots
  - Prevents rogue robot from making unilateral decisions

- **Ed25519 Robot Identity** - Each robot has a unique cryptographic identity
  ```cpp
  auto robot_key = Key::generate().value();
  auto signature = robot_key.sign(sensor_data).value();
  ```

- **Task Ledger** - Publish, claim, and complete tasks with full audit trail
  - Priority-based task ordering
  - Prevents double-claiming
  - Immutable completion records

- **Sensor Data Anchoring** - Tamper-proof evidence of robot observations
  - Anchor camera frames, LIDAR scans, any sensor data
  - Verify data integrity against blockchain

- **Role-Based Access Control** - Different robots have different permissions
  - Leaders approve members, scouts patrol, carriers deliver
  - Grant/revoke capabilities dynamically

- **State Broadcasting** - Robots share verified position and status
  - Cryptographically signed state updates
  - Other robots can trust reported positions

- **Persistent Memory** - Save/restore swarm history
  - New robots sync full history
  - Survives power cycles and reboots

- **Thread-Safe** - Safe for multi-threaded robot software
  - Multiple readers, single writer
  - Works with ROS, sensor callbacks, etc.

- **Zero-Copy Serialization** - Fast binary format via datapod
  - Efficient for bandwidth-limited robot networks
  - Minimal CPU overhead

- **SIMD Optimizations** - Fast on ARM (robot) and x86 (ground station)
  - NEON on ARM64
  - AVX/AVX2 on x86_64

## License

MIT License - see [LICENSE](./LICENSE) for details.

## Acknowledgments

Made possible thanks to [these amazing projects](./ACKNOWLEDGMENTS.md).
