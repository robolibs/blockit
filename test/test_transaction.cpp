#include "blockit.hpp"
#include <doctest/doctest.h>
#include <memory>

// Test data structure
struct TestData {
    std::string value;
    int number;

    std::string to_string() const { return "TestData{value:" + value + ",number:" + std::to_string(number) + "}"; }
};

TEST_SUITE("Transaction Tests") {
    TEST_CASE("Transaction creation and basic properties") {
        TestData data{"test_data", 42};
        chain::Transaction<TestData> tx("tx-001", data, 100);

        CHECK(tx.uuid_ == "tx-001");
        CHECK(tx.priority_ == 100);
        CHECK(tx.function_.value == "test_data");
        CHECK(tx.function_.number == 42);
        CHECK(tx.timestamp_.sec > 0);
        CHECK(tx.timestamp_.nanosec >= 0);
    }

    TEST_CASE("Transaction priority validation") {
        TestData data{"test", 1};

        // Valid priorities
        chain::Transaction<TestData> tx1("tx-1", data, 0);   // Min priority
        chain::Transaction<TestData> tx2("tx-2", data, 255); // Max priority
        chain::Transaction<TestData> tx3("tx-3", data, 100); // Normal priority

        CHECK(tx1.priority_ == 0);
        CHECK(tx2.priority_ == 255);
        CHECK(tx3.priority_ == 100);
    }

    TEST_CASE("Transaction signing") {
        auto privateKey = std::make_shared<chain::Crypto>("test_key");
        TestData data{"sign_test", 123};
        chain::Transaction<TestData> tx("tx-sign", data, 150);

        // Initially unsigned
        CHECK(tx.signature_.empty());

        // Sign transaction
        tx.signTransaction(privateKey);

        // Should now have signature
        CHECK_FALSE(tx.signature_.empty());
        CHECK(tx.signature_.size() > 0);
    }

    TEST_CASE("Transaction toString method") {
        TestData data{"string_test", 456};
        chain::Transaction<TestData> tx("tx-string", data, 200);

        std::string txString = tx.toString();
        CHECK_FALSE(txString.empty());

        // Should contain key components
        CHECK(txString.find("tx-string") != std::string::npos);
        CHECK(txString.find("200") != std::string::npos);
    }

    TEST_CASE("Transaction validation") {
        auto privateKey = std::make_shared<chain::Crypto>("validation_key");
        TestData data{"valid_test", 789};
        chain::Transaction<TestData> tx("tx-valid", data, 175);

        // Before signing - should be invalid due to empty signature
        CHECK_FALSE(tx.isValid());

        // After signing - should be valid
        tx.signTransaction(privateKey);
        CHECK(tx.isValid());
    }

    TEST_CASE("Transaction with invalid priority should still create but fail validation") {
        TestData data{"invalid", 1};
        chain::Transaction<TestData> tx("tx-invalid", data, -1); // Invalid priority

        CHECK(tx.priority_ == -1); // Should store the value
        CHECK_FALSE(tx.isValid()); // But validation should fail
    }
}
