#pragma once
#include <windows.h>
#include <string>

struct Config {
    bool instantDodge = true;
    DWORD tapDurationMs = 25;

    bool toggleSprint = true;
    bool holdSprint = false;
    std::string sprintKey = "LeftAlt";
    bool smartWasd = true;
    bool resumeAfterRoll = false;
    DWORD minHoldDurationMs = 380;
    bool infiniteSprintStamina = true;

    bool chatSuspend = true;

    bool disableTargetSwitch = true;
    bool preventLockBreak = true;

    bool logging = false;
};

extern Config g_config;
void LoadConfig(const std::string& iniPath);
