#include "blockit/blockit.hpp"
#include <chrono>
#include <doctest/doctest.h>
#include <memory>
#include <thread>
#include <vector>

// Integration tests combining multiple components

struct IntegrationTestData {
    std::string type;
    std::string payload;
    int priority;

    std::string to_string() const {
        return "IntegrationTestData{" + type + ":" + payload + " (priority:" + std::to_string(priority) + ")}";
    }
};

TEST_SUITE("Integration Tests") {
    TEST_CASE("Complete transaction lifecycle") {
        auto privateKey = std::make_shared<chain::Crypto>("integration_key");

        // Create blockchain with authenticator
        chain::Chain<IntegrationTestData> blockchain("integration-chain", "genesis",
                                                     IntegrationTestData{"genesis", "initial", 0}, privateKey);

        // Register participants with proper parameter order
        blockchain.registerParticipant("alice", "active", {{"role", "user"}, {"department", "research"}});
        blockchain.registerParticipant("bob", "active", {{"role", "admin"}, {"department", "IT"}});
        blockchain.grantCapability("alice", "create_transaction");
        blockchain.grantCapability("bob", "validate_transaction");

        // Create and sign transaction
        chain::Transaction<IntegrationTestData> tx("integration-tx-001",
                                                   IntegrationTestData{"transfer", "alice->bob:100", 5}, 150);
        tx.signTransaction(privateKey);

        // Verify transaction is valid
        auto tx_valid = tx.isValid();
        CHECK(tx_valid.is_ok());
        CHECK(tx_valid.value());
        CHECK(blockchain.canParticipantPerform("alice", "create_transaction"));

        // Create block with transaction
        chain::Block<IntegrationTestData> block({tx});

        // Verify block structure
        CHECK(block.transactions_.size() == 1);
        CHECK(block.merkle_root_.size() > 0);
        auto verify0 = block.verifyTransaction(0);
        CHECK(verify0.is_ok());
        CHECK(verify0.value()); // Verify first transaction by index

        // Add block to chain
        CHECK(blockchain.addBlock(block).is_ok());
        CHECK(blockchain.getChainLength() == 2); // Genesis + new block

        // Verify chain integrity
        auto chain_valid = blockchain.isChainValid();
        CHECK(chain_valid.is_ok());
        CHECK(chain_valid.value());

        // Check double-spend prevention
        CHECK(blockchain.isTransactionUsed("integration-tx-001"));

        // Try to add same transaction again (should fail)
        chain::Block<IntegrationTestData> duplicateBlock({tx});
        CHECK_FALSE(blockchain.addBlock(duplicateBlock).is_ok());
    }

    TEST_CASE("Multi-participant blockchain workflow") {
        auto privateKey = std::make_shared<chain::Crypto>("multi_key");

        chain::Chain<IntegrationTestData> blockchain("multi-chain", "genesis",
                                                     IntegrationTestData{"genesis", "start", 0}, privateKey);

        // Setup multiple participants with different roles
        std::vector<std::string> participants = {"robot1", "robot2", "supervisor", "maintenance"};
        std::vector<std::string> capabilities = {"sensor_reading", "movement", "coordination", "repair"};

        for (const auto &participant : participants) {
            blockchain.registerParticipant(participant, "active", {{"type", "autonomous_agent"}});
        }

        // Grant specific capabilities
        blockchain.grantCapability("robot1", "sensor_reading");
        blockchain.grantCapability("robot2", "movement");
        blockchain.grantCapability("supervisor", "coordination");
        blockchain.grantCapability("maintenance", "repair");

        // Create a sequence of coordinated transactions
        std::vector<chain::Transaction<IntegrationTestData>> transactions;

        // Robot1 reports sensor data
        transactions.emplace_back("sensor-001", IntegrationTestData{"sensor", "temperature:25.5C", 1}, 200);
        transactions.back().signTransaction(privateKey);

        // Supervisor coordinates based on sensor data
        transactions.emplace_back("coord-001", IntegrationTestData{"coordinate", "move_to_sector_B", 2}, 180);
        transactions.back().signTransaction(privateKey);

        // Robot2 executes movement
        transactions.emplace_back("move-001", IntegrationTestData{"movement", "sector_B_reached", 3}, 160);
        transactions.back().signTransaction(privateKey);

        // Maintenance logs action
        transactions.emplace_back("maint-001", IntegrationTestData{"maintenance", "routine_check_completed", 4}, 140);
        transactions.back().signTransaction(privateKey);

        // Add all transactions in a single block
        chain::Block<IntegrationTestData> workflowBlock(transactions);
        CHECK(blockchain.addBlock(workflowBlock).is_ok());

        // Verify all transactions are recorded
        for (size_t i = 0; i < transactions.size(); i++) {
            CHECK(blockchain.isTransactionUsed(std::string(transactions[i].uuid_.c_str())));
            auto verify_i = workflowBlock.verifyTransaction(i);
            CHECK(verify_i.is_ok());
            CHECK(verify_i.value()); // Verify transaction by index
        }

        // Verify Merkle tree integrity
        std::vector<std::string> tx_strings;
        for (const auto &txn : transactions) {
            tx_strings.push_back(txn.toString());
        }
        chain::MerkleTree merkleTree(tx_strings);
        auto root_result = merkleTree.getRoot();
        CHECK(root_result.is_ok());
        CHECK(root_result.value() == std::string(workflowBlock.merkle_root_.c_str()));

        // Test transaction verification through Merkle proof
        for (size_t i = 0; i < transactions.size(); i++) {
            auto proof_result = merkleTree.getProof(i);
            REQUIRE(proof_result.is_ok());
            auto proof = proof_result.value();
            auto verify_result = merkleTree.verifyProof(transactions[i].toString(), i, proof);
            CHECK(verify_result.is_ok());
            CHECK(verify_result.value());
        }
    }

    TEST_CASE("Blockchain state consistency under concurrent operations") {
        auto privateKey = std::make_shared<chain::Crypto>("concurrent_key");

        chain::Chain<IntegrationTestData> blockchain("concurrent-chain", "genesis",
                                                     IntegrationTestData{"genesis", "concurrent_test", 0}, privateKey);

        // Register multiple participants
        for (int i = 1; i <= 5; i++) {
            std::string participantId = "agent" + std::to_string(i);
            blockchain.registerParticipant(participantId, "active", {{"agent_id", std::to_string(i)}});
            blockchain.grantCapability(participantId, "submit_data");
        }

        // Simulate concurrent transaction creation (sequential for testing)
        std::vector<chain::Transaction<IntegrationTestData>> concurrentTxs;

        for (int i = 1; i <= 10; i++) {
            std::string txId = "concurrent-tx-" + std::to_string(i);
            std::string agentId = "agent" + std::to_string((i % 5) + 1);

            concurrentTxs.emplace_back(
                txId, IntegrationTestData{"data_submission", agentId + "_data_" + std::to_string(i), i}, 100 + i);
            concurrentTxs.back().signTransaction(privateKey);
        }

        // Add transactions in blocks
        for (size_t i = 0; i < concurrentTxs.size(); i += 2) {
            std::vector<chain::Transaction<IntegrationTestData>> blockTxs;
            blockTxs.push_back(concurrentTxs[i]);
            if (i + 1 < concurrentTxs.size()) {
                blockTxs.push_back(concurrentTxs[i + 1]);
            }

            chain::Block<IntegrationTestData> block(blockTxs);
            CHECK(blockchain.addBlock(block).is_ok());
        }

        // Verify final state consistency
        auto chain_valid = blockchain.isChainValid();
        CHECK(chain_valid.is_ok());
        CHECK(chain_valid.value());
        CHECK(blockchain.getChainLength() == 6); // Genesis + 5 blocks

        // Verify all transactions are accounted for
        for (const auto &tx : concurrentTxs) {
            CHECK(blockchain.isTransactionUsed(std::string(tx.uuid_.c_str())));
        }

        // Verify participant states
        for (int i = 1; i <= 5; i++) {
            std::string participantId = "agent" + std::to_string(i);
            CHECK(blockchain.isParticipantRegistered(participantId));
            CHECK(blockchain.canParticipantPerform(participantId, "submit_data"));
        }
    }

    TEST_CASE("End-to-end farming scenario integration") {
        auto privateKey = std::make_shared<chain::Crypto>("farming_key");

        chain::Chain<IntegrationTestData> farmChain("farm-integration", "genesis",
                                                    IntegrationTestData{"genesis", "farm_initialized", 0}, privateKey);

        // Setup farming ecosystem participants
        std::vector<std::pair<std::string, std::string>> farmEntities = {{"tractor_001", "equipment"},
                                                                         {"drone_alpha", "monitoring"},
                                                                         {"sensor_field_A", "environment"},
                                                                         {"weather_station", "meteorology"},
                                                                         {"farm_manager", "human_operator"}};

        for (const auto &[entityId, entityType] : farmEntities) {
            farmChain.registerParticipant(entityId, "active", {{"type", entityType}, {"location", "farm_sector_1"}});
        }

        // Grant appropriate capabilities
        farmChain.grantCapability("tractor_001", "soil_cultivation");
        farmChain.grantCapability("drone_alpha", "aerial_monitoring");
        farmChain.grantCapability("sensor_field_A", "environmental_sensing");
        farmChain.grantCapability("weather_station", "weather_reporting");
        farmChain.grantCapability("farm_manager", "operation_oversight");

        // Simulate a day's farming operations
        std::vector<chain::Transaction<IntegrationTestData>> dailyOperations;

        // Morning: Weather and soil conditions
        dailyOperations.emplace_back("weather_001", IntegrationTestData{"weather", "sunny,temp:22C,humidity:65%", 1},
                                     255);
        dailyOperations.back().signTransaction(privateKey);

        dailyOperations.emplace_back(
            "soil_001", IntegrationTestData{"soil_analysis", "moisture:45%,pH:6.8,nutrients:optimal", 2}, 240);
        dailyOperations.back().signTransaction(privateKey);

        // Midday: Equipment operations
        dailyOperations.emplace_back("tractor_op_001",
                                     IntegrationTestData{"cultivation", "sector_A_plowed,area:2.5_hectares", 3}, 220);
        dailyOperations.back().signTransaction(privateKey);

        dailyOperations.emplace_back("drone_survey_001",
                                     IntegrationTestData{"monitoring", "crop_health:95%,pest_activity:low", 4}, 200);
        dailyOperations.back().signTransaction(privateKey);

        // Evening: Management oversight
        dailyOperations.emplace_back(
            "mgmt_report_001", IntegrationTestData{"oversight", "daily_objectives_completed,efficiency:98%", 5}, 180);
        dailyOperations.back().signTransaction(privateKey);

        // Process operations in chronological blocks
        for (size_t i = 0; i < dailyOperations.size(); i += 2) {
            std::vector<chain::Transaction<IntegrationTestData>> timeSlotTxs;
            timeSlotTxs.push_back(dailyOperations[i]);
            if (i + 1 < dailyOperations.size()) {
                timeSlotTxs.push_back(dailyOperations[i + 1]);
            }

            chain::Block<IntegrationTestData> timeSlotBlock(timeSlotTxs);
            CHECK(farmChain.addBlock(timeSlotBlock).is_ok());
        }

        // Verify complete farming day integration
        auto chain_valid = farmChain.isChainValid();
        CHECK(chain_valid.is_ok());
        CHECK(chain_valid.value());
        CHECK(farmChain.getChainLength() == 4); // Genesis + 3 operational blocks

        // Verify all farming operations are recorded
        for (const auto &op : dailyOperations) {
            CHECK(farmChain.isTransactionUsed(std::string(op.uuid_.c_str())));
        }

        // Verify ecosystem integrity
        for (const auto &[entityId, entityType] : farmEntities) {
            CHECK(farmChain.isParticipantRegistered(entityId));
        }

        // Test audit trail capabilities
        auto lastBlock_result = farmChain.getLastBlock();
        REQUIRE(lastBlock_result.is_ok());
        auto lastBlock = lastBlock_result.value();
        CHECK(lastBlock.transactions_.size() == 1); // Management report
        CHECK(lastBlock.transactions_[0].function_.type == "oversight");
    }

    TEST_CASE("Performance and scalability integration test") {
        auto privateKey = std::make_shared<chain::Crypto>("performance_key");

        chain::Chain<IntegrationTestData> perfChain("performance-chain", "genesis",
                                                    IntegrationTestData{"genesis", "performance_test", 0}, privateKey);

        // Large-scale participant registration
        const int numParticipants = 100;
        for (int i = 1; i <= numParticipants; i++) {
            std::string participantId = "perf_participant_" + std::to_string(i);
            perfChain.registerParticipant(participantId, "active",
                                          {{"batch", std::to_string(i / 10)}, {"index", std::to_string(i)}});
            perfChain.grantCapability(participantId, "bulk_operation");
        }

        // Batch transaction processing
        const int numTransactions = 500;
        const int txsPerBlock = 10;

        auto startTime = std::chrono::high_resolution_clock::now();

        for (int batch = 0; batch < numTransactions / txsPerBlock; batch++) {
            std::vector<chain::Transaction<IntegrationTestData>> batchTxs;

            for (int i = 0; i < txsPerBlock; i++) {
                int txIndex = batch * txsPerBlock + i;
                std::string txId = "perf_tx_" + std::to_string(txIndex);
                std::string participantId = "perf_participant_" + std::to_string((txIndex % numParticipants) + 1);

                batchTxs.emplace_back(txId,
                                      IntegrationTestData{"bulk_operation",
                                                          participantId + "_operation_" + std::to_string(txIndex),
                                                          txIndex % 10},
                                      100);
                batchTxs.back().signTransaction(privateKey);
            }

            chain::Block<IntegrationTestData> batchBlock(batchTxs);
            CHECK(perfChain.addBlock(batchBlock).is_ok());
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Performance assertions
        auto chain_valid = perfChain.isChainValid();
        CHECK(chain_valid.is_ok());
        CHECK(chain_valid.value());
        CHECK(perfChain.getChainLength() == (numTransactions / txsPerBlock) + 1); // +1 for genesis

        // Verify transaction throughput (should be reasonable for testing)
        double txsPerSecond = (double)numTransactions / (duration.count() / 1000.0);
        CHECK(txsPerSecond > 10.0); // At least 10 transactions per second

        INFO("Processed ", numTransactions, " transactions in ", duration.count(), "ms");
        INFO("Throughput: ", txsPerSecond, " transactions/second");

        // Memory and state consistency checks
        for (int i = 1; i <= numParticipants; i++) {
            std::string participantId = "perf_participant_" + std::to_string(i);
            CHECK(perfChain.isParticipantRegistered(participantId));
        }
    }
}
