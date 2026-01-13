/// DID (Decentralized Identifiers) Demo
/// Demonstrates basic DID operations: creation, resolution, update, and deactivation

#include <blockit/identity/identity.hpp>
#include <iostream>

using namespace blockit;

int main() {
    std::cout << "=== Blockit DID Demo ===" << std::endl;
    std::cout << std::endl;

    // === Part 1: Create a DID ===
    std::cout << "--- Part 1: Creating a DID ---" << std::endl;

    // Generate a key pair
    auto key_result = Key::generate();
    if (!key_result.is_ok()) {
        std::cerr << "Failed to generate key" << std::endl;
        return 1;
    }
    auto key = key_result.value();

    // Create a DID from the key
    auto did = DID::fromKey(key);
    std::cout << "Created DID: " << did.toString() << std::endl;
    std::cout << "  Method: " << did.getMethod() << std::endl;
    std::cout << "  Method-specific ID: " << did.getMethodSpecificId().substr(0, 16) << "..." << std::endl;
    std::cout << std::endl;

    // === Part 2: Create a DID Document ===
    std::cout << "--- Part 2: Creating a DID Document ---" << std::endl;

    auto doc = DIDDocument::create(did, key);
    std::cout << "DID Document created" << std::endl;
    std::cout << "  Active: " << (doc.isActive() ? "yes" : "no") << std::endl;
    std::cout << "  Version: " << doc.getVersion() << std::endl;

    // Check verification methods
    auto vms = doc.getVerificationMethods();
    std::cout << "  Verification Methods: " << vms.size() << std::endl;
    for (const auto &vm : vms) {
        std::cout << "    - " << vm.getId() << " (type: " << vm.getTypeString() << ")" << std::endl;
    }
    std::cout << std::endl;

    // === Part 3: Add Services ===
    std::cout << "--- Part 3: Adding Services ---" << std::endl;

    doc.addService(Service(did.withFragment("api"), ServiceType::SensorDataEndpoint, "https://robot.example.com/sensors"));
    doc.addService(Service(did.withFragment("tasks"), ServiceType::TaskQueue, "https://robot.example.com/tasks"));

    auto services = doc.getServices();
    std::cout << "Services added: " << services.size() << std::endl;
    for (const auto &svc : services) {
        std::cout << "    - " << svc.getId() << " -> " << svc.getServiceEndpoint() << std::endl;
    }
    std::cout << std::endl;

    // === Part 4: DID Registry ===
    std::cout << "--- Part 4: Using DID Registry ---" << std::endl;

    DIDRegistry registry;

    // Create a new DID through the registry
    auto create_result = registry.create(key);
    if (!create_result.is_ok()) {
        std::cerr << "Failed to create DID in registry" << std::endl;
        return 1;
    }
    auto [created_doc, create_op] = create_result.value();
    std::cout << "DID created in registry: " << created_doc.getId().toString() << std::endl;
    std::cout << "  Operation type: " << didOperationTypeToString(create_op.getOperationType()) << std::endl;

    // Resolve the DID
    auto resolve_result = registry.resolve(created_doc.getId());
    if (resolve_result.is_ok()) {
        std::cout << "DID resolved successfully" << std::endl;
        std::cout << "  Active: " << (resolve_result.value().isActive() ? "yes" : "no") << std::endl;
    }
    std::cout << std::endl;

    // === Part 5: Update and Deactivate ===
    std::cout << "--- Part 5: Update and Deactivate ---" << std::endl;

    // Update the document (add a service)
    auto updated_doc = resolve_result.value();
    updated_doc.addService(Service(created_doc.getId().withFragment("messaging"), ServiceType::DIDCommMessaging,
                                   "https://robot.example.com/didcomm"));

    auto update_result = registry.update(created_doc.getId(), updated_doc, key);
    if (update_result.is_ok()) {
        std::cout << "DID updated successfully" << std::endl;
        std::cout << "  Operation type: " << didOperationTypeToString(update_result.value().getOperationType())
                  << std::endl;
    }

    // Check history
    auto history = registry.getHistory(created_doc.getId());
    std::cout << "Document history: " << history.size() << " versions" << std::endl;

    // Deactivate the DID
    auto deactivate_result = registry.deactivate(created_doc.getId(), key);
    if (deactivate_result.is_ok()) {
        std::cout << "DID deactivated successfully" << std::endl;

        // Verify it's deactivated
        auto final_resolve = registry.resolve(created_doc.getId());
        if (final_resolve.is_ok()) {
            std::cout << "  Final state - Active: " << (final_resolve.value().isActive() ? "yes" : "no") << std::endl;
        }
    }
    std::cout << std::endl;

    // === Part 6: DID URLs ===
    std::cout << "--- Part 6: Working with DID URLs ---" << std::endl;

    // Create DID URL with fragment
    std::string did_url = did.withFragment("key-1");
    std::cout << "DID URL: " << did_url << std::endl;

    // Parse base DID from URL (fragment is stripped)
    auto parsed_result = DID::parse(did_url);
    if (parsed_result.is_ok()) {
        auto parsed = parsed_result.value();
        std::cout << "  Base DID: " << parsed.toString() << std::endl;
        std::cout << "  Same as original: " << (parsed == did ? "yes" : "no") << std::endl;
    }
    std::cout << std::endl;

    std::cout << "=== Demo Complete ===" << std::endl;
    return 0;
}
