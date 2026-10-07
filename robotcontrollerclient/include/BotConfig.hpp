#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>

/**
 * BotConfig - Data-driven robot configuration.
 * 
 * Instead of scattering if(primaryBot)/else if(backupBot)/else if(dumpBot)
 * across 30+ locations, define each robot's motors, mechanisms, and features
 * once in a BotConfig struct. All GUI setup, message routing, and telemetry
 * display reads from this config.
 * 
 * Adding a new robot = defining one new BotConfig. Zero code changes elsewhere.
 * 
 * Usage:
 *   // At startup, select the active config:
 *   BotConfig activeConfig = configs::primaryBot();
 *   // or:
 *   activeConfig = configs::dumpBot();
 * 
 *   // In setupGUI — iterate config instead of branching:
 *   for (const auto& motor : activeConfig.upperLeftMotors) {
 *       create_labeled_box(motor.displayName, ...);
 *   }
 */

// ============================================================================
// Enums
// ============================================================================

enum class MotorType {
    TALON,
    FALCON,
    NEO,
    KRAKEN
};

enum class MotorRole {
    ARM,            // Arm actuator (has position sensor)
    BUCKET,         // Bucket actuator (has position sensor)
    DRIVE,          // Drivetrain motor
    CONVEYOR,       // Conveyor belt (speed only, no position)
    OTHER           // Anything else
};

enum class PanelPosition {
    UPPER_LEFT,     // Inner left column (arm indicators, etc.)
    UPPER_RIGHT,    // Inner right column (bucket indicators, etc.)
    LOWER_LEFT,     // Lower left (drive motors)
    LOWER_RIGHT     // Lower right (drive motors)
};

enum class MechanismMode {
    NONE,           // No mechanism (dump bot has no arm)
    SINGLE,         // Single actuator
    PAIRED          // L/R paired actuators with sync checking
};

// ============================================================================
// Motor definition
// ============================================================================

struct MotorDef {
    std::string internalLabel;   // "Talon 1", "Falcon 1", etc. — matches BinaryMessage label
    std::string displayName;     // "Arm L", "Dump Bucket", "Kraken 1" — shown in GUI
    MotorType type;
    MotorRole role;
    PanelPosition panel;
    bool rightAligned = false;   // For create_labeled_box right parameter

    // For actuators: sensor range for PositionBar
    int sensorMin = 0;
    int sensorMax = 920;
};

// ============================================================================
// Mechanism definition (arm, bucket, conveyor)
// ============================================================================

struct MechanismDef {
    std::string name;            // "Arm", "Bucket", "Conveyor"
    MechanismMode mode;
    int sensorMax;               // Max sensor range (920 for arm, 700 for bucket)

    // Which MotorDef internalLabels drive this mechanism
    // Single mode: leftMotor only. Paired mode: both.
    std::string leftMotor;       // internal label of the left (or single) motor
    std::string rightMotor;      // internal label of the right motor (empty if SINGLE)

    int syncThreshold = 50;      // For paired mode: position difference that triggers desync warning
};

// ============================================================================
// Feature flags
// ============================================================================

struct BotFeatures {
    bool hasLidar = false;
    bool hasProximityBar = false;
    bool hasConveyor = false;
    int armShowThreshold = 400;   // For ProximityBar arm-gating
};

// ============================================================================
// Bot configuration
// ============================================================================

struct BotConfig {
    std::string name;            // "Primary Bot", "Backup Bot", "Dump Bot"
    std::string cliFlag;         // "--primary", "--backup_bot", "--dump_bot"

    // All motors on this robot
    std::vector<MotorDef> motors;

    // Mechanisms (arm, bucket, conveyor)
    std::vector<MechanismDef> mechanisms;

    // Feature flags
    BotFeatures features;

    // Network
    std::string orinIP = "192.168.0.6";
    std::string nanoIP = "192.168.0.5";

    // ---- Derived helpers (call after construction) ----

    /** Get all motors assigned to a specific panel position. */
    std::vector<MotorDef> getMotorsForPanel(PanelPosition pos) const {
        std::vector<MotorDef> result;
        for (const auto& m : motors) {
            if (m.panel == pos) result.push_back(m);
        }
        return result;
    }

    /** Get the set of internal labels for a given motor type. */
    std::set<std::string> getLabelsForType(MotorType type) const {
        std::set<std::string> labels;
        for (const auto& m : motors) {
            if (m.type == type) labels.insert(m.internalLabel);
        }
        return labels;
    }

    /** Get all internal labels (for validLabels set). */
    std::set<std::string> getAllMotorLabels() const {
        std::set<std::string> labels;
        for (const auto& m : motors) labels.insert(m.internalLabel);
        return labels;
    }

    /** Build display name map: internalLabel → displayName */
    std::map<std::string, std::string> getDisplayNameMap() const {
        std::map<std::string, std::string> result;
        for (const auto& m : motors) {
            result[m.internalLabel] = m.displayName;
        }
        return result;
    }

    /** Find a motor definition by internal label. Returns nullptr if not found. */
    const MotorDef* findMotor(const std::string& internalLabel) const {
        for (const auto& m : motors) {
            if (m.internalLabel == internalLabel) return &m;
        }
        return nullptr;
    }

    /** Find the mechanism definition for a given name. */
    const MechanismDef* findMechanism(const std::string& name) const {
        for (const auto& m : mechanisms) {
            if (m.name == name) return &m;
        }
        return nullptr;
    }

    /** Get the mechanism a motor drives (by internal label). */
    const MechanismDef* getMechanismForMotor(const std::string& internalLabel) const {
        for (const auto& mech : mechanisms) {
            if (mech.leftMotor == internalLabel || mech.rightMotor == internalLabel) {
                return &mech;
            }
        }
        return nullptr;
    }
};


// ============================================================================
// Predefined configurations
// ============================================================================

namespace configs {

inline BotConfig primaryBot() {
    BotConfig cfg;
    cfg.name = "Primary Bot";
    cfg.cliFlag = "--primary";

    cfg.motors = {
        // Arm actuators (Talons, upper left panel)
        {"Talon 1", "Arm",    MotorType::TALON, MotorRole::ARM,    PanelPosition::UPPER_LEFT, true, 0, 920},
        // Bucket actuator (Talon, upper left panel)
        {"Talon 3", "Bucket", MotorType::TALON, MotorRole::BUCKET, PanelPosition::UPPER_LEFT, true, 0, 700},
        // Drive motors (Krakens, lower panels)
        {"Kraken 1", "Kraken 1", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_LEFT,  false},
        {"Kraken 2", "Kraken 2", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_LEFT,  false},
        {"Kraken 3", "Kraken 3", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_RIGHT, false},
        {"Kraken 4", "Kraken 4", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_RIGHT, false},
    };

    cfg.mechanisms = {
        {"Arm",    MechanismMode::SINGLE, 920, "Talon 1", "", 0},
        {"Bucket", MechanismMode::SINGLE, 920, "Talon 3", "", 0},
    };

    cfg.features.hasLidar = true;
    cfg.features.hasProximityBar = true;
    cfg.features.armShowThreshold = 500;

    return cfg;
}

inline BotConfig backupBot() {
    BotConfig cfg;
    cfg.name = "Backup Bot";
    cfg.cliFlag = "--backup_bot";

    cfg.motors = {
        // Single arm actuator
        {"Talon 1", "Arm",      MotorType::TALON, MotorRole::ARM,    PanelPosition::UPPER_LEFT, true, 0, 920},
        // Single bucket actuator
        {"Talon 3", "Bucket",   MotorType::TALON, MotorRole::BUCKET, PanelPosition::UPPER_RIGHT, false, 0, 700},
        // Drive motors (Krakens)
        {"Kraken 1", "Kraken 1", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_LEFT,  false},
        {"Kraken 2", "Kraken 2", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_LEFT,  false},
        {"Kraken 3", "Kraken 3", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_RIGHT, false},
        {"Kraken 4", "Kraken 4", MotorType::KRAKEN, MotorRole::DRIVE, PanelPosition::LOWER_RIGHT, false},
    };

    cfg.mechanisms = {
        {"Arm",    MechanismMode::SINGLE, 920, "Talon 1", "", 0},
        {"Bucket", MechanismMode::SINGLE, 920, "Talon 3", "", 0},
    };

    cfg.features.hasLidar = true;
    cfg.features.hasProximityBar = true;
    cfg.features.armShowThreshold = 400;

    return cfg;
}

inline BotConfig dumpBot() {
    BotConfig cfg;
    cfg.name = "Dump Bot";
    cfg.cliFlag = "--dump_bot";

    cfg.motors = {
        // Conveyor belt motor
        {"Neo 1", "Dump Bucket", MotorType::FALCON, MotorRole::CONVEYOR, PanelPosition::UPPER_LEFT, true},
        // Drive motors (Falcons)
        {"Falcon 1", "Falcon 1", MotorType::FALCON, MotorRole::DRIVE, PanelPosition::LOWER_LEFT,  false},
        {"Falcon 2", "Falcon 2", MotorType::FALCON, MotorRole::DRIVE, PanelPosition::LOWER_RIGHT,  false},
        {"Falcon 3", "Falcon 3", MotorType::FALCON, MotorRole::DRIVE, PanelPosition::LOWER_LEFT,  false},
        {"Falcon 4", "Falcon 4", MotorType::FALCON, MotorRole::DRIVE, PanelPosition::LOWER_RIGHT, false},
        // You can add more here as the dump bot design evolves
    };

    cfg.mechanisms = {
        // No arm or bucket — conveyor is handled separately
    };

    cfg.features.hasLidar = false;
    cfg.features.hasProximityBar = false;
    cfg.features.hasConveyor = true;
    cfg.features.armShowThreshold = 0;

    return cfg;
}

} // namespace configs


// ============================================================================
// Helper: select config from CLI args
// ============================================================================

inline BotConfig selectConfigFromArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--backup_bot")  return configs::backupBot();
        if (arg == "--dump_bot")    return configs::dumpBot();
    }
    return configs::primaryBot(); // Default
}
