#include "blockit/identity/credential_status.hpp"
#include "blockit/identity/did_registry.hpp"
#include "blockit/ledger/chain.hpp"
#include <doctest/doctest.h>

using namespace blockit;

// Test data type with required to_string() method
struct ChainDIDTestData {
    dp::String name{};
    dp::i32 value{0};

    ChainDIDTestData() = default;
    ChainDIDTestData(const std::string &n, int v) : name(dp::String(n.c_str())), value(v) {}

    std::string to_string() const { return std::string(name.c_str()) + ":" + std::to_string(value); }

    auto members() { return std::tie(name, value); }
    auto members() const { return std::tie(name, value); }
};

TEST_SUITE("Chain DID Integration Tests") {

    TEST_CASE("Initialize DID support on chain") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});

        CHECK_FALSE(chain.hasDID());

        chain.initializeDID();

        CHECK(chain.hasDID());
        CHECK(chain.getDIDRegistry() != nullptr);
        CHECK(chain.getCredentialStatusList() != nullptr);
    }

    TEST_CASE("Create DID through chain") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});
        chain.initializeDID();

        auto key = Key::generate().value();
        auto registry = chain.getDIDRegistry();

        auto result = registry->create(key);
        REQUIRE(result.is_ok());

        auto [doc, op] = result.value();
        CHECK(doc.isActive());
        CHECK(op.getOperationType() == DIDOperationType::Create);
    }

    TEST_CASE("Resolve DID through chain") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});
        chain.initializeDID();

        auto key = Key::generate().value();
        auto registry = chain.getDIDRegistry();

        auto create_result = registry->create(key);
        REQUIRE(create_result.is_ok());

        auto [doc, op] = create_result.value();
        auto did = doc.getId();

        // Resolve by DID
        auto resolve_result = registry->resolve(did);
        REQUIRE(resolve_result.is_ok());
        CHECK(resolve_result.value().getId() == did);

        // Resolve by string
        auto resolve_str_result = registry->resolve(did.toString());
        REQUIRE(resolve_str_result.is_ok());
        CHECK(resolve_str_result.value().getId() == did);
    }

    TEST_CASE("DID registry tracks multiple DIDs") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});
        chain.initializeDID();

        auto registry = chain.getDIDRegistry();

        // Create multiple DIDs
        std::vector<DID> dids;
        for (int i = 0; i < 5; i++) {
            auto key = Key::generate().value();
            auto result = registry->create(key);
            REQUIRE(result.is_ok());
            dids.push_back(result.value().first.getId());
        }

        CHECK(registry->size() == 5);

        // Verify all can be resolved
        for (const auto &did : dids) {
            auto resolve_result = registry->resolve(did);
            REQUIRE(resolve_result.is_ok());
        }
    }

    TEST_CASE("Credential status list tracks credentials") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});
        chain.initializeDID();

        auto status_list = chain.getCredentialStatusList();

        // Record some credentials
        status_list->recordIssue("cred-001", "did:blockit:issuer1");
        status_list->recordIssue("cred-002", "did:blockit:issuer1");
        status_list->recordIssue("cred-003", "did:blockit:issuer2");

        CHECK(status_list->size() == 3);
        CHECK(status_list->isActive("cred-001"));
        CHECK(status_list->isActive("cred-002"));
        CHECK(status_list->isActive("cred-003"));

        // Revoke one
        auto revoke_result = status_list->recordRevoke("cred-001", "did:blockit:issuer1", "Compromised");
        REQUIRE(revoke_result.is_ok());
        CHECK(status_list->isRevoked("cred-001"));
        CHECK_FALSE(status_list->isActive("cred-001"));
    }

    TEST_CASE("Update DID document through registry") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});
        chain.initializeDID();

        auto key = Key::generate().value();
        auto registry = chain.getDIDRegistry();

        // Create DID
        auto create_result = registry->create(key);
        REQUIRE(create_result.is_ok());

        auto [doc, op] = create_result.value();
        auto did = doc.getId();

        // Update the document (add a service)
        auto updated_doc = doc;
        updated_doc.addService(Service(did.withFragment("api"), "API", "https://example.com/api"));

        auto update_result = registry->update(did, updated_doc, key);
        REQUIRE(update_result.is_ok());

        // Verify update
        auto resolve_result = registry->resolve(did);
        REQUIRE(resolve_result.is_ok());
        auto loaded_doc = resolve_result.value();

        auto services = loaded_doc.getServices();
        CHECK(services.size() == 1);
    }

    TEST_CASE("Deactivate DID through registry") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});
        chain.initializeDID();

        auto key = Key::generate().value();
        auto registry = chain.getDIDRegistry();

        // Create DID
        auto create_result = registry->create(key);
        REQUIRE(create_result.is_ok());

        auto did = create_result.value().first.getId();
        CHECK(create_result.value().first.isActive());

        // Deactivate
        auto deactivate_result = registry->deactivate(did, key);
        REQUIRE(deactivate_result.is_ok());
        CHECK(deactivate_result.value().getOperationType() == DIDOperationType::Deactivate);

        // Verify deactivated
        auto resolve_result = registry->resolve(did);
        REQUIRE(resolve_result.is_ok());
        CHECK_FALSE(resolve_result.value().isActive());
    }

    TEST_CASE("DID not initialized error") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});

        // Without initialization, DID registry should be null
        CHECK(chain.getDIDRegistry() == nullptr);
        CHECK(chain.getCredentialStatusList() == nullptr);
    }

    TEST_CASE("Chain with PoA and DID") {
        Chain<ChainDIDTestData> chain("test-chain", "genesis-tx", ChainDIDTestData{"genesis", 0});

        // Initialize both PoA and DID
        chain.initializePoA();
        chain.initializeDID();

        CHECK(chain.hasPoA());
        CHECK(chain.hasDID());

        // Create a validator with DID
        auto validator_key = Key::generate().value();
        auto registry = chain.getDIDRegistry();

        auto did_result = registry->create(validator_key);
        REQUIRE(did_result.is_ok());
        auto validator_did = did_result.value().first.getId();

        // Add as validator
        auto add_result = chain.addValidator(validator_did.toString(), validator_key);
        CHECK(add_result.is_ok());
        CHECK(chain.getActiveValidatorCount() == 1);
    }
}
