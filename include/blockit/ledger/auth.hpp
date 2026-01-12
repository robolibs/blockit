#pragma once

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blockit::ledger {

    class Authenticator {
      private:
        std::unordered_set<std::string> authorized_participants_;
        std::unordered_set<std::string> used_transaction_ids_;
        std::unordered_map<std::string, std::string> participant_states_;
        std::unordered_map<std::string, std::vector<std::string>> participant_capabilities_;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> participant_metadata_;

      public:
        Authenticator() = default;

        inline void registerParticipant(const std::string &participant_id,
                                        const std::string &initial_state = "inactive",
                                        const std::unordered_map<std::string, std::string> &metadata = {}) {
            authorized_participants_.insert(participant_id);
            participant_states_[participant_id] = initial_state;
            if (!metadata.empty()) {
                participant_metadata_[participant_id] = metadata;
            }
            std::cout << "Participant " << participant_id << " registered with state: " << initial_state << std::endl;
        }

        inline bool isParticipantAuthorized(const std::string &participant_id) const {
            return authorized_participants_.find(participant_id) != authorized_participants_.end();
        }

        inline std::string getParticipantState(const std::string &participant_id) const {
            auto it = participant_states_.find(participant_id);
            return (it != participant_states_.end()) ? it->second : "unknown";
        }

        inline bool updateParticipantState(const std::string &participant_id, const std::string &new_state) {
            if (!isParticipantAuthorized(participant_id)) {
                std::cout << "Unauthorized participant: " << participant_id << " cannot update state" << std::endl;
                return false;
            }
            participant_states_[participant_id] = new_state;
            return true;
        }

        inline std::string getParticipantMetadata(const std::string &participant_id, const std::string &key) const {
            auto participant_it = participant_metadata_.find(participant_id);
            if (participant_it == participant_metadata_.end())
                return "";

            auto metadata_it = participant_it->second.find(key);
            return (metadata_it != participant_it->second.end()) ? metadata_it->second : "";
        }

        inline void setParticipantMetadata(const std::string &participant_id, const std::string &key,
                                           const std::string &value) {
            if (isParticipantAuthorized(participant_id)) {
                participant_metadata_[participant_id][key] = value;
            }
        }

        inline bool isTransactionUsed(const std::string &tx_id) const {
            return used_transaction_ids_.find(tx_id) != used_transaction_ids_.end();
        }

        inline void markTransactionUsed(const std::string &tx_id) { used_transaction_ids_.insert(tx_id); }

        inline void grantCapability(const std::string &participant_id, const std::string &capability) {
            if (isParticipantAuthorized(participant_id)) {
                participant_capabilities_[participant_id].push_back(capability);
            }
        }

        inline void revokeCapability(const std::string &participant_id, const std::string &capability) {
            if (isParticipantAuthorized(participant_id)) {
                auto &capabilities = participant_capabilities_[participant_id];
                capabilities.erase(std::remove(capabilities.begin(), capabilities.end(), capability),
                                   capabilities.end());
            }
        }

        inline bool hasCapability(const std::string &participant_id, const std::string &capability) const {
            auto it = participant_capabilities_.find(participant_id);
            if (it == participant_capabilities_.end())
                return false;

            const auto &capabilities = it->second;
            return std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end();
        }

        inline bool validateAndRecordAction(const std::string &issuer_participant,
                                            const std::string &action_description, const std::string &tx_id,
                                            const std::string &required_capability = "") {
            if (isTransactionUsed(tx_id)) {
                std::cout << "Duplicate action detected: Transaction " << tx_id << " already recorded" << std::endl;
                return false;
            }

            if (!isParticipantAuthorized(issuer_participant)) {
                std::cout << "Unauthorized participant: " << issuer_participant << " cannot perform actions"
                          << std::endl;
                return false;
            }

            if (!required_capability.empty() && !hasCapability(issuer_participant, required_capability)) {
                std::cout << "Participant " << issuer_participant << " lacks capability: " << required_capability
                          << std::endl;
                return false;
            }

            markTransactionUsed(tx_id);
            std::cout << "Action recorded by " << issuer_participant << ": " << action_description << std::endl;
            return true;
        }

        inline std::unordered_set<std::string> getAuthorizedParticipants() const { return authorized_participants_; }

        inline std::vector<std::string> getParticipantCapabilities(const std::string &participant_id) const {
            auto it = participant_capabilities_.find(participant_id);
            return (it != participant_capabilities_.end()) ? it->second : std::vector<std::string>{};
        }

        inline void printSystemSummary() const {
            std::cout << "=== Authenticator Summary ===" << std::endl;
            std::cout << "Authorized Participants (" << authorized_participants_.size() << "):" << std::endl;
            for (const auto &participant : authorized_participants_) {
                std::cout << "  " << participant << " (state: " << getParticipantState(participant) << ")" << std::endl;
                auto cap_it = participant_capabilities_.find(participant);
                if (cap_it != participant_capabilities_.end() && !cap_it->second.empty()) {
                    std::cout << "    Capabilities: ";
                    for (const auto &cap : cap_it->second) {
                        std::cout << cap << " ";
                    }
                    std::cout << std::endl;
                }
                auto meta_it = participant_metadata_.find(participant);
                if (meta_it != participant_metadata_.end() && !meta_it->second.empty()) {
                    std::cout << "    Metadata: ";
                    for (const auto &meta_pair : meta_it->second) {
                        std::cout << meta_pair.first << "=" << meta_pair.second << " ";
                    }
                    std::cout << std::endl;
                }
            }
            std::cout << "Recorded Actions: " << used_transaction_ids_.size() << std::endl;
        }

        inline std::string serialize() const {
            std::stringstream ss;
            ss << R"({)";
            ss << R"("authorized_participants": [)";
            bool first = true;
            for (const auto &participant : authorized_participants_) {
                if (!first)
                    ss << ",";
                ss << "\"" << participant << "\"";
                first = false;
            }
            ss << R"(],)";
            ss << R"("used_transaction_ids": [)";
            first = true;
            for (const auto &tx_id : used_transaction_ids_) {
                if (!first)
                    ss << ",";
                ss << "\"" << tx_id << "\"";
                first = false;
            }
            ss << R"(],)";
            ss << R"("participant_states": {)";
            first = true;
            for (const auto &[participant, state] : participant_states_) {
                if (!first)
                    ss << ",";
                ss << "\"" << participant << "\": \"" << state << "\"";
                first = false;
            }
            ss << R"(},)";
            ss << R"("participant_capabilities": {)";
            first = true;
            for (const auto &[participant, capabilities] : participant_capabilities_) {
                if (!first)
                    ss << ",";
                ss << "\"" << participant << "\": [";
                bool first_cap = true;
                for (const auto &cap : capabilities) {
                    if (!first_cap)
                        ss << ",";
                    ss << "\"" << cap << "\"";
                    first_cap = false;
                }
                ss << "]";
                first = false;
            }
            ss << R"(},)";
            ss << R"("participant_metadata": {)";
            first = true;
            for (const auto &[participant, metadata] : participant_metadata_) {
                if (!first)
                    ss << ",";
                ss << "\"" << participant << "\": {";
                bool first_meta = true;
                for (const auto &[key, value] : metadata) {
                    if (!first_meta)
                        ss << ",";
                    ss << "\"" << key << "\": \"" << value << "\"";
                    first_meta = false;
                }
                ss << "}";
                first = false;
            }
            ss << R"(})";
            ss << R"(})";
            return ss.str();
        }

        inline static Authenticator deserialize(const std::string &data) {
            Authenticator result;

            size_t auth_start = data.find("\"authorized_participants\": [") + 29;
            size_t auth_end = data.find("],", auth_start);
            std::string auth_data = data.substr(auth_start, auth_end - auth_start);
            size_t pos = 0;
            while (pos < auth_data.length()) {
                size_t quote_start = auth_data.find("\"", pos);
                if (quote_start == std::string::npos)
                    break;
                size_t quote_end = auth_data.find("\"", quote_start + 1);
                if (quote_end == std::string::npos)
                    break;
                std::string participant = auth_data.substr(quote_start + 1, quote_end - quote_start - 1);
                result.authorized_participants_.insert(participant);
                pos = quote_end + 1;
            }

            size_t tx_start = data.find("\"used_transaction_ids\": [") + 26;
            size_t tx_end = data.find("],", tx_start);
            std::string tx_data = data.substr(tx_start, tx_end - tx_start);
            pos = 0;
            while (pos < tx_data.length()) {
                size_t quote_start = tx_data.find("\"", pos);
                if (quote_start == std::string::npos)
                    break;
                size_t quote_end = tx_data.find("\"", quote_start + 1);
                if (quote_end == std::string::npos)
                    break;
                std::string tx_id = tx_data.substr(quote_start + 1, quote_end - quote_start - 1);
                result.used_transaction_ids_.insert(tx_id);
                pos = quote_end + 1;
            }

            size_t states_start = data.find("\"participant_states\": {") + 24;
            size_t states_end = data.find("},", states_start);
            std::string states_data = data.substr(states_start, states_end - states_start);
            pos = 0;
            while (pos < states_data.length()) {
                size_t key_start = states_data.find("\"", pos);
                if (key_start == std::string::npos)
                    break;
                size_t key_end = states_data.find("\"", key_start + 1);
                if (key_end == std::string::npos)
                    break;
                size_t val_start = states_data.find("\"", key_end + 2);
                if (val_start == std::string::npos)
                    break;
                size_t val_end = states_data.find("\"", val_start + 1);
                if (val_end == std::string::npos)
                    break;
                std::string key = states_data.substr(key_start + 1, key_end - key_start - 1);
                std::string value = states_data.substr(val_start + 1, val_end - val_start - 1);
                result.participant_states_[key] = value;
                pos = val_end + 1;
            }

            return result;
        }
    };

    using EntityManager = Authenticator;
    using LedgerManager = Authenticator;
    using DeviceManager = Authenticator;
    using AuthorizationManager = Authenticator;

} // namespace blockit::ledger
