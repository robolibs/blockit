#pragma once

#include <blockit/key.hpp>
#include <chrono>
#include <datapod/datapod.hpp>
#include <string>
#include <vector>

namespace blockit {
    namespace ledger {

        /// Validator status for PoA consensus
        enum class ValidatorStatus : dp::u8 {
            ACTIVE = 0,  // Can propose and sign blocks
            OFFLINE = 1, // Not responding (temporary)
            REVOKED = 2  // Permanently removed (manual revocation)
        };

        /// Validator with Key identity for PoA consensus
        /// Header-only implementation
        class Validator {
          public:
            /// Create validator with Key identity
            inline Validator(const std::string &participant_id, const Key &identity, int weight = 1)
                : participant_id_(participant_id), identity_(identity), weight_(weight),
                  status_(ValidatorStatus::ACTIVE), last_seen_(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                   std::chrono::system_clock::now().time_since_epoch())
                                                                   .count()) {}

            /// Get unique ID (hash of public key)
            inline std::string getId() const { return identity_.getId(); }

            /// Get participant name
            inline const std::string &getParticipantId() const { return participant_id_; }

            /// Get identity type (always "key")
            inline std::string getIdentityType() const { return "key"; }

            /// Get the Key identity
            inline const Key &getIdentity() const { return identity_; }

            /// Get weight (for voting)
            inline int getWeight() const { return weight_; }

            /// Set weight
            inline void setWeight(int weight) { weight_ = weight; }

            /// Get status
            inline ValidatorStatus getStatus() const { return status_; }

            /// Set status
            inline void setStatus(ValidatorStatus status) { status_ = status; }

            /// Check if validator can sign (active + valid identity)
            inline bool canSign() const { return status_ == ValidatorStatus::ACTIVE && identity_.isValid(); }

            /// Sign data (delegates to identity)
            inline dp::Result<std::vector<uint8_t>, dp::Error> sign(const std::vector<uint8_t> &data) const {
                if (!canSign()) {
                    return dp::Result<std::vector<uint8_t>, dp::Error>::err(
                        dp::Error::io_error("Validator cannot sign: inactive or invalid identity"));
                }
                return identity_.sign(data);
            }

            /// Verify signature (delegates to identity)
            inline dp::Result<bool, dp::Error> verify(const std::vector<uint8_t> &data,
                                                      const std::vector<uint8_t> &signature) const {
                return identity_.verify(data, signature);
            }

            /// Update last seen timestamp
            inline void updateActivity() {
                last_seen_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
            }

            /// Check if online (based on last activity and timeout)
            inline bool isOnline(int64_t timeout_ms = 60000) const {
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
                return (now_ms - last_seen_) < timeout_ms;
            }

            /// Get last seen timestamp
            inline dp::i64 getLastSeen() const { return last_seen_; }

            /// Mark validator as online (set status to ACTIVE and update activity)
            inline void markOnline() {
                if (status_ == ValidatorStatus::OFFLINE) {
                    status_ = ValidatorStatus::ACTIVE;
                }
                updateActivity();
            }

            /// Mark validator as offline
            inline void markOffline() { status_ = ValidatorStatus::OFFLINE; }

            /// Revoke validator (permanent)
            inline void revokeValidator() { status_ = ValidatorStatus::REVOKED; }

            /// Serialize for storage
            inline std::vector<uint8_t> serialize() const {
                std::vector<dp::u8> result;
                auto identity_data = identity_.serialize();

                // Calculate lengths
                dp::u32 pid_len = static_cast<dp::u32>(participant_id_.length());
                dp::u32 identity_len = static_cast<dp::u32>(identity_data.size());

                result.reserve(1 + 4 + pid_len + 4 + identity_len + 4 + 8);

                // Status (1 byte)
                result.push_back(static_cast<dp::u8>(status_));

                // Participant ID length (4 bytes)
                const dp::u8 *pid_len_bytes = reinterpret_cast<const dp::u8 *>(&pid_len);
                result.insert(result.end(), pid_len_bytes, pid_len_bytes + 4);

                // Participant ID
                result.insert(result.end(), participant_id_.begin(), participant_id_.end());

                // Identity data length (4 bytes)
                const dp::u8 *identity_len_bytes = reinterpret_cast<const dp::u8 *>(&identity_len);
                result.insert(result.end(), identity_len_bytes, identity_len_bytes + 4);

                // Identity data
                result.insert(result.end(), identity_data.begin(), identity_data.end());

                // Weight (4 bytes)
                const dp::u8 *weight_bytes = reinterpret_cast<const dp::u8 *>(&weight_);
                result.insert(result.end(), weight_bytes, weight_bytes + 4);

                // Last seen (8 bytes)
                const dp::u8 *last_seen_bytes = reinterpret_cast<const dp::u8 *>(&last_seen_);
                result.insert(result.end(), last_seen_bytes, last_seen_bytes + 8);

                return result;
            }

            /// Deserialize
            inline static dp::Result<Validator, dp::Error> deserialize(const std::vector<uint8_t> &data) {
                if (data.size() < 9) { // Minimum: 1 (status) + 4 (len) + 4 (len)
                    return dp::Result<Validator, dp::Error>::err(
                        dp::Error::invalid_argument("Invalid serialized validator data"));
                }

                size_t offset = 0;

                // Status (1 byte)
                ValidatorStatus status = static_cast<ValidatorStatus>(data[offset]);
                offset += 1;

                // Participant ID length (4 bytes)
                if (offset + 4 > data.size()) {
                    return dp::Result<Validator, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
                }
                dp::u32 pid_len = *reinterpret_cast<const dp::u32 *>(data.data() + offset);
                offset += 4;

                // Participant ID
                if (offset + pid_len > data.size()) {
                    return dp::Result<Validator, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
                }
                std::string participant_id(data.begin() + offset, data.begin() + offset + pid_len);
                offset += pid_len;

                // Identity data length (4 bytes)
                if (offset + 4 > data.size()) {
                    return dp::Result<Validator, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
                }
                dp::u32 identity_len = *reinterpret_cast<const dp::u32 *>(data.data() + offset);
                offset += 4;

                // Identity data
                if (offset + identity_len > data.size()) {
                    return dp::Result<Validator, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
                }
                std::vector<uint8_t> identity_data(data.begin() + offset, data.begin() + offset + identity_len);
                auto identity_result = Key::deserialize(identity_data);
                if (!identity_result.is_ok()) {
                    return dp::Result<Validator, dp::Error>::err(identity_result.error());
                }
                offset += identity_len;

                // Weight (4 bytes)
                if (offset + 4 > data.size()) {
                    return dp::Result<Validator, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
                }
                int weight = *reinterpret_cast<const int *>(data.data() + offset);
                offset += 4;

                // Last seen (8 bytes)
                if (offset + 8 > data.size()) {
                    return dp::Result<Validator, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
                }
                dp::i64 last_seen = *reinterpret_cast<const dp::i64 *>(data.data() + offset);

                // Create validator and restore state
                Validator validator(participant_id, identity_result.value(), weight);
                validator.status_ = status;
                validator.last_seen_ = last_seen;

                return dp::Result<Validator, dp::Error>::ok(validator);
            }

          private:
            std::string participant_id_;
            Key identity_;
            int weight_;
            ValidatorStatus status_;
            dp::i64 last_seen_;
        };

    } // namespace ledger

    // Backward compatibility
    using ledger::Validator;
    using ledger::ValidatorStatus;

} // namespace blockit
