#include "blockit/identity/service.hpp"
#include <doctest/doctest.h>

using namespace blockit;

TEST_SUITE("Service Endpoint Tests") {

    TEST_CASE("Create service with predefined type") {
        Service service("did:blockit:abc123#swarm", ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");

        CHECK(service.getId() == "did:blockit:abc123#swarm");
        CHECK(service.getServiceType() == ServiceType::SwarmCoordinator);
        CHECK(service.getTypeString() == "SwarmCoordinator");
        CHECK(service.getServiceEndpoint() == "udp://192.168.1.100:9000");
        CHECK_FALSE(service.isCustomType());
    }

    TEST_CASE("Create service with custom type") {
        Service service("did:blockit:abc123#custom", "RobotTelemetry", "https://api.example.com/telemetry");

        CHECK(service.getId() == "did:blockit:abc123#custom");
        CHECK(service.getServiceType() == ServiceType::Custom);
        CHECK(service.getTypeString() == "RobotTelemetry");
        CHECK(service.getServiceEndpoint() == "https://api.example.com/telemetry");
        CHECK(service.isCustomType());
    }

    TEST_CASE("Service type to string") {
        CHECK(serviceTypeToString(ServiceType::SwarmCoordinator) == "SwarmCoordinator");
        CHECK(serviceTypeToString(ServiceType::SensorDataEndpoint) == "SensorDataEndpoint");
        CHECK(serviceTypeToString(ServiceType::TaskQueue) == "TaskQueue");
        CHECK(serviceTypeToString(ServiceType::CredentialRepository) == "CredentialRepository");
        CHECK(serviceTypeToString(ServiceType::DIDCommMessaging) == "DIDCommMessaging");
        CHECK(serviceTypeToString(ServiceType::LinkedDomains) == "LinkedDomains");
        CHECK(serviceTypeToString(ServiceType::Custom) == "Custom");
    }

    TEST_CASE("All predefined service types") {
        Service s1("id1", ServiceType::SwarmCoordinator, "endpoint1");
        CHECK(s1.getServiceType() == ServiceType::SwarmCoordinator);

        Service s2("id2", ServiceType::SensorDataEndpoint, "endpoint2");
        CHECK(s2.getServiceType() == ServiceType::SensorDataEndpoint);

        Service s3("id3", ServiceType::TaskQueue, "endpoint3");
        CHECK(s3.getServiceType() == ServiceType::TaskQueue);

        Service s4("id4", ServiceType::CredentialRepository, "endpoint4");
        CHECK(s4.getServiceType() == ServiceType::CredentialRepository);

        Service s5("id5", ServiceType::DIDCommMessaging, "endpoint5");
        CHECK(s5.getServiceType() == ServiceType::DIDCommMessaging);

        Service s6("id6", ServiceType::LinkedDomains, "endpoint6");
        CHECK(s6.getServiceType() == ServiceType::LinkedDomains);
    }

    TEST_CASE("Service equality comparison") {
        Service s1("did:blockit:abc123#swarm", ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");
        Service s2("did:blockit:abc123#swarm", ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");
        Service s3("did:blockit:abc123#other", ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");

        CHECK(s1 == s2);
        CHECK(s1 != s3);
    }

    TEST_CASE("Service with various endpoint formats") {
        // UDP endpoint
        Service udp_service("id1", ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");
        CHECK(udp_service.getServiceEndpoint() == "udp://192.168.1.100:9000");

        // HTTPS endpoint
        Service https_service("id2", ServiceType::CredentialRepository, "https://example.com/credentials");
        CHECK(https_service.getServiceEndpoint() == "https://example.com/credentials");

        // WebSocket endpoint
        Service ws_service("id3", ServiceType::DIDCommMessaging, "wss://messaging.example.com");
        CHECK(ws_service.getServiceEndpoint() == "wss://messaging.example.com");

        // MQTT endpoint
        Service mqtt_service("id4", "MQTTBroker", "mqtt://broker.example.com:1883");
        CHECK(mqtt_service.getServiceEndpoint() == "mqtt://broker.example.com:1883");
    }

    TEST_CASE("Service serialization round-trip") {
        Service original("did:blockit:abc123#swarm", ServiceType::SwarmCoordinator, "udp://192.168.1.100:9000");

        // Serialize
        auto serialized = dp::serialize<dp::Mode::WITH_VERSION>(original);

        // Deserialize
        auto deserialized = dp::deserialize<dp::Mode::WITH_VERSION, Service>(serialized);

        CHECK(deserialized.getId() == original.getId());
        CHECK(deserialized.getServiceType() == original.getServiceType());
        CHECK(deserialized.getTypeString() == original.getTypeString());
        CHECK(deserialized.getServiceEndpoint() == original.getServiceEndpoint());
    }

    TEST_CASE("Custom service serialization round-trip") {
        Service original("did:blockit:abc123#custom", "RobotTelemetry", "https://api.example.com/telemetry");

        // Serialize
        auto serialized = dp::serialize<dp::Mode::WITH_VERSION>(original);

        // Deserialize
        auto deserialized = dp::deserialize<dp::Mode::WITH_VERSION, Service>(serialized);

        CHECK(deserialized.getId() == original.getId());
        CHECK(deserialized.isCustomType());
        CHECK(deserialized.getTypeString() == "RobotTelemetry");
        CHECK(deserialized.getServiceEndpoint() == original.getServiceEndpoint());
    }

    TEST_CASE("Default service is empty") {
        Service empty;
        CHECK(empty.getId().empty());
        CHECK(empty.getServiceEndpoint().empty());
    }
}
