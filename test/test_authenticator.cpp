#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <unordered_map>

TEST_SUITE("Authenticator Tests") {
    TEST_CASE("Participant registration") {
        chain::Authenticator auth;

        // Register participant without metadata
        auth.registerParticipant("robot-001", "idle");

        CHECK(auth.isParticipantAuthorized("robot-001"));
        auto state_result = auth.getParticipantState("robot-001");
        CHECK(state_result.is_ok());
        CHECK(state_result.value() == "idle");
        CHECK_FALSE(auth.isParticipantAuthorized("robot-999"));
    }

    TEST_CASE("Participant registration with metadata") {
        chain::Authenticator auth;
        std::unordered_map<std::string, std::string> metadata = {
            {"model", "Industrial_Robot_v2"}, {"location", "Factory_Floor_A"}, {"max_payload", "50kg"}};

        auth.registerParticipant("robot-002", "ready", metadata);

        CHECK(auth.isParticipantAuthorized("robot-002"));
        auto state_result = auth.getParticipantState("robot-002");
        CHECK(state_result.is_ok());
        CHECK(state_result.value() == "ready");

        auto model_result = auth.getParticipantMetadata("robot-002", "model");
        CHECK(model_result.is_ok());
        CHECK(model_result.value() == "Industrial_Robot_v2");

        auto location_result = auth.getParticipantMetadata("robot-002", "location");
        CHECK(location_result.is_ok());
        CHECK(location_result.value() == "Factory_Floor_A");

        auto payload_result = auth.getParticipantMetadata("robot-002", "max_payload");
        CHECK(payload_result.is_ok());
        CHECK(payload_result.value() == "50kg");

        auto nonexistent_result = auth.getParticipantMetadata("robot-002", "nonexistent");
        CHECK_FALSE(nonexistent_result.is_ok());
    }

    TEST_CASE("State management") {
        chain::Authenticator auth;
        auth.registerParticipant("device-001", "inactive");

        // Update state
        CHECK(auth.updateParticipantState("device-001", "active").is_ok());
        auto state1 = auth.getParticipantState("device-001");
        CHECK(state1.is_ok());
        CHECK(state1.value() == "active");

        // Update state again
        CHECK(auth.updateParticipantState("device-001", "maintenance").is_ok());
        auto state2 = auth.getParticipantState("device-001");
        CHECK(state2.is_ok());
        CHECK(state2.value() == "maintenance");

        // Try to update state of unauthorized participant
        CHECK_FALSE(auth.updateParticipantState("unknown-device", "active").is_ok());
    }

    TEST_CASE("Capability management") {
        chain::Authenticator auth;
        auth.registerParticipant("robot-003", "ready");

        // Grant capabilities
        auth.grantCapability("robot-003", "MOVE");
        auth.grantCapability("robot-003", "PICK");
        auth.grantCapability("robot-003", "SCAN");

        // Check capabilities
        CHECK(auth.hasCapability("robot-003", "MOVE"));
        CHECK(auth.hasCapability("robot-003", "PICK"));
        CHECK(auth.hasCapability("robot-003", "SCAN"));
        CHECK_FALSE(auth.hasCapability("robot-003", "FLY")); // Not granted

        // Check capabilities for unauthorized participant
        CHECK_FALSE(auth.hasCapability("unknown-robot", "MOVE"));
    }

    TEST_CASE("Transaction duplicate detection") {
        chain::Authenticator auth;

        // First use should succeed
        CHECK_FALSE(auth.isTransactionUsed("tx-001"));
        CHECK(auth.markTransactionUsed("tx-001").is_ok());
        CHECK(auth.isTransactionUsed("tx-001"));

        // Different transaction should not be marked
        CHECK_FALSE(auth.isTransactionUsed("tx-002"));
    }

    TEST_CASE("Action validation and recording") {
        chain::Authenticator auth;
        auth.registerParticipant("tractor-001", "ready");
        auth.grantCapability("tractor-001", "SPRAY");
        auth.grantCapability("tractor-001", "TILLAGE");

        // Valid action with required capability
        CHECK(auth.validateAndRecordAction("tractor-001", "spray_pesticide", "action-001", "SPRAY").is_ok());

        // Action should now be recorded
        CHECK(auth.isTransactionUsed("action-001"));

        // Try duplicate action - should fail
        CHECK_FALSE(auth.validateAndRecordAction("tractor-001", "spray_again", "action-001", "SPRAY").is_ok());

        // Valid action without capability requirement
        CHECK(auth.validateAndRecordAction("tractor-001", "status_update", "action-002").is_ok());

        // Action with missing capability - should fail
        CHECK_FALSE(auth.validateAndRecordAction("tractor-001", "harvest_grain", "action-003", "HARVEST").is_ok());

        // Unauthorized participant - should fail
        CHECK_FALSE(auth.validateAndRecordAction("unknown-device", "some_action", "action-004").is_ok());
    }

    TEST_CASE("Multiple participants management") {
        chain::Authenticator auth;

        // Register multiple participants
        auth.registerParticipant("robot-A", "idle");
        auth.registerParticipant("robot-B", "active");
        auth.registerParticipant("sensor-001", "monitoring");

        // Grant different capabilities
        auth.grantCapability("robot-A", "MOVE");
        auth.grantCapability("robot-B", "MOVE");
        auth.grantCapability("robot-B", "PICK");
        auth.grantCapability("sensor-001", "READ_DATA");

        // Verify all participants
        auto participants = auth.getAuthorizedParticipants();
        CHECK(participants.size() == 3);
        CHECK(participants.find("robot-A") != participants.end());
        CHECK(participants.find("robot-B") != participants.end());
        CHECK(participants.find("sensor-001") != participants.end());

        // Verify capabilities
        CHECK(auth.hasCapability("robot-A", "MOVE"));
        CHECK_FALSE(auth.hasCapability("robot-A", "PICK"));
        CHECK(auth.hasCapability("robot-B", "MOVE"));
        CHECK(auth.hasCapability("robot-B", "PICK"));
        CHECK(auth.hasCapability("sensor-001", "READ_DATA"));
    }

    TEST_CASE("Metadata management") {
        chain::Authenticator auth;
        auth.registerParticipant("device-001", "ready");

        // Set metadata
        CHECK(auth.setParticipantMetadata("device-001", "firmware_version", "v2.1.0").is_ok());
        CHECK(auth.setParticipantMetadata("device-001", "last_maintenance", "2025-01-15").is_ok());

        // Get metadata
        auto fw_result = auth.getParticipantMetadata("device-001", "firmware_version");
        CHECK(fw_result.is_ok());
        CHECK(fw_result.value() == "v2.1.0");

        auto maint_result = auth.getParticipantMetadata("device-001", "last_maintenance");
        CHECK(maint_result.is_ok());
        CHECK(maint_result.value() == "2025-01-15");

        auto nonexistent = auth.getParticipantMetadata("device-001", "nonexistent");
        CHECK_FALSE(nonexistent.is_ok());

        // Try to set metadata for unauthorized participant (should fail)
        CHECK_FALSE(auth.setParticipantMetadata("unknown-device", "some_key", "some_value").is_ok());
        auto unknown_meta = auth.getParticipantMetadata("unknown-device", "some_key");
        CHECK_FALSE(unknown_meta.is_ok());
    }
}
