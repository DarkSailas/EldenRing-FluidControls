#include "config.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

Config g_config;

static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void LoadConfig(const std::string& iniPath) {
    std::ifstream file(iniPath);
    if (!file.is_open()) return;

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = Trim(line.substr(0, eqPos));
        std::string val = Trim(line.substr(eqPos + 1));

        if (key == "ChatSuspend") {
            g_config.chatSuspend = (val == "true" || val == "1");
        } else if (section == "Dodge") {
            if (key == "InstantDodge") g_config.instantDodge = (val == "true" || val == "1");
            else if (key == "TapDurationMs") g_config.tapDurationMs = (DWORD)std::strtoul(val.c_str(), nullptr, 10);
        } else if (section == "Sprint") {
            if (key == "ToggleSprint") g_config.toggleSprint = (val == "true" || val == "1");
            else if (key == "HoldSprint") g_config.holdSprint = (val == "true" || val == "1");
            else if (key == "SprintKey") g_config.sprintKey = val;
            else if (key == "SmartWASD") g_config.smartWasd = (val == "true" || val == "1");
            else if (key == "ResumeAfterRoll") g_config.resumeAfterRoll = (val == "true" || val == "1");
            else if (key == "MinHoldDurationMs") g_config.minHoldDurationMs = (DWORD)std::strtoul(val.c_str(), nullptr, 10);
            else if (key == "InfiniteSprintStamina") g_config.infiniteSprintStamina = (val == "true" || val == "1");
        } else if (section == "LockOn") {
            if (key == "DisableTargetSwitch") g_config.disableTargetSwitch = (val == "true" || val == "1");
            else if (key == "PreventLockBreak") g_config.preventLockBreak = (val == "true" || val == "1");
        } else if (section == "Debug") {
            if (key == "Logging") g_config.logging = (val == "true" || val == "1");
        }
    }
}
