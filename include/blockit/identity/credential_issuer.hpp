#pragma once

#include "credential_status.hpp"
#include "did_registry.hpp"
#include "verifiable_credential.hpp"
#include <chrono>

namespace blockit {

    /// Credential Issuer - issues and manages verifiable credentials
    class CredentialIssuer {
      public:
        CredentialIssuer() = default;

        /// Create an issuer with DID and private key
        CredentialIssuer(const DID &issuer_did, const Key &issuer_key)
            : issuer_did_(issuer_did), issuer_key_(issuer_key), key_id_(issuer_did.withFragment("key-1")) {}

        /// Create an issuer with custom key ID
        CredentialIssuer(const DID &issuer_did, const Key &issuer_key, const std::string &key_id)
            : issuer_did_(issuer_did), issuer_key_(issuer_key), key_id_(key_id) {}

        // === Factory Methods for Common Credential Types ===

        /// Issue a robot authorization credential
        inline dp::Result<VerifiableCredential, dp::Error>
        issueRobotAuthorization(const DID &robot_did, const std::string &robot_id,
                                const std::vector<std::string> &capabilities,
                                std::chrono::milliseconds validity_duration = std::chrono::hours(24 * 365)) {
            // Generate unique ID
            auto cred_id = generateCredentialId("robot-auth");

            auto vc = VerifiableCredential::create(cred_id, CredentialType::RobotAuthorization, issuer_did_, robot_did);

            // Set claims
            vc.setClaim("robot_id", robot_id);

            // Join capabilities into comma-separated string
            std::string caps_str;
            for (size_t i = 0; i < capabilities.size(); i++) {
                if (i > 0)
                    caps_str += ",";
                caps_str += capabilities[i];
            }
            vc.setClaim("capabilities", caps_str);

            // Set expiration
            if (validity_duration.count() > 0) {
                vc.setExpiresIn(validity_duration);
            }

            // Sign
            auto sign_result = vc.sign(issuer_key_, key_id_);
            if (!sign_result.is_ok()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(sign_result.error());
            }

            return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(vc));
        }

        /// Issue a capability grant credential
        inline dp::Result<VerifiableCredential, dp::Error>
        issueCapabilityGrant(const DID &subject_did, const std::string &capability, const std::string &resource,
                             std::chrono::milliseconds validity_duration = std::chrono::hours(24)) {
            auto cred_id = generateCredentialId("capability");

            auto vc = VerifiableCredential::create(cred_id, CredentialType::CapabilityGrant, issuer_did_, subject_did);

            vc.setClaim("capability", capability);
            vc.setClaim("resource", resource);

            if (validity_duration.count() > 0) {
                vc.setExpiresIn(validity_duration);
            }

            auto sign_result = vc.sign(issuer_key_, key_id_);
            if (!sign_result.is_ok()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(sign_result.error());
            }

            return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(vc));
        }

        /// Issue a zone access credential
        inline dp::Result<VerifiableCredential, dp::Error>
        issueZoneAccess(const DID &robot_did, const std::string &zone_id,
                        std::chrono::milliseconds validity_duration = std::chrono::hours(8)) {
            auto cred_id = generateCredentialId("zone-access");

            auto vc = VerifiableCredential::create(cred_id, CredentialType::ZoneAccess, issuer_did_, robot_did);

            vc.setClaim("zone_id", zone_id);

            if (validity_duration.count() > 0) {
                vc.setExpiresIn(validity_duration);
            }

            auto sign_result = vc.sign(issuer_key_, key_id_);
            if (!sign_result.is_ok()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(sign_result.error());
            }

            return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(vc));
        }

        /// Issue a task certification credential
        inline dp::Result<VerifiableCredential, dp::Error>
        issueTaskCertification(const DID &robot_did, const std::string &task_id, const std::string &task_type,
                               const std::string &result = "completed") {
            auto cred_id = generateCredentialId("task-cert");

            auto vc = VerifiableCredential::create(cred_id, CredentialType::TaskCertification, issuer_did_, robot_did);

            vc.setClaim("task_id", task_id);
            vc.setClaim("task_type", task_type);
            vc.setClaim("result", result);

            // Certification timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
            vc.setClaim("certified_at", ss.str());

            // Task certifications don't expire by default
            auto sign_result = vc.sign(issuer_key_, key_id_);
            if (!sign_result.is_ok()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(sign_result.error());
            }

            return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(vc));
        }

        /// Issue a sensor calibration certificate
        inline dp::Result<VerifiableCredential, dp::Error>
        issueSensorCalibration(const DID &robot_did, const std::string &sensor_type, const std::string &sensor_id,
                               const std::map<std::string, std::string> &calibration_data,
                               std::chrono::milliseconds validity_duration = std::chrono::hours(24 * 30)) {
            auto cred_id = generateCredentialId("sensor-cal");

            auto vc = VerifiableCredential::create(cred_id, CredentialType::SensorCalibration, issuer_did_, robot_did);

            vc.setClaim("sensor_type", sensor_type);
            vc.setClaim("sensor_id", sensor_id);

            // Add calibration data claims
            for (const auto &[key, value] : calibration_data) {
                vc.setClaim("cal_" + key, value);
            }

            // Calibration timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
            vc.setClaim("calibrated_at", ss.str());

            if (validity_duration.count() > 0) {
                vc.setExpiresIn(validity_duration);
            }

            auto sign_result = vc.sign(issuer_key_, key_id_);
            if (!sign_result.is_ok()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(sign_result.error());
            }

            return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(vc));
        }

        /// Issue a swarm membership credential
        inline dp::Result<VerifiableCredential, dp::Error>
        issueSwarmMembership(const DID &robot_did, const std::string &swarm_id, const std::string &role = "member",
                             std::chrono::milliseconds validity_duration = std::chrono::hours(24 * 365)) {
            auto cred_id = generateCredentialId("swarm-member");

            auto vc = VerifiableCredential::create(cred_id, CredentialType::SwarmMembership, issuer_did_, robot_did);

            vc.setClaim("swarm_id", swarm_id);
            vc.setClaim("role", role);

            // Membership timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
            vc.setClaim("joined_at", ss.str());

            if (validity_duration.count() > 0) {
                vc.setExpiresIn(validity_duration);
            }

            auto sign_result = vc.sign(issuer_key_, key_id_);
            if (!sign_result.is_ok()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(sign_result.error());
            }

            return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(vc));
        }

        /// Issue a custom credential
        inline dp::Result<VerifiableCredential, dp::Error>
        issueCustomCredential(const DID &subject_did, CredentialType type,
                              const std::map<std::string, std::string> &claims,
                              std::chrono::milliseconds validity_duration = std::chrono::milliseconds(0)) {
            auto cred_id = generateCredentialId("custom");

            auto vc = VerifiableCredential::create(cred_id, type, issuer_did_, subject_did);

            // Add all claims
            for (const auto &[key, value] : claims) {
                vc.setClaim(key, value);
            }

            if (validity_duration.count() > 0) {
                vc.setExpiresIn(validity_duration);
            }

            auto sign_result = vc.sign(issuer_key_, key_id_);
            if (!sign_result.is_ok()) {
                return dp::Result<VerifiableCredential, dp::Error>::err(sign_result.error());
            }

            return dp::Result<VerifiableCredential, dp::Error>::ok(std::move(vc));
        }

        // === Status Management ===

        /// Create a revocation operation for a credential
        inline CredentialOperation createRevokeOperation(const std::string &credential_id,
                                                         const std::string &reason = "") {
            return CredentialOperation::createRevoke(credential_id, issuer_did_.toString(), reason);
        }

        /// Create a suspension operation for a credential
        inline CredentialOperation createSuspendOperation(const std::string &credential_id,
                                                          const std::string &reason = "") {
            return CredentialOperation::createSuspend(credential_id, issuer_did_.toString(), reason);
        }

        /// Create an unsuspend operation for a credential
        inline CredentialOperation createUnsuspendOperation(const std::string &credential_id) {
            return CredentialOperation::createUnsuspend(credential_id, issuer_did_.toString());
        }

        /// Issue credential and record in status list
        inline dp::Result<VerifiableCredential, dp::Error>
        issueAndRecord(CredentialStatusList &status_list, const DID &subject_did, CredentialType type,
                       const std::map<std::string, std::string> &claims,
                       std::chrono::milliseconds validity_duration = std::chrono::milliseconds(0)) {
            auto result = issueCustomCredential(subject_did, type, claims, validity_duration);
            if (result.is_ok()) {
                status_list.recordIssue(result.value().getId(), issuer_did_.toString());
            }
            return result;
        }

        /// Revoke credential in status list
        inline dp::Result<void, dp::Error> revoke(CredentialStatusList &status_list, const std::string &credential_id,
                                                  const std::string &reason = "") {
            return status_list.recordRevoke(credential_id, issuer_did_.toString(), reason);
        }

        /// Suspend credential in status list
        inline dp::Result<void, dp::Error> suspend(CredentialStatusList &status_list, const std::string &credential_id,
                                                   const std::string &reason = "") {
            return status_list.recordSuspend(credential_id, issuer_did_.toString(), reason);
        }

        /// Unsuspend credential in status list
        inline dp::Result<void, dp::Error> unsuspend(CredentialStatusList &status_list,
                                                     const std::string &credential_id) {
            return status_list.recordUnsuspend(credential_id, issuer_did_.toString());
        }

        // === Getters ===

        /// Get issuer DID
        inline DID getIssuerDID() const { return issuer_did_; }

        /// Get issuer DID as string
        inline std::string getIssuerDIDString() const { return issuer_did_.toString(); }

        /// Get key ID used for signing
        inline std::string getKeyId() const { return key_id_; }

        /// Set key ID
        inline void setKeyId(const std::string &key_id) { key_id_ = key_id; }

      private:
        DID issuer_did_;
        Key issuer_key_;
        std::string key_id_;

        /// Generate a unique credential ID
        inline std::string generateCredentialId(const std::string &prefix) const {
            auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
            return "urn:uuid:" + prefix + "-" + std::to_string(now);
        }
    };

} // namespace blockit
