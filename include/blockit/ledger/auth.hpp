#pragma once

#include <algorithm>
#include <datapod/datapod.hpp>
#include <iostream>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <blockit/common/error.hpp>

namespace blockit::ledger {

    class Authenticator {
      private:
        dp::Set<dp::String> authorized_participants_{};
        dp::Set<dp::String> used_transaction_ids_{};
        dp::Map<dp::String, dp::String> participant_states_{};
        dp::Map<dp::String, dp::Vector<dp::String>> participant_capabilities_{};
        dp::Map<dp::String, dp::Map<dp::String, dp::String>> participant_metadata_{};
        mutable std::shared_mutex mutex_;

      public:
        Authenticator() = default;

        // Copy constructor (copies data, creates new mutex)
        Authenticator(const Authenticator &other) {
            std::shared_lock lock(other.mutex_);
            authorized_participants_ = other.authorized_participants_;
            used_transaction_ids_ = other.used_transaction_ids_;
            participant_states_ = other.participant_states_;
            participant_capabilities_ = other.participant_capabilities_;
            participant_metadata_ = other.participant_metadata_;
        }

        // Copy assignment
        Authenticator &operator=(const Authenticator &other) {
            if (this != &other) {
                std::unique_lock lock1(mutex_, std::defer_lock);
                std::shared_lock lock2(other.mutex_, std::defer_lock);
                std::lock(lock1, lock2);
                authorized_participants_ = other.authorized_participants_;
                used_transaction_ids_ = other.used_transaction_ids_;
                participant_states_ = other.participant_states_;
                participant_capabilities_ = other.participant_capabilities_;
                participant_metadata_ = other.participant_metadata_;
            }
            return *this;
        }

        // Move constructor
        Authenticator(Authenticator &&other) noexcept {
            std::unique_lock lock(other.mutex_);
            authorized_participants_ = std::move(other.authorized_participants_);
            used_transaction_ids_ = std::move(other.used_transaction_ids_);
            participant_states_ = std::move(other.participant_states_);
            participant_capabilities_ = std::move(other.participant_capabilities_);
            participant_metadata_ = std::move(other.participant_metadata_);
        }

        // Move assignment
        Authenticator &operator=(Authenticator &&other) noexcept {
            if (this != &other) {
                std::unique_lock lock1(mutex_, std::defer_lock);
                std::unique_lock lock2(other.mutex_, std::defer_lock);
                std::lock(lock1, lock2);
                authorized_participants_ = std::move(other.authorized_participants_);
                used_transaction_ids_ = std::move(other.used_transaction_ids_);
                participant_states_ = std::move(other.participant_states_);
                participant_capabilities_ = std::move(other.participant_capabilities_);
                participant_metadata_ = std::move(other.participant_metadata_);
            }
            return *this;
        }

        auto members() {
            return std::tie(authorized_participants_, used_transaction_ids_, participant_states_,
                            participant_capabilities_, participant_metadata_);
        }
        auto members() const {
            return std::tie(authorized_participants_, used_transaction_ids_, participant_states_,
                            participant_capabilities_, participant_metadata_);
        }

        inline dp::Result<void, dp::Error>
        registerParticipant(const std::string &participant_id, const std::string &initial_state = "inactive",
                            const std::unordered_map<std::string, std::string> &metadata = {}) {
            std::unique_lock lock(mutex_);
            authorized_participants_.insert(dp::String(participant_id.c_str()));
            participant_states_.insert({dp::String(participant_id.c_str()), dp::String(initial_state.c_str())});
            if (!metadata.empty()) {
                dp::Map<dp::String, dp::String> meta_map;
                for (const auto &[key, value] : metadata) {
                    meta_map.insert({dp::String(key.c_str()), dp::String(value.c_str())});
                }
                participant_metadata_.insert({dp::String(participant_id.c_str()), std::move(meta_map)});
            }
            return dp::Result<void, dp::Error>::ok();
        }

        inline bool isParticipantAuthorized(const std::string &participant_id) const {
            std::shared_lock lock(mutex_);
            return authorized_participants_.find(dp::String(participant_id.c_str())) != authorized_participants_.end();
        }

        inline dp::Result<std::string, dp::Error> getParticipantState(const std::string &participant_id) const {
            std::shared_lock lock(mutex_);
            auto it = participant_states_.find(dp::String(participant_id.c_str()));
            if (it == participant_states_.end()) {
                return dp::Result<std::string, dp::Error>::err(dp::Error::not_found("Participant not found"));
            }
            return dp::Result<std::string, dp::Error>::ok(std::string(it->second.c_str()));
        }

        inline dp::Result<void, dp::Error> updateParticipantState(const std::string &participant_id,
                                                                  const std::string &new_state) {
            std::unique_lock lock(mutex_);
            if (authorized_participants_.find(dp::String(participant_id.c_str())) == authorized_participants_.end()) {
                return dp::Result<void, dp::Error>::err(unauthorized("Unauthorized participant cannot update state"));
            }
            participant_states_[dp::String(participant_id.c_str())] = dp::String(new_state.c_str());
            return dp::Result<void, dp::Error>::ok();
        }

        inline dp::Result<std::string, dp::Error> getParticipantMetadata(const std::string &participant_id,
                                                                         const std::string &key) const {
            std::shared_lock lock(mutex_);
            auto participant_it = participant_metadata_.find(dp::String(participant_id.c_str()));
            if (participant_it == participant_metadata_.end()) {
                return dp::Result<std::string, dp::Error>::err(dp::Error::not_found("Participant metadata not found"));
            }

            auto metadata_it = participant_it->second.find(dp::String(key.c_str()));
            if (metadata_it == participant_it->second.end()) {
                return dp::Result<std::string, dp::Error>::err(dp::Error::not_found("Metadata key not found"));
            }
            return dp::Result<std::string, dp::Error>::ok(std::string(metadata_it->second.c_str()));
        }

        inline dp::Result<void, dp::Error> setParticipantMetadata(const std::string &participant_id,
                                                                  const std::string &key, const std::string &value) {
            std::unique_lock lock(mutex_);
            if (authorized_participants_.find(dp::String(participant_id.c_str())) == authorized_participants_.end()) {
                return dp::Result<void, dp::Error>::err(unauthorized("Unauthorized participant cannot set metadata"));
            }
            auto it = participant_metadata_.find(dp::String(participant_id.c_str()));
            if (it == participant_metadata_.end()) {
                dp::Map<dp::String, dp::String> meta_map;
                meta_map[dp::String(key.c_str())] = dp::String(value.c_str());
                participant_metadata_[dp::String(participant_id.c_str())] = std::move(meta_map);
            } else {
                it->second[dp::String(key.c_str())] = dp::String(value.c_str());
            }
            return dp::Result<void, dp::Error>::ok();
        }

        inline bool isTransactionUsed(const std::string &tx_id) const {
            std::shared_lock lock(mutex_);
            return used_transaction_ids_.find(dp::String(tx_id.c_str())) != used_transaction_ids_.end();
        }

        inline dp::Result<void, dp::Error> markTransactionUsed(const std::string &tx_id) {
            std::unique_lock lock(mutex_);
            if (used_transaction_ids_.find(dp::String(tx_id.c_str())) != used_transaction_ids_.end()) {
                return dp::Result<void, dp::Error>::err(duplicate_tx("Transaction already used"));
            }
            used_transaction_ids_.insert(dp::String(tx_id.c_str()));
            return dp::Result<void, dp::Error>::ok();
        }

        inline dp::Result<void, dp::Error> grantCapability(const std::string &participant_id,
                                                           const std::string &capability) {
            std::unique_lock lock(mutex_);
            if (authorized_participants_.find(dp::String(participant_id.c_str())) == authorized_participants_.end()) {
                return dp::Result<void, dp::Error>::err(
                    unauthorized("Unauthorized participant cannot be granted capabilities"));
            }
            auto it = participant_capabilities_.find(dp::String(participant_id.c_str()));
            if (it == participant_capabilities_.end()) {
                dp::Vector<dp::String> caps;
                caps.push_back(dp::String(capability.c_str()));
                participant_capabilities_.insert({dp::String(participant_id.c_str()), std::move(caps)});
            } else {
                // Only add if not already present (prevent duplicates)
                bool found = false;
                for (const auto &cap : it->second) {
                    if (std::string(cap.c_str()) == capability) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    it->second.push_back(dp::String(capability.c_str()));
                }
            }
            return dp::Result<void, dp::Error>::ok();
        }

        inline dp::Result<void, dp::Error> revokeCapability(const std::string &participant_id,
                                                            const std::string &capability) {
            std::unique_lock lock(mutex_);
            if (authorized_participants_.find(dp::String(participant_id.c_str())) == authorized_participants_.end()) {
                return dp::Result<void, dp::Error>::err(unauthorized("Unauthorized participant"));
            }
            auto it = participant_capabilities_.find(dp::String(participant_id.c_str()));
            if (it == participant_capabilities_.end()) {
                return dp::Result<void, dp::Error>::err(dp::Error::not_found("Capability not found"));
            }
            auto &capabilities = it->second;
            for (size_t i = 0; i < capabilities.size(); ++i) {
                if (std::string(capabilities[i].c_str()) == capability) {
                    capabilities.erase(capabilities.begin() + i);
                    return dp::Result<void, dp::Error>::ok();
                }
            }
            return dp::Result<void, dp::Error>::err(dp::Error::not_found("Capability not found"));
        }

        inline bool hasCapability(const std::string &participant_id, const std::string &capability) const {
            std::shared_lock lock(mutex_);
            auto it = participant_capabilities_.find(dp::String(participant_id.c_str()));
            if (it == participant_capabilities_.end())
                return false;

            const auto &capabilities = it->second;
            for (const auto &cap : capabilities) {
                if (std::string(cap.c_str()) == capability)
                    return true;
            }
            return false;
        }

        inline dp::Result<void, dp::Error> validateAndRecordAction(const std::string &issuer_participant,
                                                                   const std::string &action_description,
                                                                   const std::string &tx_id,
                                                                   const std::string &required_capability = "") {
            std::unique_lock lock(mutex_);

            if (used_transaction_ids_.find(dp::String(tx_id.c_str())) != used_transaction_ids_.end()) {
                return dp::Result<void, dp::Error>::err(duplicate_tx("Transaction already recorded"));
            }

            if (authorized_participants_.find(dp::String(issuer_participant.c_str())) ==
                authorized_participants_.end()) {
                return dp::Result<void, dp::Error>::err(
                    unauthorized("Unauthorized participant cannot perform actions"));
            }

            if (!required_capability.empty()) {
                auto cap_it = participant_capabilities_.find(dp::String(issuer_participant.c_str()));
                if (cap_it == participant_capabilities_.end()) {
                    return dp::Result<void, dp::Error>::err(
                        capability_missing("Participant lacks required capability"));
                }
                bool found = false;
                for (const auto &cap : cap_it->second) {
                    if (std::string(cap.c_str()) == required_capability) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return dp::Result<void, dp::Error>::err(
                        capability_missing("Participant lacks required capability"));
                }
            }

            used_transaction_ids_.insert(dp::String(tx_id.c_str()));
            (void)action_description; // Used for logging in original, kept for API compatibility
            return dp::Result<void, dp::Error>::ok();
        }

        inline std::unordered_set<std::string> getAuthorizedParticipants() const {
            std::shared_lock lock(mutex_);
            std::unordered_set<std::string> result;
            for (const auto &p : authorized_participants_) {
                result.insert(std::string(p.c_str()));
            }
            return result;
        }

        inline std::vector<std::string> getParticipantCapabilities(const std::string &participant_id) const {
            std::shared_lock lock(mutex_);
            auto it = participant_capabilities_.find(dp::String(participant_id.c_str()));
            if (it == participant_capabilities_.end())
                return {};
            std::vector<std::string> result;
            for (const auto &cap : it->second) {
                result.push_back(std::string(cap.c_str()));
            }
            return result;
        }

        inline void printSystemSummary() const {
            std::shared_lock lock(mutex_);
            std::cout << "=== Authenticator Summary ===" << std::endl;
            std::cout << "Authorized Participants (" << authorized_participants_.size() << "):" << std::endl;
            for (const auto &participant : authorized_participants_) {
                auto state_it = participant_states_.find(participant);
                std::string state =
                    (state_it != participant_states_.end()) ? std::string(state_it->second.c_str()) : "unknown";
                std::cout << "  " << participant.c_str() << " (state: " << state << ")" << std::endl;
                auto cap_it = participant_capabilities_.find(participant);
                if (cap_it != participant_capabilities_.end() && !cap_it->second.empty()) {
                    std::cout << "    Capabilities: ";
                    for (const auto &cap : cap_it->second) {
                        std::cout << cap.c_str() << " ";
                    }
                    std::cout << std::endl;
                }
                auto meta_it = participant_metadata_.find(participant);
                if (meta_it != participant_metadata_.end() && !meta_it->second.empty()) {
                    std::cout << "    Metadata: ";
                    for (const auto &meta_pair : meta_it->second) {
                        std::cout << meta_pair.first.c_str() << "=" << meta_pair.second.c_str() << " ";
                    }
                    std::cout << std::endl;
                }
            }
            std::cout << "Recorded Actions: " << used_transaction_ids_.size() << std::endl;
        }

        // Serialize to binary using datapod
        inline dp::ByteBuf serialize() const {
            std::shared_lock lock(mutex_);
            auto &self = const_cast<Authenticator &>(*this);
            return dp::serialize<dp::Mode::WITH_VERSION>(self);
        }

        // Deserialize from binary using datapod
        static dp::Result<Authenticator, dp::Error> deserialize(const dp::ByteBuf &data) {
            try {
                auto result = dp::deserialize<dp::Mode::WITH_VERSION, Authenticator>(data);
                return dp::Result<Authenticator, dp::Error>::ok(std::move(result));
            } catch (const std::exception &e) {
                return dp::Result<Authenticator, dp::Error>::err(deserialization_failed(dp::String(e.what())));
            }
        }
    };

    using EntityManager = Authenticator;
    using LedgerManager = Authenticator;
    using DeviceManager = Authenticator;
    using AuthorizationManager = Authenticator;

} // namespace blockit::ledger
