#ifndef INPUT_MAPPER_HPP
#define INPUT_MAPPER_HPP

/*
 * InputMapper.hpp
 * 
 * Joystick/Controller input rebinding system.
 * 
 * Two uses:
 *   1) Standalone rebind tool (input_rebind_tool.cpp) - interactive SDL+terminal
 *      program that lets users discover inputs and save a JSON config.
 *   2) Integration into control.cpp - loads the saved JSON config and replaces
 *      the hardcoded remapJoystickInputs() logic with a data-driven lookup table.
 *
 * Depends on: nlohmann/json.hpp (already in your project), SDL2
 */

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <nlohmann/json.hpp>

// ─── Data Structures ──────────────────────────────────────────────────────────

struct AxisMapping {
    int inputJoystick;   // Physical SDL joystick index (or virtual index for controller sticks)
    int inputAxis;       // Physical SDL axis index
    int outputJoystick;  // The virtual joystick index the robot expects (0 or 1)
    int outputAxis;      // The virtual axis index the robot expects
    std::string action;  // Human-readable label e.g. "Roll", "Arm Up/Down"
    bool invert = false; // Invert the axis value
};

struct ButtonMapping {
    int inputJoystick;
    int inputButton;
    std::string action;  // e.g. "E-Stop", "Toggle Autonomy"
    int hatIndex = -1;   // If >= 0, this is a hat mapping, not a button
    int hatDirection = 0;
};

struct InputConfig {
    // Global settings
    bool isController  = false;
    bool twoJoysticks  = false;
    bool useAltLayout  = false;
    bool invertY       = false;
    int  deadZone      = 4000;

    // Mappings
    std::vector<AxisMapping>   axisMappings;
    std::vector<ButtonMapping> buttonMappings;

    // ── Serialization ──────────────────────────────────────────────────────

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["version"] = 1;

        j["controller"]["isController"] = isController;
        j["controller"]["twoJoysticks"] = twoJoysticks;
        j["controller"]["useAltLayout"] = useAltLayout;
        j["controller"]["invertY"]      = invertY;
        j["controller"]["deadZone"]     = deadZone;

        j["axisMappings"] = nlohmann::json::array();
        for (auto& m : axisMappings) {
            j["axisMappings"].push_back({
                {"inputJoystick",  m.inputJoystick},
                {"inputAxis",     m.inputAxis},
                {"outputJoystick", m.outputJoystick},
                {"outputAxis",    m.outputAxis},
                {"action",        m.action},
                {"invert",        m.invert}
            });
        }

        j["buttonMappings"] = nlohmann::json::array();
        for (auto& m : buttonMappings) {
            nlohmann::json bj = {
                {"inputJoystick", m.inputJoystick},
                {"inputButton",   m.inputButton},
                {"action",        m.action}
            };
            if (m.hatIndex >= 0) {
                bj["hatIndex"]     = m.hatIndex;
                bj["hatDirection"] = m.hatDirection;
            }
            j["buttonMappings"].push_back(bj);
        }

        return j;
    }

    static InputConfig fromJson(const nlohmann::json& j) {
        InputConfig cfg;

        if (j.contains("controller")) {
            auto& c = j["controller"];
            if (c.contains("isController"))  cfg.isController  = c["isController"].get<bool>();
            if (c.contains("twoJoysticks"))  cfg.twoJoysticks  = c["twoJoysticks"].get<bool>();
            if (c.contains("useAltLayout"))  cfg.useAltLayout  = c["useAltLayout"].get<bool>();
            if (c.contains("invertY"))       cfg.invertY       = c["invertY"].get<bool>();
            if (c.contains("deadZone"))      cfg.deadZone      = c["deadZone"].get<int>();
        }

        if (j.contains("axisMappings")) {
            for (auto& m : j["axisMappings"]) {
                AxisMapping am;
                am.inputJoystick  = m["inputJoystick"].get<int>();
                am.inputAxis      = m["inputAxis"].get<int>();
                am.outputJoystick = m["outputJoystick"].get<int>();
                am.outputAxis     = m["outputAxis"].get<int>();
                am.action         = m.value("action", "");
                am.invert         = m.value("invert", false);
                cfg.axisMappings.push_back(am);
            }
        }

        if (j.contains("buttonMappings")) {
            for (auto& m : j["buttonMappings"]) {
                ButtonMapping bm;
                bm.inputJoystick = m["inputJoystick"].get<int>();
                bm.inputButton   = m["inputButton"].get<int>();
                bm.action        = m.value("action", "");
                bm.hatIndex      = m.value("hatIndex", -1);
                bm.hatDirection  = m.value("hatDirection", 0);
                cfg.buttonMappings.push_back(bm);
            }
        }

        return cfg;
    }

    // ── File I/O ───────────────────────────────────────────────────────────

    bool saveToFile(const std::string& path) const {
        try {
            std::ofstream file(path);
            if (!file.is_open()) {
                std::cerr << "InputConfig: Cannot open " << path << " for writing" << std::endl;
                return false;
            }
            file << toJson().dump(2) << std::endl;
            file.close();
            std::cout << "InputConfig: Saved to " << path << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "InputConfig: Save error: " << e.what() << std::endl;
            return false;
        }
    }

    static InputConfig loadFromFile(const std::string& path) {
        InputConfig cfg;
        try {
            std::ifstream file(path);
            if (!file.is_open()) {
                std::cerr << "InputConfig: Cannot open " << path << " — using defaults" << std::endl;
                return cfg;
            }
            nlohmann::json j;
            file >> j;
            cfg = fromJson(j);
            std::cout << "InputConfig: Loaded " << cfg.axisMappings.size() << " axis mappings, "
                      << cfg.buttonMappings.size() << " button mappings from " << path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "InputConfig: Load error: " << e.what() << std::endl;
        }
        return cfg;
    }

    // ── Runtime Remapping ──────────────────────────────────────────────────

    // Build a fast lookup table: key = (inputJoystick << 8 | inputAxis)
    // value = (outputJoystick, outputAxis, invert)
    struct RemapResult {
        uint8_t which;
        uint8_t axis;
        bool    invert;
    };

    std::map<int, RemapResult> buildRemapTable() const {
        std::map<int, RemapResult> table;
        for (auto& m : axisMappings) {
            int key = (m.inputJoystick << 8) | m.inputAxis;
            table[key] = { (uint8_t)m.outputJoystick, (uint8_t)m.outputAxis, m.invert };
        }
        return table;
    }

    // Direct remap function — replaces remapJoystickInputs()
    // Returns true if a mapping was found (which/axis are modified in-place).
    // Returns false if no mapping exists (which/axis are unchanged — passthrough).
    bool remap(uint8_t* which, uint8_t* axis, bool* invert = nullptr) const {
        int key = (*which << 8) | *axis;
        auto it = remapTable_.find(key);
        if (it != remapTable_.end()) {
            *which = it->second.which;
            *axis  = it->second.axis;
            if (invert) *invert = it->second.invert;
            return true;
        }
        return false;
    }

    // Call this after loading to pre-build the lookup table
    void buildLookup() {
        remapTable_ = buildRemapTable();
    }

private:
    std::map<int, RemapResult> remapTable_;
};


// ─── Default Config Factory ───────────────────────────────────────────────────

inline InputConfig makeDefaultConfig_TwoJoysticks() {
    InputConfig cfg;
    cfg.twoJoysticks = true;
    cfg.isController = false;
    cfg.deadZone = 4000;

    // Joystick 0 Axis 0 → Roll  (output: joystick 0 axis 0)
    // Joystick 0 Axis 1 → Pitch (output: joystick 0 axis 1)
    // Joystick 1 Axis 0 → Bucket (output: joystick 1 axis 0)
    // Joystick 1 Axis 1 → Arm    (output: joystick 1 axis 1)
    cfg.axisMappings = {
        {0, 0, 0, 0, "Roll",   false},
        {0, 1, 0, 1, "Pitch",  false},
        {1, 0, 1, 0, "Bucket", false},
        {1, 1, 1, 1, "Arm",    false},
    };
    return cfg;
}

inline InputConfig makeDefaultConfig_Controller() {
    InputConfig cfg;
    cfg.twoJoysticks = false;
    cfg.isController = true;
    cfg.deadZone = 4000;

    // Controller: axes 0,1 = left stick; axes 2,3 = right stick (single device)
    // Left stick  → Roll/Pitch (output joystick 0)
    // Right stick → Bucket/Arm (output joystick 1)
    cfg.axisMappings = {
        {0, 0, 0, 0, "Roll",   false},
        {0, 1, 0, 1, "Pitch",  false},
        {0, 2, 1, 0, "Bucket", false},
        {0, 3, 1, 1, "Arm",    false},
    };
    return cfg;
}

inline InputConfig makeDefaultConfig_SingleJoystick() {
    InputConfig cfg;
    cfg.twoJoysticks = false;
    cfg.isController = false;
    cfg.deadZone = 4000;

    // Single joystick: axis 0,1 = roll/pitch; axis 2 (twist) = bucket
    cfg.axisMappings = {
        {0, 0, 0, 0, "Roll",   false},
        {0, 1, 0, 1, "Pitch",  false},
        {0, 2, 1, 0, "Bucket", false},
    };
    return cfg;
}

#endif // INPUT_MAPPER_HPP