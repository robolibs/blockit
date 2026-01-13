#pragma once

#include <blockit/ledger/key.hpp>
#include <blockit/ledger/validator.hpp>
#include <chrono>
#include <datapod/datapod.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace blockit {

    /// Validator's signature on a block
    struct BlockSignature {
        dp::String validator_id;
        dp::String participant_id;
        dp::Vector<dp::u8> signature;
        dp::i64 signed_at{0}; // Unix timestamp in milliseconds

        // Default constructor
        BlockSignature() = default;

        // Constructor from std types (for convenience)
        BlockSignature(const std::string &vid, const std::string &pid, const std::vector<uint8_t> &sig, int64_t ts)
            : validator_id(dp::String(vid.c_str())), participant_id(dp::String(pid.c_str())),
              signature(dp::Vector<dp::u8>(sig.begin(), sig.end())), signed_at(ts) {}

        // For datapod serialization
        auto members() { return std::tie(validator_id, participant_id, signature, signed_at); }
        auto members() const { return std::tie(validator_id, participant_id, signature, signed_at); }

        /// Serialize block signature (manual - for compatibility)
        inline std::vector<uint8_t> serialize() const {
            std::vector<dp::u8> result;
            std::string vid(validator_id.c_str());
            std::string pid(participant_id.c_str());

            dp::u32 vid_len = static_cast<dp::u32>(vid.length());
            dp::u32 pid_len = static_cast<dp::u32>(pid.length());
            dp::u32 sig_len = static_cast<dp::u32>(signature.size());

            result.reserve(4 + vid_len + 4 + pid_len + 4 + sig_len + 8);

            // Validator ID length + data
            const dp::u8 *vid_len_bytes = reinterpret_cast<const dp::u8 *>(&vid_len);
            result.insert(result.end(), vid_len_bytes, vid_len_bytes + 4);
            result.insert(result.end(), vid.begin(), vid.end());

            // Participant ID length + data
            const dp::u8 *pid_len_bytes = reinterpret_cast<const dp::u8 *>(&pid_len);
            result.insert(result.end(), pid_len_bytes, pid_len_bytes + 4);
            result.insert(result.end(), pid.begin(), pid.end());

            // Signature length + data
            const dp::u8 *sig_len_bytes = reinterpret_cast<const dp::u8 *>(&sig_len);
            result.insert(result.end(), sig_len_bytes, sig_len_bytes + 4);
            result.insert(result.end(), signature.begin(), signature.end());

            // Signed at timestamp
            const dp::u8 *ts_bytes = reinterpret_cast<const dp::u8 *>(&signed_at);
            result.insert(result.end(), ts_bytes, ts_bytes + 8);

            return result;
        }

        /// Deserialize block signature (manual - for compatibility)
        inline static dp::Result<BlockSignature, dp::Error> deserialize(const std::vector<uint8_t> &data) {
            if (data.size() < 20) {
                return dp::Result<BlockSignature, dp::Error>::err(
                    dp::Error::invalid_argument("Invalid block signature data"));
            }

            size_t offset = 0;
            BlockSignature sig;

            // Validator ID
            dp::u32 vid_len = *reinterpret_cast<const dp::u32 *>(data.data() + offset);
            offset += 4;
            if (offset + vid_len > data.size()) {
                return dp::Result<BlockSignature, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            sig.validator_id = dp::String(std::string(data.begin() + offset, data.begin() + offset + vid_len).c_str());
            offset += vid_len;

            // Participant ID
            if (offset + 4 > data.size()) {
                return dp::Result<BlockSignature, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            dp::u32 pid_len = *reinterpret_cast<const dp::u32 *>(data.data() + offset);
            offset += 4;
            if (offset + pid_len > data.size()) {
                return dp::Result<BlockSignature, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            sig.participant_id =
                dp::String(std::string(data.begin() + offset, data.begin() + offset + pid_len).c_str());
            offset += pid_len;

            // Signature
            if (offset + 4 > data.size()) {
                return dp::Result<BlockSignature, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            dp::u32 sig_len = *reinterpret_cast<const dp::u32 *>(data.data() + offset);
            offset += 4;
            if (offset + sig_len > data.size()) {
                return dp::Result<BlockSignature, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            sig.signature = dp::Vector<dp::u8>(data.begin() + offset, data.begin() + offset + sig_len);
            offset += sig_len;

            // Signed at
            if (offset + 8 > data.size()) {
                return dp::Result<BlockSignature, dp::Error>::err(dp::Error::invalid_argument("Truncated data"));
            }
            sig.signed_at = *reinterpret_cast<const dp::i64 *>(data.data() + offset);

            return dp::Result<BlockSignature, dp::Error>::ok(sig);
        }
    };

    /// PoA consensus configuration
    struct PoAConfig {
        // Quorum settings
        int initial_required_signatures = 2; // Fixed threshold
        int minimum_required_signatures = 1; // Fallback for offline

        // Timing (all configurable)
        int64_t signature_timeout_ms = 30000;  // 30 seconds to collect signatures
        int64_t offline_threshold_ms = 120000; // 120s before marked offline

        // Rate limiting (spam prevention)
        int max_proposals_per_hour = 10;
        int min_seconds_between_proposals = 5;
        double backoff_multiplier = 2.0;

        // Validator management
        bool auto_remove_offline = false;
        int64_t offline_remove_threshold_ms = 3600000; // 1 hour
    };

    /// Proof of Authority consensus engine
    /// Header-only implementation
    class PoAConsensus {
      public:
        inline explicit PoAConsensus(const PoAConfig &config = PoAConfig{}) : config_(config) {}

        // ===========================================
        // Validator Management
        // ===========================================

        /// Register new validator
        inline dp::Result<void, dp::Error> addValidator(const std::string &participant_id, const Key &identity,
                                                        int weight = 1) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto validator_id = identity.getId();

            if (validators_.find(validator_id) != validators_.end()) {
                return dp::Result<void, dp::Error>::err(
                    dp::Error::invalid_argument("Validator with this ID already exists"));
            }

            validators_[validator_id] = std::make_unique<Validator>(participant_id, identity, weight);
            validators_[validator_id]->updateActivity();

            return dp::Result<void, dp::Error>::ok();
        }

        /// Remove validator
        inline dp::Result<void, dp::Error> removeValidator(const std::string &validator_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (validators_.find(validator_id) == validators_.end()) {
                return dp::Result<void, dp::Error>::err(dp::Error::not_found("Validator not found"));
            }

            validators_.erase(validator_id);
            return dp::Result<void, dp::Error>::ok();
        }

        /// Get validator by ID
        inline dp::Result<Validator *, dp::Error> getValidator(const std::string &validator_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = validators_.find(validator_id);
            if (it == validators_.end()) {
                return dp::Result<Validator *, dp::Error>::err(dp::Error::not_found("Validator not found"));
            }

            return dp::Result<Validator *, dp::Error>::ok(it->second.get());
        }

        /// Get validator by participant ID
        inline dp::Result<Validator *, dp::Error> getValidatorByParticipant(const std::string &participant_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto &[id, validator] : validators_) {
                if (validator->getParticipantId() == participant_id) {
                    return dp::Result<Validator *, dp::Error>::ok(validator.get());
                }
            }

            return dp::Result<Validator *, dp::Error>::err(dp::Error::not_found("Validator not found for participant"));
        }

        /// Get all active validators
        inline std::vector<Validator *> getActiveValidators() {
            std::lock_guard<std::mutex> lock(mutex_);

            std::vector<Validator *> result;
            for (auto &[id, validator] : validators_) {
                if (validator->canSign()) {
                    result.push_back(validator.get());
                }
            }
            return result;
        }

        /// Get all validators
        inline std::vector<Validator *> getAllValidators() {
            std::lock_guard<std::mutex> lock(mutex_);

            std::vector<Validator *> result;
            for (auto &[id, validator] : validators_) {
                result.push_back(validator.get());
            }
            return result;
        }

        /// Mark validator as online
        inline void markOnline(const std::string &validator_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = validators_.find(validator_id);
            if (it != validators_.end()) {
                it->second->markOnline();
            }
        }

        /// Mark validator as offline
        inline void markOffline(const std::string &validator_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = validators_.find(validator_id);
            if (it != validators_.end()) {
                it->second->markOffline();
            }
        }

        /// Revoke validator (manual revocation)
        inline void revokeValidator(const std::string &validator_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = validators_.find(validator_id);
            if (it != validators_.end()) {
                it->second->revokeValidator();
            }
        }

        /// Update validator activity
        inline void updateActivity(const std::string &validator_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = validators_.find(validator_id);
            if (it != validators_.end()) {
                it->second->updateActivity();
            }
        }

        // ===========================================
        // Quorum Management
        // ===========================================

        /// Get current required signatures (dynamic based on active validators)
        inline int getRequiredSignatures() const {
            std::lock_guard<std::mutex> lock(mutex_);

            int active_count = 0;
            for (const auto &[id, validator] : validators_) {
                if (validator->canSign()) {
                    active_count++;
                }
            }

            // Dynamic threshold: reduce if not enough active validators
            int required = config_.initial_required_signatures;
            if (active_count < required) {
                required = std::max(config_.minimum_required_signatures, active_count);
            }

            return required;
        }

        /// Get total weight of active validators
        inline int getTotalActiveWeight() const {
            std::lock_guard<std::mutex> lock(mutex_);

            int total = 0;
            for (const auto &[id, validator] : validators_) {
                if (validator->canSign()) {
                    total += validator->getWeight();
                }
            }
            return total;
        }

        /// Get count of active validators
        inline size_t getActiveValidatorCount() const {
            std::lock_guard<std::mutex> lock(mutex_);

            size_t count = 0;
            for (const auto &[id, validator] : validators_) {
                if (validator->canSign()) {
                    count++;
                }
            }
            return count;
        }

        /// Check if block has quorum
        inline bool hasQuorum(const std::vector<BlockSignature> &signatures) const {
            int required = getRequiredSignatures();

            // Count unique validator signatures
            std::unordered_set<std::string> unique_signers;
            for (const auto &sig : signatures) {
                unique_signers.insert(std::string(sig.validator_id.c_str()));
            }

            return static_cast<int>(unique_signers.size()) >= required;
        }

        // ===========================================
        // Rate Limiting
        // ===========================================

        /// Check if validator can propose (rate limit check)
        inline dp::Result<bool, dp::Error> canPropose(const std::string &validator_id, const std::string &reason = "") {
            std::lock_guard<std::mutex> lock(mutex_);

            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

            // Check if validator exists and is active
            auto it = validators_.find(validator_id);
            if (it == validators_.end() || !it->second->canSign()) {
                return dp::Result<bool, dp::Error>::err(
                    dp::Error::invalid_argument("Validator not found or not active"));
            }

            // Get proposal tracker
            auto &tracker = proposal_trackers_[validator_id];

            // Check minimum time between proposals
            dp::i64 time_since_last = now_ms - tracker.last_proposal_time;
            if (tracker.last_proposal_time > 0 && time_since_last < (config_.min_seconds_between_proposals * 1000)) {
                return dp::Result<bool, dp::Error>::err(
                    dp::Error::permission_denied("Too many proposals, backoff in effect"));
            }

            // Check hourly limit
            dp::i64 one_hour_ms = 60 * 60 * 1000;
            if (time_since_last < one_hour_ms && tracker.proposal_count >= config_.max_proposals_per_hour) {
                return dp::Result<bool, dp::Error>::err(dp::Error::permission_denied("Rate limit exceeded"));
            }

            (void)reason; // Future: log or track reason
            return dp::Result<bool, dp::Error>::ok(true);
        }

        /// Record a proposal attempt
        inline void recordProposal(const std::string &validator_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

            auto &tracker = proposal_trackers_[validator_id];

            dp::i64 one_hour_ms = 60 * 60 * 1000;

            // Reset counter if more than 1 hour since last proposal
            if ((now_ms - tracker.last_proposal_time) > one_hour_ms) {
                tracker.proposal_count = 0;
            }

            tracker.last_proposal_time = now_ms;
            tracker.proposal_count++;
        }

        /// Get proposal count for validator in current window
        inline int getProposalCount(const std::string &validator_id) const {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = proposal_trackers_.find(validator_id);
            if (it == proposal_trackers_.end()) {
                return 0;
            }

            return it->second.proposal_count;
        }

        // ===========================================
        // Block Proposal Flow
        // ===========================================

        /// Create new block proposal
        inline std::string createProposal(const std::string &block_hash, const std::string &proposer_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            std::string proposal_id = "proposal_" + block_hash;

            Proposal proposal{proposal_id,
                              block_hash,
                              proposer_id,
                              std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count(),
                              getRequiredSignaturesInternal(),
                              {},
                              {},
                              {}};

            proposals_[proposal_id] = std::move(proposal);

            return proposal_id;
        }

        /// Add signature to proposal
        inline dp::Result<bool, dp::Error> addSignature(const std::string &proposal_id, const std::string &validator_id,
                                                        const std::vector<uint8_t> &signature) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = proposals_.find(proposal_id);
            if (it == proposals_.end()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::not_found("Proposal not found"));
            }

            auto &proposal = it->second;

            // Check if proposal expired
            if (proposal.isExpired(config_.signature_timeout_ms)) {
                return dp::Result<bool, dp::Error>::err(dp::Error::invalid_argument("Proposal has expired"));
            }

            // Check if validator already signed
            if (proposal.signed_validators.find(validator_id) != proposal.signed_validators.end()) {
                return dp::Result<bool, dp::Error>::err(
                    dp::Error::invalid_argument("Validator already signed this proposal"));
            }

            // Check if validator exists and can sign
            auto validator_it = validators_.find(validator_id);
            if (validator_it == validators_.end()) {
                return dp::Result<bool, dp::Error>::err(dp::Error::not_found("Validator not found"));
            }
            if (!validator_it->second->canSign()) {
                return dp::Result<bool, dp::Error>::err(
                    dp::Error::invalid_argument("Validator is not authorized to sign (revoked or offline)"));
            }

            // Store the signature
            std::string participant_id = validator_it->second->getParticipantId();

            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

            proposal.signatures.push_back(BlockSignature{validator_id, participant_id, signature, now_ms});

            // Add to signed set
            proposal.signed_validators.insert(validator_id);
            proposal.pending_validators.erase(validator_id);

            return dp::Result<bool, dp::Error>::ok(proposal.hasQuorum());
        }

        /// Check if proposal is ready (has quorum)
        inline bool isProposalReady(const std::string &proposal_id) const {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = proposals_.find(proposal_id);
            if (it == proposals_.end()) {
                return false;
            }

            return it->second.hasQuorum();
        }

        /// Get finalized signatures for proposal
        inline dp::Result<std::vector<BlockSignature>, dp::Error>
        getFinalizedSignatures(const std::string &proposal_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = proposals_.find(proposal_id);
            if (it == proposals_.end()) {
                return dp::Result<std::vector<BlockSignature>, dp::Error>::err(
                    dp::Error::not_found("Proposal not found"));
            }

            if (!it->second.hasQuorum()) {
                return dp::Result<std::vector<BlockSignature>, dp::Error>::err(
                    dp::Error::invalid_argument("Proposal does not have quorum"));
            }

            return dp::Result<std::vector<BlockSignature>, dp::Error>::ok(it->second.signatures);
        }

        /// Remove proposal
        inline void removeProposal(const std::string &proposal_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            proposals_.erase(proposal_id);
        }

        /// Cleanup expired proposals
        inline void cleanupExpired() {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto it = proposals_.begin(); it != proposals_.end();) {
                if (it->second.isExpired(config_.signature_timeout_ms)) {
                    it = proposals_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        /// Get configuration
        inline const PoAConfig &getConfig() const { return config_; }

        /// Update configuration
        inline void setConfig(const PoAConfig &config) {
            std::lock_guard<std::mutex> lock(mutex_);
            config_ = config;
        }

      private:
        struct Proposal {
            std::string id;
            std::string block_hash;
            std::string proposer_id;
            dp::i64 created_at;
            int required_signatures;

            std::unordered_set<std::string> pending_validators;
            std::unordered_set<std::string> signed_validators;
            std::vector<BlockSignature> signatures;

            inline bool isExpired(int64_t timeout_ms) const {
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
                return (now_ms - created_at) > timeout_ms;
            }

            inline bool hasQuorum() const { return static_cast<int>(signed_validators.size()) >= required_signatures; }
        };

        struct ProposalTracker {
            dp::i64 last_proposal_time = 0;
            int proposal_count = 0;
        };

        // Internal helper that doesn't lock (assumes mutex already held)
        inline int getRequiredSignaturesInternal() const {
            int active_count = 0;
            for (const auto &[id, validator] : validators_) {
                if (validator->canSign()) {
                    active_count++;
                }
            }

            int required = config_.initial_required_signatures;
            if (active_count < required) {
                required = std::max(config_.minimum_required_signatures, active_count);
            }

            return required;
        }

        PoAConfig config_;
        std::unordered_map<std::string, std::unique_ptr<Validator>> validators_;
        std::unordered_map<std::string, Proposal> proposals_;
        std::unordered_map<std::string, ProposalTracker> proposal_trackers_;
        mutable std::mutex mutex_;
    };

} // namespace blockit
