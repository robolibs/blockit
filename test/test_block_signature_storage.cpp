#include "blockit/blockit.hpp"
#include <doctest/doctest.h>

using namespace blockit;

// Test data structure
struct TestData {
    std::string name;
    int value;

    std::string to_string() const { return "TestData{" + name + "," + std::to_string(value) + "}"; }

    auto members() { return std::tie(name, value); }
    auto members() const { return std::tie(name, value); }
};

TEST_SUITE("Block Signature Storage Tests") {
    TEST_CASE("Block with validator signatures serialization") {
        // Create a block with transactions
        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<TestData> tx("tx_001", TestData{"test", 42}, 100);
        tx.signTransaction(privateKey);

        Block<TestData> block({tx});
        block.setProposer("alice");

        // Add validator signatures
        std::vector<uint8_t> sig1(64, 0xAA);
        std::vector<uint8_t> sig2(64, 0xBB);

        auto add_result1 = block.addValidatorSignature("validator_001", "alice", sig1);
        REQUIRE(add_result1.is_ok());

        auto add_result2 = block.addValidatorSignature("validator_002", "bob", sig2);
        REQUIRE(add_result2.is_ok());

        CHECK(block.countValidSignatures() == 2);
        CHECK(block.hasSigned("validator_001"));
        CHECK(block.hasSigned("validator_002"));
        CHECK_FALSE(block.hasSigned("validator_003"));

        // Serialize
        auto serialized = block.serialize();
        REQUIRE(!serialized.empty());

        // Deserialize
        auto deserialized = Block<TestData>::deserialize(serialized);
        REQUIRE(deserialized.is_ok());

        auto &restored = deserialized.value();
        CHECK(restored.getProposer() == "alice");
        CHECK(restored.countValidSignatures() == 2);
        CHECK(restored.hasSigned("validator_001"));
        CHECK(restored.hasSigned("validator_002"));
    }

    TEST_CASE("Duplicate signature rejected") {
        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<TestData> tx("tx_001", TestData{"test", 42}, 100);
        tx.signTransaction(privateKey);

        Block<TestData> block({tx});

        std::vector<uint8_t> sig(64, 0xAA);

        auto add_result1 = block.addValidatorSignature("validator_001", "alice", sig);
        REQUIRE(add_result1.is_ok());

        // Try to add same validator again
        auto add_result2 = block.addValidatorSignature("validator_001", "alice", sig);
        CHECK_FALSE(add_result2.is_ok());

        CHECK(block.countValidSignatures() == 1);
    }

    TEST_CASE("BlockSignature serialization") {
        BlockSignature sig;
        sig.validator_id = dp::String("validator_001");
        sig.participant_id = dp::String("alice");
        std::vector<uint8_t> sig_data(64, 0xCC);
        sig.signature = dp::Vector<dp::u8>(sig_data.begin(), sig_data.end());
        sig.signed_at = 1234567890;

        auto serialized = sig.serialize();
        REQUIRE(!serialized.empty());

        auto deserialized = BlockSignature::deserialize(serialized);
        REQUIRE(deserialized.is_ok());

        auto &restored = deserialized.value();
        CHECK(std::string(restored.validator_id.c_str()) == "validator_001");
        CHECK(std::string(restored.participant_id.c_str()) == "alice");
        CHECK(restored.signature.size() == 64);
        CHECK(restored.signed_at == 1234567890);
    }

    TEST_CASE("Block with empty signatures") {
        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<TestData> tx("tx_001", TestData{"test", 42}, 100);
        tx.signTransaction(privateKey);

        Block<TestData> block({tx});

        CHECK(block.countValidSignatures() == 0);
        CHECK(block.getProposer().empty());

        // Serialize and deserialize empty signatures
        auto serialized = block.serialize();
        auto deserialized = Block<TestData>::deserialize(serialized);
        REQUIRE(deserialized.is_ok());

        CHECK(deserialized.value().countValidSignatures() == 0);
    }

    TEST_CASE("Proposer ID") {
        auto privateKey = std::make_shared<Crypto>("test_key");
        Transaction<TestData> tx("tx_001", TestData{"test", 42}, 100);
        tx.signTransaction(privateKey);

        Block<TestData> block({tx});

        CHECK(block.getProposer().empty());

        block.setProposer("leader_robot");
        CHECK(block.getProposer() == "leader_robot");

        // Verify persistence
        auto serialized = block.serialize();
        auto deserialized = Block<TestData>::deserialize(serialized);
        REQUIRE(deserialized.is_ok());

        CHECK(deserialized.value().getProposer() == "leader_robot");
    }
}
