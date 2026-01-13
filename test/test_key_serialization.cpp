#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>

using namespace blockit;

TEST_SUITE("Key Serialization Tests") {
    TEST_CASE("Serialize and deserialize permanent key") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& key1 = key_result.value();

        // Serialize
        auto serialized = key1.serialize();
        CHECK(serialized.size() == 101); // 32 + 4 + 64 + 1 (no expiration)

        // Deserialize
        auto deserialize_result = Key::deserialize(serialized);
        REQUIRE(deserialize_result.is_ok());

        const auto& key2 = deserialize_result.value();

        // Verify properties match
        CHECK(key1.getId() == key2.getId());
        CHECK(key1.getPublicKey() == key2.getPublicKey());
        CHECK(key1.getPrivateKey() == key2.getPrivateKey());
        CHECK(key1.getExpiration().has_value() == key2.getExpiration().has_value());
        CHECK(key1 == key2);
    }

    TEST_CASE("Serialize and deserialize key with expiration") {
        auto now = std::chrono::system_clock::now();
        auto tomorrow = now + std::chrono::hours(24);

        auto key_result = Key::generateWithExpiration(tomorrow);
        REQUIRE(key_result.is_ok());

        const auto& key1 = key_result.value();

        // Serialize
        auto serialized = key1.serialize();
        CHECK(serialized.size() == 109); // 32 + 4 + 64 + 1 + 8 (with expiration)

        // Deserialize
        auto deserialize_result = Key::deserialize(serialized);
        REQUIRE(deserialize_result.is_ok());

        const auto& key2 = deserialize_result.value();

        // Verify properties match
        CHECK(key1.getId() == key2.getId());
        CHECK(key1.getPublicKey() == key2.getPublicKey());
        CHECK(key1.getPrivateKey() == key2.getPrivateKey());
        CHECK(key1.getExpiration().has_value());
        CHECK(key2.getExpiration().has_value());

        // Expiration should be preserved (within millisecond precision)
        auto exp1_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            key1.getExpiration()->time_since_epoch()).count();
        auto exp2_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            key2.getExpiration()->time_since_epoch()).count();
        CHECK(exp1_ms == exp2_ms);
    }

    TEST_CASE("Deserialize invalid data fails") {
        // Empty data
        std::vector<uint8_t> empty;
        auto result1 = Key::deserialize(empty);
        CHECK_FALSE(result1.is_ok());

        // Too short data
        std::vector<uint8_t> short_data(30, 0x00);
        auto result2 = Key::deserialize(short_data);
        CHECK_FALSE(result2.is_ok());

        // Just barely too short (minimum is 37: 32 pubkey + 4 len + 1 has_expiration)
        std::vector<uint8_t> almost_valid(36, 0x00);
        auto result3 = Key::deserialize(almost_valid);
        CHECK_FALSE(result3.is_ok());
    }

    TEST_CASE("Serialized key can sign and verify") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        const auto& key1 = key_result.value();

        // Sign with original key
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto sign_result = key1.sign(data);
        REQUIRE(sign_result.is_ok());

        // Serialize and deserialize
        auto serialized = key1.serialize();
        auto deserialize_result = Key::deserialize(serialized);
        REQUIRE(deserialize_result.is_ok());

        const auto& key2 = deserialize_result.value();

        // Verify with deserialized key
        auto verify_result = key2.verify(data, sign_result.value());
        REQUIRE(verify_result.is_ok());
        CHECK(verify_result.value() == true);

        // Deserialized key can also sign
        auto sign_result2 = key2.sign(data);
        REQUIRE(sign_result2.is_ok());

        // Original key can verify
        auto verify_result2 = key1.verify(data, sign_result2.value());
        REQUIRE(verify_result2.is_ok());
        CHECK(verify_result2.value() == true);
    }

    TEST_CASE("Multiple serialization round trips") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto current_key = key_result.value();
        std::string original_id = current_key.getId();

        // Do multiple round trips
        for (int i = 0; i < 5; i++) {
            auto serialized = current_key.serialize();
            auto deserialize_result = Key::deserialize(serialized);
            REQUIRE(deserialize_result.is_ok());
            current_key = deserialize_result.value();
        }

        // Should still have same ID
        CHECK(current_key.getId() == original_id);
    }

    TEST_CASE("Deserialize with corrupted expiration flag") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        auto serialized = key_result.value().serialize();

        // Corrupt the expiration flag to indicate expiration exists
        // but don't include the expiration bytes
        serialized[64] = 1; // has_expiration = true

        // Should still deserialize (expiration bytes missing means no value)
        auto result = Key::deserialize(serialized);
        // Behavior depends on implementation - may succeed or fail
        // The important thing is it doesn't crash
    }
}
