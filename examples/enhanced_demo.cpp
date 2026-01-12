#include "blockit/blockit.hpp"
#include <iostream>
#include <memory>
#include <vector>

// Command data structure for robot coordination
struct RobotCommand {
    std::string issuer_robot;
    std::string target_robot;
    std::string command;
    int priority_level;

    std::string to_string() const {
        return "RobotCommand{issuer:" + issuer_robot + ",target:" + target_robot + ",cmd:" + command +
               ",priority:" + std::to_string(priority_level) + "}";
    }
};

// Ledger entry for tracking robot states
struct LedgerEntry {
    std::string robot_id;
    std::string operation;
    std::string data;

    std::string to_string() const {
        return "LedgerEntry{robot:" + robot_id + ",op:" + operation + ",data:" + data + "}";
    }
};

void printSeparator(const std::string &title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void demonstrateRobotCoordination() {
    printSeparator("ROBOT COORDINATION DEMONSTRATION");

    auto privateKey = std::make_shared<chain::Crypto>("robot_coordination_key");

    // Create blockchain for robot coordination
    chain::Chain<RobotCommand> robotChain("robot-coordination-chain", "genesis-cmd",
                                          RobotCommand{"system", "all", "initialize", 255}, privateKey);

    // Register robots in the system
    robotChain.registerEntity("robot-001", "idle");
    robotChain.registerEntity("robot-002", "idle");
    robotChain.registerEntity("robot-003", "idle");

    // Grant permissions
    robotChain.grantPermission("robot-001", "MOVE");
    robotChain.grantPermission("robot-001", "PICK");
    robotChain.grantPermission("robot-002", "MOVE");
    robotChain.grantPermission("robot-002", "SCAN");
    robotChain.grantPermission("robot-003", "MOVE");

    std::cout << "Registered 3 robots with different permissions" << std::endl;

    // Create robot commands
    std::vector<chain::Transaction<RobotCommand>> commands;

    // Robot 001 issues a move command
    RobotCommand cmd1{"robot-001", "robot-002", "MOVE_TO(10,20)", 100};
    chain::Transaction<RobotCommand> tx1("cmd-001", cmd1, 100);
    tx1.signTransaction(privateKey);
    commands.push_back(tx1);

    // Robot 002 issues a scan command
    RobotCommand cmd2{"robot-002", "robot-003", "SCAN_AREA(A1)", 150};
    chain::Transaction<RobotCommand> tx2("cmd-002", cmd2, 150);
    tx2.signTransaction(privateKey);
    commands.push_back(tx2);

    // Try to execute duplicate command (should fail)
    RobotCommand cmd3{"robot-001", "robot-002", "MOVE_TO(10,20)", 100};
    chain::Transaction<RobotCommand> tx3("cmd-001", cmd3, 100); // Same UUID - should fail
    tx3.signTransaction(privateKey);
    commands.push_back(tx3);

    // Create block with commands
    chain::Block<RobotCommand> commandBlock(commands);

    std::cout << "\nAttempting to add block with commands..." << std::endl;
    robotChain.addBlock(commandBlock);

    // Update robot states
    robotChain.updateEntityState("robot-001", "moving");
    robotChain.updateEntityState("robot-002", "scanning");

    std::cout << "\nRobot Coordination Summary:" << std::endl;
    robotChain.printChainSummary();
}

void demonstrateLedgerTracking() {
    printSeparator("LEDGER TRACKING DEMONSTRATION");

    auto privateKey = std::make_shared<chain::Crypto>("ledger_key");

    // Create blockchain for ledger tracking
    chain::Chain<LedgerEntry> ledgerChain("ledger-chain", "genesis-entry",
                                          LedgerEntry{"system", "init", "blockchain_started"}, privateKey);

    // Register entities that can make ledger entries
    ledgerChain.registerEntity("sensor-001", "active");
    ledgerChain.registerEntity("actuator-001", "active");
    ledgerChain.registerEntity("controller-001", "active");

    // Grant permissions
    ledgerChain.grantPermission("sensor-001", "READ_DATA");
    ledgerChain.grantPermission("actuator-001", "WRITE_DATA");
    ledgerChain.grantPermission("controller-001", "READ_DATA");
    ledgerChain.grantPermission("controller-001", "WRITE_DATA");

    // Create ledger entries
    std::vector<chain::Transaction<LedgerEntry>> entries;

    // Sensor reading
    LedgerEntry entry1{"sensor-001", "temperature_reading", "25.6C"};
    chain::Transaction<LedgerEntry> tx1("ledger-001", entry1, 120);
    tx1.signTransaction(privateKey);
    entries.push_back(tx1);

    // Actuator action
    LedgerEntry entry2{"actuator-001", "valve_adjustment", "opened_50%"};
    chain::Transaction<LedgerEntry> tx2("ledger-002", entry2, 110);
    tx2.signTransaction(privateKey);
    entries.push_back(tx2);

    // Controller decision
    LedgerEntry entry3{"controller-001", "decision", "increase_flow_rate"};
    chain::Transaction<LedgerEntry> tx3("ledger-003", entry3, 130);
    tx3.signTransaction(privateKey);
    entries.push_back(tx3);

    // Create block with ledger entries
    chain::Block<LedgerEntry> ledgerBlock(entries);

    std::cout << "Adding ledger block..." << std::endl;
    ledgerChain.addBlock(ledgerBlock);

    // Demonstrate Merkle tree verification
    std::cout << "\nMerkle Tree Verification:" << std::endl;
    for (size_t i = 0; i < entries.size(); i++) {
        auto verify_result = ledgerChain.blocks_.back().verifyTransaction(i);
        bool verified = verify_result.is_ok() && verify_result.value();
        std::cout << "Transaction " << i << " (" << entries[i].uuid_ << "): " << (verified ? "VERIFIED" : "FAILED")
                  << std::endl;
    }

    std::cout << "\nLedger Summary:" << std::endl;
    ledgerChain.printChainSummary();
}

void demonstrateDoubleSpendPrevention() {
    printSeparator("DOUBLE-SPEND PREVENTION DEMONSTRATION");

    auto privateKey = std::make_shared<chain::Crypto>("double_spend_key");

    // Create blockchain
    chain::Chain<RobotCommand> testChain("test-chain", "genesis", RobotCommand{"system", "all", "start", 255},
                                         privateKey);

    // Register a robot
    testChain.registerEntity("robot-alpha", "ready");

    // Create first transaction
    RobotCommand cmd1{"robot-alpha", "robot-beta", "EXECUTE_TASK", 100};
    chain::Transaction<RobotCommand> tx1("unique-cmd-001", cmd1, 100);
    tx1.signTransaction(privateKey);

    // Create second transaction with same ID (should be prevented)
    RobotCommand cmd2{"robot-alpha", "robot-gamma", "DIFFERENT_TASK", 100};
    chain::Transaction<RobotCommand> tx2("unique-cmd-001", cmd2, 100); // Same UUID!
    tx2.signTransaction(privateKey);

    // First block
    chain::Block<RobotCommand> block1({tx1});
    std::cout << "Adding first block..." << std::endl;
    testChain.addBlock(block1);

    // Second block with duplicate transaction
    chain::Block<RobotCommand> block2({tx2});
    std::cout << "Attempting to add block with duplicate transaction..." << std::endl;
    testChain.addBlock(block2); // This should fail

    std::cout << "\nDouble-Spend Prevention Summary:" << std::endl;
    testChain.printChainSummary();
}

void demonstrateMerkleTreeEfficiency() {
    printSeparator("MERKLE TREE EFFICIENCY DEMONSTRATION");

    // Create a large number of transactions to show efficiency
    std::vector<std::string> large_transaction_set;
    for (int i = 0; i < 1000; i++) {
        large_transaction_set.push_back("transaction_" + std::to_string(i) + "_data");
    }

    std::cout << "Creating Merkle tree with " << large_transaction_set.size() << " transactions..." << std::endl;

    chain::MerkleTree large_tree(large_transaction_set);

    auto root_result = large_tree.getRoot();
    std::string root = root_result.is_ok() ? root_result.value() : "ERROR";
    std::cout << "Merkle root: " << root.substr(0, 32) << "..." << std::endl;
    std::cout << "Total transactions: " << large_tree.getTransactionCount() << std::endl;

    // Verify a few transactions
    std::cout << "\nVerifying random transactions:" << std::endl;
    for (int i : {0, 100, 500, 999}) {
        auto proof_result = large_tree.getProof(i);
        if (!proof_result.is_ok()) {
            std::cout << "Transaction " << i << ": FAILED (could not get proof)" << std::endl;
            continue;
        }
        auto proof = proof_result.value();
        auto verify_result = large_tree.verifyProof(large_transaction_set[i], i, proof);
        bool verified = verify_result.is_ok() && verify_result.value();
        std::cout << "Transaction " << i << ": " << (verified ? "VERIFIED" : "FAILED")
                  << " (proof size: " << proof.size() << " hashes)" << std::endl;
    }

    std::cout << "\nMerkle tree allows efficient verification of individual transactions" << std::endl;
    std::cout << "without needing to process all " << large_transaction_set.size() << " transactions!" << std::endl;
}

int main() {
    std::cout << "Enhanced Blockit Library Demonstration" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "Demonstrating robot coordination, ledger tracking, and enhanced security features." << std::endl;

    try {
        demonstrateRobotCoordination();
        demonstrateLedgerTracking();
        demonstrateDoubleSpendPrevention();
        demonstrateMerkleTreeEfficiency();

        printSeparator("ENHANCED DEMONSTRATION COMPLETE");
        std::cout << "All enhanced features demonstrated successfully!" << std::endl;
        std::cout << "\nImplemented Features:" << std::endl;
        std::cout << "✅ Entity Management - Robot/system authorization and state tracking" << std::endl;
        std::cout << "✅ Double-Spend Prevention - Duplicate transaction detection" << std::endl;
        std::cout << "✅ Merkle Trees - Efficient transaction verification" << std::endl;
        std::cout << "✅ Enhanced Block Validation - Cryptographic integrity checks" << std::endl;
        std::cout << "✅ Permission System - Role-based access control" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
