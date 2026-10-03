#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

uintptr_t GetModuleBase(const char* moduleName);
uintptr_t FindPattern(const char* moduleName, const uint8_t* pattern, const char* mask);
uintptr_t ResolveRipRelative(uintptr_t instructionAddr, int32_t instructionLength, int32_t ripOffset);
bool ReadInt32Safe(uintptr_t address, int32_t* out);

void SetLogFilePath(const std::string& path);
void LogMessage(const char* fmt, ...);

void InitMemoryScanner();
void MaintainPlayerStamina();
bool ApplySprintStaminaPatch(uintptr_t base);
bool ApplyDisableTargetSwitchPatch(uintptr_t base);
bool ApplyPreventLockBreakPatch(uintptr_t base);
