#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>
#include <thread>

using namespace blockit::ledger;

TEST_SUITE("Key Identity Tests") {
    TEST_CASE("Generate permanent key") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        CHECK(key.getPublicKey().size() == 32);
        CHECK(key.getPrivateKey().size() == 64); // Ed25519 private key is 64 bytes (seed + pubkey)
        CHECK(key.hasPrivateKey());
        CHECK(key.isValid());
        CHECK_FALSE(key.isExpired());
        CHECK_FALSE(key.getId().empty());
        CHECK(key.getId() != "unknown");
    }

    TEST_CASE("Generate key with future expiration") {
        auto now = std::chrono::system_clock::now();
        auto tomorrow = now + std::chrono::hours(24);

        auto key_result = Key::generateWithExpiration(tomorrow);
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        CHECK(key.isValid());
        CHECK_FALSE(key.isExpired());
        CHECK(key.getExpiration().has_value());
        CHECK(key.getExpiration().value() == tomorrow);
    }

    TEST_CASE("Generate key with past expiration (already expired)") {
        auto now = std::chrono::system_clock::now();
        auto yesterday = now - std::chrono::hours(24);

        auto key_result = Key::generateWithExpiration(yesterday);
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        CHECK(key.isExpired());
        CHECK_FALSE(key.isValid()); // Invalid because expired
        CHECK(key.getExpiration().has_value());
    }

    TEST_CASE("Sign and verify data") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

        auto sign_result = key.sign(data);
        REQUIRE(sign_result.is_ok());

        const auto& signature = sign_result.value();
        CHECK(signature.size() == 64); // Ed25519 signature is 64 bytes

        auto verify_result = key.verify(data, signature);
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Verify fails with wrong data") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        std::vector<uint8_t> wrong_data = {0x04, 0x05, 0x06};

        auto sign_result = key.sign(data);
        REQUIRE(sign_result.is_ok());

        auto verify_result = key.verify(wrong_data, sign_result.value());
        // Verification should either fail or return false
        if (verify_result.is_ok()) {
            CHECK(verify_result.value() == false);
        }
    }

    TEST_CASE("Verify fails with wrong signature") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& key = key_result.value();
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        std::vector<uint8_t> fake_signature(64, 0xFF);

        auto verify_result = key.verify(data, fake_signature);
        // Should either fail or return false
        if (verify_result.is_ok()) {
            CHECK(verify_result.value() == false);
        }
    }

    TEST_CASE("Cross-key verification") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        const auto& key1 = key1_result.value();
        const auto& key2 = key2_result.value();
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};

        // Sign with key1
        auto sign_result = key1.sign(data);
        REQUIRE(sign_result.is_ok());

        // Verify with key2 should fail
        auto verify_result = key2.verify(data, sign_result.value());
        if (verify_result.is_ok()) {
            CHECK(verify_result.value() == false);
        }
    }

    TEST_CASE("Unique IDs for different keys") {
        auto key1_result = Key::generate();
        auto key2_result = Key::generate();
        REQUIRE(key1_result.is_ok());
        REQUIRE(key2_result.is_ok());

        CHECK(key1_result.value().getId() != key2_result.value().getId());
    }

    TEST_CASE("Load from keypair bytes") {
        auto key1_result = Key::generate();
        REQUIRE(key1_result.is_ok());

        const auto& key1 = key1_result.value();

        // Load from the same key bytes
        auto key2_result = Key::fromKeypair(key1.getPublicKey(), key1.getPrivateKey());
        REQUIRE(key2_result.is_ok());

        const auto& key2 = key2_result.value();

        // Should have same ID
        CHECK(key1.getId() == key2.getId());

        // Should be able to sign and verify the same
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto sign1 = key1.sign(data);
        auto sign2 = key2.sign(data);
        REQUIRE(sign1.is_ok());
        REQUIRE(sign2.is_ok());

        // key2 should verify key1's signature
        auto verify_result = key2.verify(data, sign1.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Load from public key only") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& full_key = key_result.value();

        // Create key from public key only
        auto pub_key_result = Key::fromPublicKey(full_key.getPublicKey());
        REQUIRE(pub_key_result.is_ok());

        const auto& pub_key = pub_key_result.value();

        // Should have same ID
        CHECK(full_key.getId() == pub_key.getId());

        // Should not be able to sign
        CHECK_FALSE(pub_key.hasPrivateKey());
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto sign_result = pub_key.sign(data);
        CHECK_FALSE(sign_result.is_ok());

        // But should be able to verify
        auto full_sign = full_key.sign(data);
        REQUIRE(full_sign.is_ok());

        auto verify_result = pub_key.verify(data, full_sign.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);
    }

    TEST_CASE("Invalid keypair bytes rejected") {
        std::vector<uint8_t> too_short(16, 0x00);

        auto result = Key::fromKeypair(too_short, too_short);
        CHECK_FALSE(result.is_ok());
    }

    TEST_CASE("Set and clear expiration") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto key = key_result.value();
        CHECK_FALSE(key.getExpiration().has_value());

        // Set expiration
        auto future = std::chrono::system_clock::now() + std::chrono::hours(1);
        key.setExpiration(future);
        CHECK(key.getExpiration().has_value());
        CHECK_FALSE(key.isExpired());

        // Clear expiration
        key.clearExpiration();
        CHECK_FALSE(key.getExpiration().has_value());
    }

    TEST_CASE("Key equality operator") {
        auto key1_result = Key::generate();
        REQUIRE(key1_result.is_ok());

        const auto& key1 = key1_result.value();
        auto key2_result = Key::fromKeypair(key1.getPublicKey(), key1.getPrivateKey());
        REQUIRE(key2_result.is_ok());

        // Same public key should be equal
        CHECK(key1 == key2_result.value());

        // Different key should not be equal
        auto key3_result = Key::generate();
        REQUIRE(key3_result.is_ok());
        CHECK(key1 != key3_result.value());
    }
}
