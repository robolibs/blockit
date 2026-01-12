#include "blockit.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

// Farming operation data structure
struct FarmingOperation {
    std::string equipment_id;
    std::string operation_type;
    std::string field_location;
    std::string crop_type;
    double area_covered; // in hectares
    std::string timestamp;

    std::string to_string() const {
        return "FarmingOp{equipment:" + equipment_id + ",operation:" + operation_type + ",field:" + field_location +
               ",crop:" + crop_type + ",area:" + std::to_string(area_covered) + ",time:" + timestamp + "}";
    }
};

// Sensor reading data structure
struct SensorReading {
    std::string sensor_id;
    std::string sensor_type;
    std::string location;
    double value;
    std::string unit;

    std::string to_string() const {
        return "SensorReading{sensor:" + sensor_id + ",type:" + sensor_type + ",location:" + location +
               ",value:" + std::to_string(value) + ",unit:" + unit + "}";
    }
};

// Equipment maintenance record
struct MaintenanceRecord {
    std::string equipment_id;
    std::string maintenance_type;
    std::string technician_id;
    std::string description;

    std::string to_string() const {
        return "Maintenance{equipment:" + equipment_id + ",type:" + maintenance_type + ",technician:" + technician_id +
               ",desc:" + description + "}";
    }
};

void printSeparator(const std::string &title) {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(70, '=') << std::endl;
}

void demonstrateFarmingOperations() {
    printSeparator("FARMING OPERATIONS TRACKING");

    auto privateKey = std::make_shared<chain::Crypto>("farming_key");

    // Create blockchain for farming operations
    chain::Chain<FarmingOperation> farmChain(
        "farm-operations-chain", "genesis-op",
        FarmingOperation{"system", "initialization", "HQ", "none", 0.0, "2025-01-01"}, privateKey);

    // Register farming equipment with metadata
    std::unordered_map<std::string, std::string> tractor_metadata = {
        {"model", "John_Deere_8370R"}, {"year", "2023"}, {"location", "Field_A"}, {"fuel_capacity", "680L"}};
    farmChain.registerParticipant("tractor-001", "ready", tractor_metadata);

    std::unordered_map<std::string, std::string> sprayer_metadata = {
        {"model", "Apache_AS1250"}, {"year", "2024"}, {"tank_capacity", "4540L"}, {"spray_width", "36.6m"}};
    farmChain.registerParticipant("sprayer-001", "ready", sprayer_metadata);

    std::unordered_map<std::string, std::string> harvester_metadata = {
        {"model", "Case_IH_9250"}, {"year", "2022"}, {"grain_tank", "14100L"}};
    farmChain.registerParticipant("harvester-001", "ready", harvester_metadata);

    // Grant capabilities
    farmChain.grantCapability("tractor-001", "TILLAGE");
    farmChain.grantCapability("tractor-001", "SEEDING");
    farmChain.grantCapability("tractor-001", "TRANSPORT");

    farmChain.grantCapability("sprayer-001", "SPRAY_PESTICIDE");
    farmChain.grantCapability("sprayer-001", "SPRAY_FERTILIZER");
    farmChain.grantCapability("sprayer-001", "SPRAY_HERBICIDE");

    farmChain.grantCapability("harvester-001", "HARVEST_GRAIN");
    farmChain.grantCapability("harvester-001", "HARVEST_CORN");

    std::cout << "Registered 3 farming equipment with capabilities and metadata" << std::endl;

    // Create farming operations
    std::vector<chain::Transaction<FarmingOperation>> operations;

    // Tractor tillage operation
    FarmingOperation op1{"tractor-001", "TILLAGE", "Field_A_North", "wheat", 15.5, "2025-03-15_08:30"};
    chain::Transaction<FarmingOperation> tx1("tillage-001", op1, 120);
    tx1.signTransaction(privateKey);
    operations.push_back(tx1);

    // Sprayer pesticide application
    FarmingOperation op2{"sprayer-001", "SPRAY_PESTICIDE", "Field_B_East", "corn", 22.3, "2025-04-20_14:15"};
    chain::Transaction<FarmingOperation> tx2("spray-001", op2, 100);
    tx2.signTransaction(privateKey);
    operations.push_back(tx2);

    // Harvester grain harvest
    FarmingOperation op3{"harvester-001", "HARVEST_GRAIN", "Field_C_West", "wheat", 35.2, "2025-08-10_09:00"};
    chain::Transaction<FarmingOperation> tx3("harvest-001", op3, 150);
    tx3.signTransaction(privateKey);
    operations.push_back(tx3);

    // Try duplicate spray operation (should be prevented)
    FarmingOperation op4{"sprayer-001", "SPRAY_PESTICIDE", "Field_B_East", "corn", 22.3, "2025-04-20_14:15"};
    chain::Transaction<FarmingOperation> tx4("spray-001", op4, 100); // Same ID - should fail
    tx4.signTransaction(privateKey);
    operations.push_back(tx4);

    // Create block with operations
    chain::Block<FarmingOperation> operationsBlock(operations);

    std::cout << "\nRecording farming operations..." << std::endl;
    farmChain.addBlock(operationsBlock);

    // Update equipment states
    farmChain.updateParticipantState("tractor-001", "working");
    farmChain.updateParticipantState("sprayer-001", "maintenance");
    farmChain.updateParticipantState("harvester-001", "idle");

    std::cout << "\nFarming Operations Summary:" << std::endl;
    farmChain.printChainSummary();
}

void demonstrateSensorNetworkLedger() {
    printSeparator("AGRICULTURAL SENSOR NETWORK LEDGER");

    auto privateKey = std::make_shared<chain::Crypto>("sensor_key");

    // Create blockchain for sensor readings
    chain::Chain<SensorReading> sensorChain("sensor-network-chain", "genesis-reading",
                                            SensorReading{"system", "initialization", "HQ", 0.0, "none"}, privateKey);

    // Register sensors with metadata
    std::unordered_map<std::string, std::string> soil_metadata = {
        {"type", "soil_moisture_temperature"}, {"field", "Field_A"}, {"depth", "15cm"}, {"battery_level", "85%"}};
    sensorChain.registerParticipant("soil-sensor-001", "active", soil_metadata);

    std::unordered_map<std::string, std::string> weather_metadata = {
        {"type", "weather_station"}, {"location", "Central_Tower"}, {"height", "10m"}, {"connectivity", "LoRaWAN"}};
    sensorChain.registerParticipant("weather-station-001", "active", weather_metadata);

    std::unordered_map<std::string, std::string> drone_metadata = {
        {"model", "DJI_Agras_T40"}, {"camera", "multispectral"}, {"flight_time", "55min"}};
    sensorChain.registerParticipant("drone-001", "ready", drone_metadata);

    // Grant capabilities
    sensorChain.grantCapability("soil-sensor-001", "READ_SOIL_MOISTURE");
    sensorChain.grantCapability("soil-sensor-001", "READ_SOIL_TEMPERATURE");
    sensorChain.grantCapability("weather-station-001", "READ_WEATHER_DATA");
    sensorChain.grantCapability("drone-001", "AERIAL_SURVEY");

    // Create sensor readings
    std::vector<chain::Transaction<SensorReading>> readings;

    // Soil moisture reading
    SensorReading reading1{"soil-sensor-001", "soil_moisture", "Field_A_North", 23.5, "percent"};
    chain::Transaction<SensorReading> tx1("moisture-001", reading1, 110);
    tx1.signTransaction(privateKey);
    readings.push_back(tx1);

    // Weather data
    SensorReading reading2{"weather-station-001", "temperature", "Central_Tower", 18.2, "celsius"};
    chain::Transaction<SensorReading> tx2("weather-001", reading2, 100);
    tx2.signTransaction(privateKey);
    readings.push_back(tx2);

    // Drone survey data
    SensorReading reading3{"drone-001", "NDVI_index", "Field_C_West", 0.75, "index"};
    chain::Transaction<SensorReading> tx3("drone-001", reading3, 120);
    tx3.signTransaction(privateKey);
    readings.push_back(tx3);

    // Create block with sensor readings
    chain::Block<SensorReading> sensorBlock(readings);

    std::cout << "Recording sensor readings..." << std::endl;
    sensorChain.addBlock(sensorBlock);

    // Demonstrate Merkle tree verification
    std::cout << "\nMerkle Tree Verification of Sensor Data:" << std::endl;
    for (size_t i = 0; i < readings.size(); i++) {
        bool verified = sensorChain.blocks_.back().verifyTransaction(i);
        std::cout << "Sensor Reading " << i << " (" << readings[i].uuid_ << "): " << (verified ? "VERIFIED" : "FAILED")
                  << std::endl;
    }

    std::cout << "\nSensor Network Summary:" << std::endl;
    sensorChain.printChainSummary();
}

void demonstrateMaintenanceLedger() {
    printSeparator("EQUIPMENT MAINTENANCE LEDGER");

    auto privateKey = std::make_shared<chain::Crypto>("maintenance_key");

    // Create blockchain for maintenance records
    chain::Chain<MaintenanceRecord> maintenanceChain(
        "maintenance-chain", "genesis-maintenance",
        MaintenanceRecord{"system", "initialization", "admin", "system_start"}, privateKey);

    // Register maintenance personnel and equipment
    maintenanceChain.registerParticipant("technician-001", "available",
                                         {{"name", "John_Smith"}, {"certification", "Level_3"}});
    maintenanceChain.registerParticipant("technician-002", "available",
                                         {{"name", "Mary_Johnson"}, {"certification", "Level_2"}});
    maintenanceChain.registerParticipant("supervisor-001", "available",
                                         {{"name", "Bob_Wilson"}, {"certification", "Supervisor"}});

    // Grant capabilities
    maintenanceChain.grantCapability("technician-001", "ROUTINE_MAINTENANCE");
    maintenanceChain.grantCapability("technician-001", "REPAIRS");
    maintenanceChain.grantCapability("technician-002", "ROUTINE_MAINTENANCE");
    maintenanceChain.grantCapability("supervisor-001", "ROUTINE_MAINTENANCE");
    maintenanceChain.grantCapability("supervisor-001", "REPAIRS");
    maintenanceChain.grantCapability("supervisor-001", "INSPECTIONS");

    // Create maintenance records
    std::vector<chain::Transaction<MaintenanceRecord>> records;

    // Routine maintenance
    MaintenanceRecord record1{"tractor-001", "ROUTINE_MAINTENANCE", "technician-001",
                              "Oil_change_and_filter_replacement"};
    chain::Transaction<MaintenanceRecord> tx1("maintenance-001", record1, 100);
    tx1.signTransaction(privateKey);
    records.push_back(tx1);

    // Repair work
    MaintenanceRecord record2{"sprayer-001", "REPAIRS", "supervisor-001", "Hydraulic_pump_replacement"};
    chain::Transaction<MaintenanceRecord> tx2("repair-001", record2, 150);
    tx2.signTransaction(privateKey);
    records.push_back(tx2);

    // Create block with maintenance records
    chain::Block<MaintenanceRecord> maintenanceBlock(records);

    std::cout << "Recording equipment maintenance..." << std::endl;
    maintenanceChain.addBlock(maintenanceBlock);

    std::cout << "\nMaintenance Ledger Summary:" << std::endl;
    maintenanceChain.printChainSummary();
}

int main() {
    std::cout << "Generic Blockit Library - Farming Industry Demo" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << "Demonstrating generic authorization system for agricultural operations." << std::endl;

    try {
        demonstrateFarmingOperations();
        demonstrateSensorNetworkLedger();
        demonstrateMaintenanceLedger();

        printSeparator("FARMING DEMO COMPLETE");
        std::cout << "Successfully demonstrated generic blockchain system for:" << std::endl;
        std::cout << "✅ Farming Equipment Operations - Tractors, sprayers, harvesters" << std::endl;
        std::cout << "✅ Agricultural Sensor Networks - Soil, weather, drone data" << std::endl;
        std::cout << "✅ Equipment Maintenance Ledger - Repair and maintenance tracking" << std::endl;
        std::cout << "✅ Capability-based Authorization - Role-based access control" << std::endl;
        std::cout << "✅ Metadata Management - Equipment specifications and parameters" << std::endl;
        std::cout << "✅ Duplicate Prevention - Avoiding double-recording of operations" << std::endl;
        std::cout << "\nThe same system can be used for:" << std::endl;
        std::cout << "• Manufacturing equipment tracking" << std::endl;
        std::cout << "• IoT device management" << std::endl;
        std::cout << "• Supply chain monitoring" << std::endl;
        std::cout << "• Energy grid management" << std::endl;
        std::cout << "• Healthcare device tracking" << std::endl;
        std::cout << "• Smart city infrastructure" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
