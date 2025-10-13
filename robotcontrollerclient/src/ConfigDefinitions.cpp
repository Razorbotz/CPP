#include "ConfigDefinitions.hpp"

// --- Helper for Initialization ---
static void initialize_bool_map(std::map<std::string, bool>& map, const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
        map[key] = true;
    }
}

// --- Accessor Function Implementations ---

std::string& get_configFile() {
    static std::string configFile = "config.txt";
    return configFile;
}

std::set<std::string>& get_speedometer_keys() {
    static std::set<std::string> speedometer_keys = {
        "DISPLAY_SPEED", "NUMBERS_INSIDE", "NUMBER_TICKS"
    };
    return speedometer_keys;
}

std::vector<std::string>& get_talon_keys() {
    static std::vector<std::string> talon_keys = {
        "Device ID", "Bus Voltage", "Output Current", "Output Percent",
        "Temperature", "Sensor Position", "Sensor Velocity", "Max Current"
    };
    return talon_keys;
}
std::vector<std::string>& get_reset_talon_keys() {
    static std::vector<std::string> reset_talon_keys = get_talon_keys();
    return reset_talon_keys;
}
std::map<std::string, bool>& get_talon_values() {
    static std::map<std::string, bool> talon_values;
    return talon_values;
}

std::vector<std::string>& get_falcon_keys() {
    static std::vector<std::string> falcon_keys = get_talon_keys();
    return falcon_keys;
}
std::vector<std::string>& get_reset_falcon_keys() {
    static std::vector<std::string> reset_falcon_keys = get_talon_keys();
    return reset_falcon_keys;
}
std::map<std::string, bool>& get_falcon_values() {
    static std::map<std::string, bool> falcon_values;
    return falcon_values;
}

std::vector<std::string>& get_linear_keys() {
    static std::vector<std::string> linear_keys = {
        "Motor Number", "Speed", "Potentiometer", "Time Without Change",
        "Max", "Min", "Error", "At Min", "At Max", "Distance", "Sensorless"
    };
    return linear_keys;
}
std::vector<std::string>& get_reset_linear_keys() {
    static std::vector<std::string> reset_linear_keys = get_linear_keys();
    return reset_linear_keys;
}
std::map<std::string, bool>& get_linear_values() {
    static std::map<std::string, bool> linear_values;
    return linear_values;
}

std::vector<std::string>& get_power_keys(){
    static std::vector<std::string> power_keys = {
        "Voltage", "Temp", "Current 0", "Current 1", "Current 2",
        "Current 3", "Current 4", "Current 5", "Current 6"
    };
    return power_keys;
}
std::vector<std::string>& get_reset_power_keys(){
    static std::vector<std::string> reset_power_keys = get_power_keys();
    return reset_power_keys;
}
std::map<std::string, bool>& get_power_values(){
    static std::map<std::string, bool> power_values;
    return power_values;
}

std::vector<std::string>& get_power2_keys(){
    static std::vector<std::string> power2_keys = {
        "Current 7", "Current 8", "Current 9", "Current 10", "Current 11",
        "Current 12", "Current 13", "Current 14", "Current 15"
    };
    return power2_keys;
}
std::vector<std::string>& get_reset_power2_keys(){
    static std::vector<std::string> reset_power2_keys = get_power2_keys();
    return reset_power2_keys;
}
std::map<std::string, bool>& get_power2_values(){
    static std::map<std::string, bool> power2_values;
    return power2_values;
}

std::vector<std::string>& get_autonomy_keys(){
    static std::vector<std::string> autonomy_keys = {
        "Robot State", "Excavation State", "Error State", "Diagnostics State",
        "Tilt State", "Dump State", "Level Bucket", "Level Arms", "Dest X", "Dest Z"
    };
    return autonomy_keys;
}
std::vector<std::string>& get_reset_autonomy_keys(){
    static std::vector<std::string> reset_autonomy_keys = get_autonomy_keys();
    return reset_autonomy_keys;
}
std::map<std::string, bool>& get_autonomy_values(){
    static std::map<std::string, bool> autonomy_values;
    return autonomy_values;
}

std::vector<std::string>& get_zed_keys(){
    static std::vector<std::string> zed_keys = {
        "X", "Y", "Z", "roll", "pitch", "yaw", "aruco"
    };
    return zed_keys;
}
std::vector<std::string>& get_reset_zed_keys(){
    static std::vector<std::string> reset_zed_keys = get_zed_keys();
    return reset_zed_keys;
}
std::map<std::string, bool>& get_zed_values(){
    static std::map<std::string, bool> zed_values;
    return zed_values;
}

std::vector<std::string>& get_communication_keys(){
    static std::vector<std::string> communication_keys = {
        "RSSI", "Wi-Fi", "CAN Bus", "Using CAN1", "RX packets", "TX packets", "CAN Bus2", "RX2 packets", "TX2 packets", "Status"
    };
    return communication_keys;
}
std::vector<std::string>& get_reset_communication_keys(){
    static std::vector<std::string> reset_communication_keys = get_communication_keys();
    return reset_communication_keys;
}
std::map<std::string, bool>& get_communication_values(){
    static std::map<std::string, bool> communication_values;
    return communication_values;
}

std::vector<std::string>& get_drivetrain_keys(){
    static std::vector<std::string> drivetrain_keys = {
        "F1 Vel", "F1 RPM", "F1 Speed", "F2 Vel", "F2 RPM", "F2 Speed", "F3 Vel", "F3 RPM", "F3 Speed", "F4 Vel", "F4 RPM", "F4 Speed"
    };
    return drivetrain_keys;
}
std::vector<std::string>& get_reset_drivetrain_keys(){
    static std::vector<std::string> reset_drivetrain_keys = get_drivetrain_keys();
    return reset_drivetrain_keys;
}
std::map<std::string, bool>& get_drivetrain_values(){
    static std::map<std::string, bool> drivetrain_values;
    return drivetrain_values;
}


// --- Central Maps ---
std::map<std::string, std::vector<std::string>*>& get_key_vectors() {
    static std::map<std::string, std::vector<std::string>*> key_vectors = {
        {"Talon", &get_talon_keys()},
        {"Falcon", &get_falcon_keys()},
        {"Linear", &get_linear_keys()},
        {"Autonomy", &get_autonomy_keys()},
        {"Communication", &get_communication_keys()},
        {"Power2", &get_power2_keys()},
        {"Power", &get_power_keys()},
        {"Zed", &get_zed_keys()},
        {"Drivetrain", &get_drivetrain_keys()}
    };
    return key_vectors;
}

std::map<std::string, std::vector<ElementInfo>>& get_element_definitions() {
    static std::map<std::string, std::vector<ElementInfo>> element_definitions = {
        {"TALON", {
            {ElementType::UInt8, "Device ID"}, {ElementType::UInt16, "Bus Voltage"},
            {ElementType::UInt16, "Output Current"}, {ElementType::Float32, "Output Percent"},
            {ElementType::Float32, "Sensor Velocity"}, {ElementType::UInt8, "Temperature"},
            {ElementType::UInt16, "Sensor Position"}, {ElementType::Float32, "Max Current"}
        }},
        {"FALCON", {
            {ElementType::UInt8, "Device ID"}, {ElementType::UInt16, "Bus Voltage"},
            {ElementType::UInt16, "Output Current"}, {ElementType::Float32, "Output Percent"},
            {ElementType::UInt8, "Temperature"}, {ElementType::Float32, "Sensor Position"},
            {ElementType::Float32, "Sensor Velocity"}, {ElementType::Float32, "Max Current"}
        }},
        {"LINEAR", {
            {ElementType::UInt8, "Motor Number"}, {ElementType::Float32, "Speed"},
            {ElementType::UInt16, "Potentiometer"}, {ElementType::UInt8, "Time Without Change"},
            {ElementType::UInt16, "Max"}, {ElementType::UInt16, "Min"},
            {ElementType::String, "Error"}, {ElementType::Boolean, "At Min"},
            {ElementType::Boolean, "At Max"}, {ElementType::Float32, "Distance"},
            {ElementType::Boolean, "Sensorless"}
        }},
        {"AUTONOMY", {
            {ElementType::String, "Robot State"}, {ElementType::String, "Excavation State"},
            {ElementType::String, "Error State"}, {ElementType::String, "Diagnostics State"},
            {ElementType::String, "Tilt State"}, {ElementType::String, "Dump State"},
            {ElementType::String, "Level Bucket"}, {ElementType::String, "Level Arms"},
            {ElementType::Float32, "Dest X"}, {ElementType::Float32, "Dest Z"}
        }},
        {"ZED", {
            {ElementType::Float32, "X"}, {ElementType::Float32, "Y"}, {ElementType::Float32, "Z"},
            {ElementType::Float32, "roll"}, {ElementType::Float32, "pitch"}, {ElementType::Float32, "yaw"},
            {ElementType::Boolean, "aruco"}
        }},
        {"COMMUNICATION", {
            {ElementType::Int32, "RSSI"}, {ElementType::String, "Wi-Fi"}, {ElementType::String, "CAN Bus"},
            {ElementType::Boolean, "Using CAN1"}, {ElementType::Int32, "RX packets"}, {ElementType::Int32, "TX packets"},
            {ElementType::String, "CAN Bus2"}, {ElementType::Int32, "RX2 packets"}, {ElementType::Int32, "TX2 packets"},
            {ElementType::String, "Status"}
        }},
        {"POWER", {
            {ElementType::Float32, "Voltage"}, {ElementType::Float32, "Temp"},
            {ElementType::Float32, "Current 0"}, {ElementType::Float32, "Current 1"},
            {ElementType::Float32, "Current 2"}, {ElementType::Float32, "Current 3"},
            {ElementType::Float32, "Current 4"}, {ElementType::Float32, "Current 5"}, {ElementType::Float32, "Current 6"}
        }},
        {"POWER2", {
            {ElementType::Float32, "Current 7"}, {ElementType::Float32, "Current 8"}, {ElementType::Float32, "Current 9"},
            {ElementType::Float32, "Current 10"}, {ElementType::Float32, "Current 11"}, {ElementType::Float32, "Current 12"},
            {ElementType::Float32, "Current 13"}, {ElementType::Float32, "Current 14"}, {ElementType::Float32, "Current 15"}
        }},
        {"DRIVETRAIN", {
            {ElementType::Float32, "F1 Vel"},
            {ElementType::Float32, "F1 RPM"},
            {ElementType::Float32, "F1 Speed"},
            {ElementType::Float32, "F2 Vel"},
            {ElementType::Float32, "F2 RPM"},
            {ElementType::Float32, "F2 Speed"},
            {ElementType::Float32, "F3 Vel"},
            {ElementType::Float32, "F3 RPM"},
            {ElementType::Float32, "F3 Speed"},
            {ElementType::Float32, "F4 Vel"},
            {ElementType::Float32, "F4 RPM"},
            {ElementType::Float32, "F4 Speed"}
        }}
    };
    return element_definitions;
}


void initialize_maps() {
    initialize_bool_map(get_talon_values(), get_talon_keys());
    initialize_bool_map(get_falcon_values(), get_falcon_keys());
    initialize_bool_map(get_linear_values(), get_linear_keys());
    initialize_bool_map(get_power_values(), get_power_keys());
    initialize_bool_map(get_power2_values(), get_power2_keys());
    initialize_bool_map(get_autonomy_values(), get_autonomy_keys());
    initialize_bool_map(get_zed_values(), get_zed_keys());
    initialize_bool_map(get_communication_values(), get_communication_keys());
    initialize_bool_map(get_drivetrain_values(), get_drivetrain_keys());
}

// --- Helper Function Implementations ---
std::vector<std::string> getKeys(const std::string& label) {
    if (label == "Power2") return get_power2_keys();
    for (const auto& [prefix, keys_ptr] : get_key_vectors()) {
        if (label.rfind(prefix, 0) == 0)
            return *keys_ptr;
    }
    return get_talon_keys();
}

std::map<std::string, bool>& getMap(std::string label) {
    if (label.rfind("Talon", 0) == 0) return get_talon_values();
    if (label.rfind("Falcon", 0) == 0) return get_falcon_values();
    if (label.rfind("Linear", 0) == 0) return get_linear_values();
    if (label.rfind("Autonomy", 0) == 0) return get_autonomy_values();
    if (label.rfind("Communication", 0) == 0) return get_communication_values();
    if (label.rfind("Power2", 0) == 0) return get_power2_values();
    if (label.rfind("Power", 0) == 0) return get_power_values();
    if (label.rfind("Zed", 0) == 0) return get_zed_values();
    if (label.rfind("Drivetrain", 0) == 0) return get_drivetrain_values();
    return get_talon_values();
}

std::string getNameFromPrefix(std::string label) {
    if (label.rfind("TALON", 0) == 0) return "Talon";
    if (label.rfind("FALCON", 0) == 0) return "Falcon";
    if (label.rfind("LINEAR", 0) == 0) return "Linear";
    if (label.rfind("AUTONOMY", 0) == 0) return "Autonomy";
    if (label.rfind("COMMUNICATION", 0) == 0) return "Communication";
    if (label.rfind("POWER2", 0) == 0) return "Power2";
    if (label.rfind("POWER", 0) == 0) return "Power";
    if (label.rfind("ZED", 0) == 0) return "Zed";
    if (label.rfind("TEST", 0) == 0) return "Test";
    if (label.rfind("DRIVETRAIN", 0) == 0) return "Drivetrain";
    return "Talon";
}