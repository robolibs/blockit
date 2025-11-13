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

        void registerParticipant(const std::string &participant_id, const std::string &initial_state = "inactive",
                                 const std::unordered_map<std::string, std::string> &metadata = {});

        bool isParticipantAuthorized(const std::string &participant_id) const;

        std::string getParticipantState(const std::string &participant_id) const;

        bool updateParticipantState(const std::string &participant_id, const std::string &new_state);

        std::string getParticipantMetadata(const std::string &participant_id, const std::string &key) const;

        void setParticipantMetadata(const std::string &participant_id, const std::string &key,
                                    const std::string &value);

        bool isTransactionUsed(const std::string &tx_id) const;

        void markTransactionUsed(const std::string &tx_id);

        void grantCapability(const std::string &participant_id, const std::string &capability);

        void revokeCapability(const std::string &participant_id, const std::string &capability);

        bool hasCapability(const std::string &participant_id, const std::string &capability) const;

        bool validateAndRecordAction(const std::string &issuer_participant, const std::string &action_description,
                                     const std::string &tx_id, const std::string &required_capability = "");

        std::unordered_set<std::string> getAuthorizedParticipants() const;

        std::vector<std::string> getParticipantCapabilities(const std::string &participant_id) const;

        void printSystemSummary() const;

        std::string serialize() const;

        static Authenticator deserialize(const std::string &data);
    };

    using EntityManager = Authenticator;
    using LedgerManager = Authenticator;
    using DeviceManager = Authenticator;
    using AuthorizationManager = Authenticator;

} // namespace blockit::ledger
