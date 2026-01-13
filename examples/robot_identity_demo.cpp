/// Robot Identity Demo
/// Demonstrates robot identity management with DIDs and Verifiable Credentials

#include <blockit/identity/identity.hpp>
#include <blockit/storage/blockit_store.hpp>
#include <filesystem>
#include <iostream>

using namespace blockit;
namespace fs = std::filesystem;

// Transaction data type for the demo
struct RobotTaskData {
    dp::String task_id{};
    dp::String task_type{};
    dp::i32 status{0};

    RobotTaskData() = default;
    RobotTaskData(const std::string &id, const std::string &type, int s = 0)
        : task_id(dp::String(id.c_str())), task_type(dp::String(type.c_str())), status(s) {}

    std::string to_string() const {
        return std::string(task_id.c_str()) + ":" + std::string(task_type.c_str()) + ":" + std::to_string(status);
    }

    auto members() { return std::tie(task_id, task_type, status); }
    auto members() const { return std::tie(task_id, task_type, status); }
};

int main() {
    std::cout << "=== Robot Identity Demo ===" << std::endl;
    std::cout << std::endl;

    // === Part 1: Setup ===
    std::cout << "--- Part 1: Setting Up Fleet Infrastructure ---" << std::endl;

    auto temp_dir = fs::temp_directory_path() / "blockit_robot_demo";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    // Initialize Blockit with DID support
    Blockit<RobotTaskData> blockit;
    auto crypto = std::make_shared<Crypto>("fleet_manager_key");

    auto init_result = blockit.initialize(dp::String(temp_dir.string().c_str()), dp::String("robot-fleet"),
                                          dp::String("genesis"), RobotTaskData{"genesis", "init", 1}, crypto);
    if (!init_result.is_ok()) {
        std::cerr << "Failed to initialize blockit" << std::endl;
        return 1;
    }

    // Enable DID support
    blockit.initializeDID();
    std::cout << "Fleet blockchain initialized with DID support" << std::endl;
    std::cout << std::endl;

    // === Part 2: Create Fleet Manager Identity ===
    std::cout << "--- Part 2: Creating Fleet Manager Identity ---" << std::endl;

    auto fleet_mgr_key = Key::generate().value();
    auto fleet_mgr_result = blockit.createDID(fleet_mgr_key);
    if (!fleet_mgr_result.is_ok()) {
        std::cerr << "Failed to create fleet manager DID" << std::endl;
        return 1;
    }
    auto fleet_mgr_did = fleet_mgr_result.value().first.getId();
    std::cout << "Fleet Manager DID: " << fleet_mgr_did.toString() << std::endl;
    std::cout << std::endl;

    // === Part 3: Onboard a Robot ===
    std::cout << "--- Part 3: Onboarding a New Robot ---" << std::endl;

    auto robot_key = Key::generate().value();
    auto robot_result =
        blockit.createRobotIdentity(robot_key, "DELIVERY_BOT_001", {"navigation", "pickup", "delivery", "charging"},
                                    fleet_mgr_key);
    if (!robot_result.is_ok()) {
        std::cerr << "Failed to create robot identity" << std::endl;
        return 1;
    }

    auto [robot_doc, robot_auth_cred] = robot_result.value();
    auto robot_did = robot_doc.getId();

    std::cout << "Robot DID: " << robot_did.toString() << std::endl;
    std::cout << "Robot ID: " << robot_auth_cred.getClaim("robot_id").value() << std::endl;
    std::cout << "Capabilities: " << robot_auth_cred.getClaim("capabilities").value() << std::endl;
    std::cout << "Credential ID: " << robot_auth_cred.getId() << std::endl;
    std::cout << std::endl;

    // === Part 4: Issue Additional Credentials ===
    std::cout << "--- Part 4: Issuing Zone Access Credential ---" << std::endl;

    auto zone_cred_result = blockit.issueCredential(fleet_mgr_key, robot_did, CredentialType::ZoneAccess,
                                                    {{"zone_id", "warehouse_a"}, {"access_level", "full"}},
                                                    std::chrono::hours(8));
    if (!zone_cred_result.is_ok()) {
        std::cerr << "Failed to issue zone access credential" << std::endl;
        return 1;
    }
    auto zone_cred = zone_cred_result.value();
    std::cout << "Zone Access Credential issued" << std::endl;
    std::cout << "  Zone: " << zone_cred.getClaim("zone_id").value() << std::endl;
    std::cout << "  Access Level: " << zone_cred.getClaim("access_level").value() << std::endl;
    std::cout << "  Valid for: 8 hours" << std::endl;
    std::cout << std::endl;

    // === Part 5: Robot Creates a Presentation ===
    std::cout << "--- Part 5: Robot Creating Verifiable Presentation ---" << std::endl;

    auto presentation = VerifiablePresentation::create(robot_did);
    presentation.addCredential(robot_auth_cred);
    presentation.addCredential(zone_cred);

    // Set challenge for authentication
    std::string challenge = "access-request-" + std::to_string(std::time(nullptr));
    presentation.setChallenge(challenge);

    // Robot signs the presentation
    auto sign_result = presentation.sign(robot_key, robot_did.withFragment("key-1"));
    if (!sign_result.is_ok()) {
        std::cerr << "Failed to sign presentation" << std::endl;
        return 1;
    }

    std::cout << "Presentation created and signed" << std::endl;
    std::cout << "  Holder: " << presentation.getHolder().toString() << std::endl;
    std::cout << "  Challenge: " << presentation.getChallenge() << std::endl;
    std::cout << "  Credentials: " << presentation.getCredentialCount() << std::endl;
    std::cout << std::endl;

    // === Part 6: Verify the Presentation ===
    std::cout << "--- Part 6: Verifying Presentation ---" << std::endl;

    // Verify presentation signature
    auto vp_verify_result = presentation.verifyWithKey(robot_key);
    if (vp_verify_result.is_ok() && vp_verify_result.value()) {
        std::cout << "Presentation signature: VALID" << std::endl;
    } else {
        std::cout << "Presentation signature: INVALID" << std::endl;
    }

    // Verify each credential
    auto credentials = presentation.getCredentials();
    for (size_t i = 0; i < credentials.size(); i++) {
        auto cred_verify = credentials[i].verify(fleet_mgr_key);
        auto type_strings = credentials[i].getTypeStrings();
        std::string type_str = type_strings.size() > 1 ? type_strings[1] : "Unknown";
        std::cout << "Credential " << (i + 1) << " (" << type_str
                  << "): " << (cred_verify.is_ok() && cred_verify.value() ? "VALID" : "INVALID") << std::endl;
    }

    // Check credential status
    auto status_list = blockit.getCredentialStatusList();
    std::cout << "Robot Auth Credential Status: "
              << (status_list->isActive(robot_auth_cred.getId()) ? "ACTIVE" : "REVOKED/SUSPENDED") << std::endl;
    std::cout << "Zone Access Credential Status: "
              << (status_list->isActive(zone_cred.getId()) ? "ACTIVE" : "REVOKED/SUSPENDED") << std::endl;
    std::cout << std::endl;

    // === Part 7: Credential Revocation ===
    std::cout << "--- Part 7: Revoking Zone Access (Shift Ended) ---" << std::endl;

    auto revoke_result =
        status_list->recordRevoke(zone_cred.getId(), fleet_mgr_did.toString(), "Shift ended - access no longer needed");
    if (revoke_result.is_ok()) {
        std::cout << "Zone Access Credential revoked" << std::endl;
        std::cout << "  New Status: " << (status_list->isRevoked(zone_cred.getId()) ? "REVOKED" : "ACTIVE") << std::endl;
    }
    std::cout << std::endl;

    // === Part 8: Summary ===
    std::cout << "--- Summary ---" << std::endl;
    std::cout << "DIDs in registry: " << blockit.getDIDRegistry()->size() << std::endl;
    std::cout << "Credentials tracked: " << status_list->size() << std::endl;
    std::cout << "  Active: " << (status_list->size() - status_list->getRevokedCredentials().size()) << std::endl;
    std::cout << "  Revoked: " << status_list->getRevokedCredentials().size() << std::endl;
    std::cout << std::endl;

    // Cleanup
    fs::remove_all(temp_dir);

    std::cout << "=== Demo Complete ===" << std::endl;
    return 0;
}
