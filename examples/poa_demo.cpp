#include <blockit/blockit.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace blockit;

int main() {
    std::cout << "=== PoA Consensus Demo ===" << std::endl;

    // Example 1: Setup validators
    std::cout << "\n1. Setting up 3 validators..." << std::endl;

    PoAConfig config;
    config.initial_required_signatures = 2;
    config.signature_timeout_ms = 30000; // 30 seconds
    config.offline_threshold_ms = 120000;  // 2 minutes

    PoAConsensus consensus(config);

    // Option 1: Use Key::generate() convenience wrapper
    auto alice_key = Key::generate();
    auto bob_key = Key::generate();

    if (!alice_key.is_ok() || !bob_key.is_ok()) {
        std::cerr << "Failed to generate keys" << std::endl;
        return 1;
    }

    // Option 2: Use keylock directly, then wrap in Key
    keylock::keylock crypto(keylock::Algorithm::Ed25519);
    auto charlie_keypair = crypto.generate_keypair();
    Key charlie_key(charlie_keypair);  // Direct construction from keylock::KeyPair

    std::cout << "   Alice/Bob: using Key::generate()" << std::endl;
    std::cout << "   Charlie: using keylock directly + Key(keypair)" << std::endl;

    consensus.addValidator("alice", alice_key.value());
    consensus.addValidator("bob", bob_key.value());
    consensus.addValidator("charlie", charlie_key);

    std::cout << "   Active validators: " << consensus.getActiveValidatorCount() << std::endl;
    std::cout << "   Required signatures: " << consensus.getRequiredSignatures() << std::endl;

    // Example 2: Create proposal and collect signatures
    std::cout << "\n2. Creating proposal and collecting signatures..." << std::endl;

    auto proposal_id = consensus.createProposal("block_hash_001", "alice");
    std::cout << "   Created proposal: " << proposal_id << std::endl;

    // Alice signs
    auto data = std::vector<uint8_t>{0x01, 0x02, 0x03};
    auto sign_result1 = alice_key.value().sign(data);
    if (!sign_result1.is_ok()) {
        std::cerr << "   Failed to sign: " << sign_result1.error().message.c_str() << std::endl;
        return 1;
    }

    auto add_result1 = consensus.addSignature(proposal_id, alice_key.value().getId(), sign_result1.value());
    std::cout << "   Alice signed (1/2)" << std::endl;
    std::cout << "   Quorum reached: " << (consensus.isProposalReady(proposal_id) ? "yes" : "no") << std::endl;

    // Bob signs
    auto sign_result2 = bob_key.value().sign(data);
    if (!sign_result2.is_ok()) {
        std::cerr << "   Failed to sign: " << sign_result2.error().message.c_str() << std::endl;
        return 1;
    }

    auto add_result2 = consensus.addSignature(proposal_id, bob_key.value().getId(), sign_result2.value());
    std::cout << "   Bob signed (2/2)" << std::endl;
    std::cout << "   Quorum reached: " << (consensus.isProposalReady(proposal_id) ? "yes" : "no") << std::endl;

    // Example 3: Get finalized signatures
    std::cout << "\n3. Getting finalized signatures..." << std::endl;

    auto sigs_result = consensus.getFinalizedSignatures(proposal_id);
    if (!sigs_result.is_ok()) {
        std::cerr << "   Failed to get signatures: " << sigs_result.error().message.c_str() << std::endl;
        return 1;
    }

    const auto& signatures = sigs_result.value();
    std::cout << "   Signatures collected: " << signatures.size() << std::endl;

    for (const auto& sig : signatures) {
        std::cout << "      - Validator: " << sig.validator_id.substr(0, 16) << "..."
                  << ", Participant: " << sig.participant_id << std::endl;
    }

    // Example 4: Test rate limiting
    std::cout << "\n4. Testing rate limiting..." << std::endl;

    consensus.recordProposal(alice_key.value().getId());
    consensus.recordProposal(alice_key.value().getId());

    auto can_propose = consensus.canPropose(alice_key.value().getId());
    std::cout << "   Can propose: " << (can_propose.is_ok() ? "yes" : "no") << std::endl;
    std::cout << "   Proposal count: " << consensus.getProposalCount(alice_key.value().getId()) << std::endl;

    // Example 5: Test offline handling
    std::cout << "\n5. Testing offline handling..." << std::endl;

    auto charlie_result = consensus.getValidator(charlie_key.getId());
    if (charlie_result.is_ok()) {
        std::cout << "   Charlie isOnline: " << (charlie_result.value()->isOnline(120000) ? "yes" : "no") << std::endl;
    }

    consensus.markOffline(charlie_key.getId());
    std::cout << "   Marked Charlie offline" << std::endl;

    charlie_result = consensus.getValidator(charlie_key.getId());
    if (charlie_result.is_ok()) {
        std::cout << "   Charlie status: " << (int)charlie_result.value()->getStatus() << " (1 = OFFLINE)" << std::endl;
    }

    std::cout << "   Active validators: " << consensus.getActiveValidatorCount() << std::endl;
    std::cout << "   Required signatures (with Charlie offline): " << consensus.getRequiredSignatures() << std::endl;

    // Example 6: Test revocation
    std::cout << "\n6. Testing revocation..." << std::endl;

    auto bob_result = consensus.getValidator(bob_key.value().getId());
    if (bob_result.is_ok()) {
        std::cout << "   Bob canSign (before revoke): " << (bob_result.value()->canSign() ? "yes" : "no") << std::endl;
    }

    consensus.revokeValidator(bob_key.value().getId());
    std::cout << "   Revoked Bob" << std::endl;

    bob_result = consensus.getValidator(bob_key.value().getId());
    if (bob_result.is_ok()) {
        std::cout << "   Bob canSign (after revoke): " << (bob_result.value()->canSign() ? "yes" : "no") << std::endl;
        std::cout << "   Bob status: " << (int)bob_result.value()->getStatus() << " (2 = REVOKED)" << std::endl;
    }

    std::cout << "   Active validators: " << consensus.getActiveValidatorCount() << std::endl;

    // Example 7: Test key with expiration
    std::cout << "\n7. Testing key with expiration..." << std::endl;

    auto now = std::chrono::system_clock::now();
    auto one_hour = now + std::chrono::hours(1);
    auto expiring_key = Key::generateWithExpiration(one_hour);

    if (expiring_key.is_ok()) {
        std::cout << "   Created key with 1-hour expiration" << std::endl;
        std::cout << "   Key isExpired: " << (expiring_key.value().isExpired() ? "yes" : "no") << std::endl;
        std::cout << "   Key isValid: " << (expiring_key.value().isValid() ? "yes" : "no") << std::endl;

        consensus.addValidator("dave", expiring_key.value());
        std::cout << "   Added Dave with expiring key" << std::endl;
        std::cout << "   Active validators: " << consensus.getActiveValidatorCount() << std::endl;
    }

    // Example 8: Key serialization
    std::cout << "\n8. Testing key serialization..." << std::endl;

    auto serialized = alice_key.value().serialize();
    std::cout << "   Serialized key size: " << serialized.size() << " bytes" << std::endl;

    auto deserialized = Key::deserialize(serialized);
    if (deserialized.is_ok()) {
        std::cout << "   Deserialized key ID matches: "
                  << (alice_key.value().getId() == deserialized.value().getId() ? "yes" : "no") << std::endl;
    }

    std::cout << "\n=== Demo Complete ===" << std::endl;
    return 0;
}
