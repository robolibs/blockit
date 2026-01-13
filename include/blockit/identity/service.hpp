#pragma once

#include <datapod/datapod.hpp>
#include <string>

namespace blockit {

    /// Service types for robot swarms
    enum class ServiceType : dp::u8 {
        SwarmCoordinator = 0,     // Robot swarm coordination endpoint
        SensorDataEndpoint = 1,   // Sensor data streaming
        TaskQueue = 2,            // Task distribution service
        CredentialRepository = 3, // Verifiable credentials storage
        DIDCommMessaging = 4,     // DIDComm v2 messaging
        LinkedDomains = 5,        // Domain linkage
        Custom = 255,             // Custom service type
    };

    /// Get string name for service type
    inline std::string serviceTypeToString(ServiceType type) {
        switch (type) {
        case ServiceType::SwarmCoordinator:
            return "SwarmCoordinator";
        case ServiceType::SensorDataEndpoint:
            return "SensorDataEndpoint";
        case ServiceType::TaskQueue:
            return "TaskQueue";
        case ServiceType::CredentialRepository:
            return "CredentialRepository";
        case ServiceType::DIDCommMessaging:
            return "DIDCommMessaging";
        case ServiceType::LinkedDomains:
            return "LinkedDomains";
        case ServiceType::Custom:
            return "Custom";
        default:
            return "Unknown";
        }
    }

    /// A service endpoint for communication with the DID subject
    /// Following W3C DID Core v1.0 specification
    struct Service {
        dp::String id;               // e.g., "did:blockit:xxx#swarm"
        dp::u8 type{0};              // ServiceType
        dp::String type_name;        // Custom type name (if type == Custom)
        dp::String service_endpoint; // URI or network address

        Service() = default;

        /// Create a service with predefined type
        inline Service(const std::string &id, ServiceType type, const std::string &endpoint)
            : id(dp::String(id.c_str())), type(static_cast<dp::u8>(type)), type_name(),
              service_endpoint(dp::String(endpoint.c_str())) {}

        /// Create a service with custom type
        inline Service(const std::string &id, const std::string &custom_type, const std::string &endpoint)
            : id(dp::String(id.c_str())), type(static_cast<dp::u8>(ServiceType::Custom)),
              type_name(dp::String(custom_type.c_str())), service_endpoint(dp::String(endpoint.c_str())) {}

        /// Get the ID as string
        inline std::string getId() const { return std::string(id.c_str()); }

        /// Get the type as enum
        inline ServiceType getServiceType() const { return static_cast<ServiceType>(type); }

        /// Get type as string
        inline std::string getTypeString() const {
            if (type == static_cast<dp::u8>(ServiceType::Custom)) {
                return std::string(type_name.c_str());
            }
            return serviceTypeToString(getServiceType());
        }

        /// Get service endpoint as string
        inline std::string getServiceEndpoint() const { return std::string(service_endpoint.c_str()); }

        /// Check if this is a custom type
        inline bool isCustomType() const { return type == static_cast<dp::u8>(ServiceType::Custom); }

        /// Equality comparison
        inline bool operator==(const Service &other) const {
            return std::string(id.c_str()) == std::string(other.id.c_str());
        }

        inline bool operator!=(const Service &other) const { return !(*this == other); }

        /// Serialization
        auto members() { return std::tie(id, type, type_name, service_endpoint); }
        auto members() const { return std::tie(id, type, type_name, service_endpoint); }
    };

} // namespace blockit
