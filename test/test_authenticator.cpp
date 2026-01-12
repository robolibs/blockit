#include "blockit/blockit.hpp"
#include <doctest/doctest.h>
#include <unordered_map>

TEST_SUITE("Authenticator Tests") {
    TEST_CASE("Participant registration") {
        chain::Authenticator auth;

        // Register participant without metadata
        auth.registerParticipant("robot-001", "idle");

        CHECK(auth.isParticipantAuthorized("robot-001"));
        CHECK(auth.getParticipantState("robot-001") == "idle");
        CHECK_FALSE(auth.isParticipantAuthorized("robot-999"));
    }

    TEST_CASE("Participant registration with metadata") {
        chain::Authenticator auth;
        std::unordered_map<std::string, std::string> metadata = {
            {"model", "Industrial_Robot_v2"}, {"location", "Factory_Floor_A"}, {"max_payload", "50kg"}};

        auth.registerParticipant("robot-002", "ready", metadata);

        CHECK(auth.isParticipantAuthorized("robot-002"));
        CHECK(auth.getParticipantState("robot-002") == "ready");
        CHECK(auth.getParticipantMetadata("robot-002", "model") == "Industrial_Robot_v2");
        CHECK(auth.getParticipantMetadata("robot-002", "location") == "Factory_Floor_A");
        CHECK(auth.getParticipantMetadata("robot-002", "max_payload") == "50kg");
        CHECK(auth.getParticipantMetadata("robot-002", "nonexistent") == "");
    }

    TEST_CASE("State management") {
        chain::Authenticator auth;
        auth.registerParticipant("device-001", "inactive");

        // Update state
        CHECK(auth.updateParticipantState("device-001", "active"));
        CHECK(auth.getParticipantState("device-001") == "active");

        // Update state again
        CHECK(auth.updateParticipantState("device-001", "maintenance"));
        CHECK(auth.getParticipantState("device-001") == "maintenance");

        // Try to update state of unauthorized participant
        CHECK_FALSE(auth.updateParticipantState("unknown-device", "active"));
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
        auth.markTransactionUsed("tx-001");
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
        CHECK(auth.validateAndRecordAction("tractor-001", "spray_pesticide", "action-001", "SPRAY"));

        // Action should now be recorded
        CHECK(auth.isTransactionUsed("action-001"));

        // Try duplicate action - should fail
        CHECK_FALSE(auth.validateAndRecordAction("tractor-001", "spray_again", "action-001", "SPRAY"));

        // Valid action without capability requirement
        CHECK(auth.validateAndRecordAction("tractor-001", "status_update", "action-002"));

        // Action with missing capability - should fail
        CHECK_FALSE(auth.validateAndRecordAction("tractor-001", "harvest_grain", "action-003", "HARVEST"));

        // Unauthorized participant - should fail
        CHECK_FALSE(auth.validateAndRecordAction("unknown-device", "some_action", "action-004"));
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
        auth.setParticipantMetadata("device-001", "firmware_version", "v2.1.0");
        auth.setParticipantMetadata("device-001", "last_maintenance", "2025-01-15");

        // Get metadata
        CHECK(auth.getParticipantMetadata("device-001", "firmware_version") == "v2.1.0");
        CHECK(auth.getParticipantMetadata("device-001", "last_maintenance") == "2025-01-15");
        CHECK(auth.getParticipantMetadata("device-001", "nonexistent") == "");

        // Try to set metadata for unauthorized participant (should be ignored)
        auth.setParticipantMetadata("unknown-device", "some_key", "some_value");
        CHECK(auth.getParticipantMetadata("unknown-device", "some_key") == "");
    }
}
