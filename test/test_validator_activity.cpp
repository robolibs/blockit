#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <chrono>
#include <thread>

using namespace blockit::ledger;

TEST_SUITE("Validator Activity Tests") {
    TEST_CASE("Update activity timestamp") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        validator.updateActivity();
        CHECK(validator.isOnline(60000)); // Online within 60s

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        validator.updateActivity();

        CHECK(validator.isOnline(60000)); // Still online
    }

    TEST_CASE("Online status with short timeout") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        validator.updateActivity();
        CHECK(validator.isOnline(60000)); // Online with 60s timeout

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Should still be online with 60s timeout
        CHECK(validator.isOnline(60000));

        // But would be offline with very short timeout
        CHECK_FALSE(validator.isOnline(50)); // 50ms timeout
    }

    TEST_CASE("Get last seen timestamp") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        auto before = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        validator.updateActivity();

        auto after = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto last_seen = validator.getLastSeen();
        CHECK(last_seen >= before);
        CHECK(last_seen <= after);
    }

    TEST_CASE("Mark online updates activity") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        // Mark offline first
        validator.markOffline();
        CHECK(validator.getStatus() == ValidatorStatus::OFFLINE);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto before_mark = validator.getLastSeen();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Mark online should update activity
        validator.markOnline();
        CHECK(validator.getStatus() == ValidatorStatus::ACTIVE);

        auto after_mark = validator.getLastSeen();
        CHECK(after_mark >= before_mark);
    }

    TEST_CASE("Offline status does not affect isOnline check") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        validator.updateActivity();

        // Mark offline changes status but not the activity timestamp
        validator.markOffline();
        CHECK(validator.getStatus() == ValidatorStatus::OFFLINE);

        // isOnline checks activity timestamp, not status
        // So it should still return true if activity was recent
        CHECK(validator.isOnline(60000));

        // But canSign should return false
        CHECK_FALSE(validator.canSign());
    }

    TEST_CASE("Revoked validator cannot sign even if online") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        validator.updateActivity();
        CHECK(validator.canSign());

        validator.revokeValidator();

        // Still shows as "online" (recent activity)
        CHECK(validator.isOnline(60000));

        // But cannot sign because revoked
        CHECK_FALSE(validator.canSign());
    }

    TEST_CASE("Activity timestamp persists through status changes") {
        auto key_result = Key::generate();
        REQUIRE(key_result.is_ok());

        Validator validator("alice", key_result.value());

        validator.updateActivity();
        auto initial_last_seen = validator.getLastSeen();

        // Status changes don't affect last_seen
        validator.markOffline();
        CHECK(validator.getLastSeen() == initial_last_seen);

        validator.setStatus(ValidatorStatus::REVOKED);
        CHECK(validator.getLastSeen() == initial_last_seen);
    }
}
