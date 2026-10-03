#include "memory.h"
#include <cstring>
#include <cstdarg>
#include <fstream>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

static std::string g_logFilePath = "";
static std::mutex g_logMutex;

void SetLogFilePath(const std::string& path) {
    g_logFilePath = path;
}

void LogMessage(const char* fmt, ...) {
    if (g_logFilePath.empty()) return;

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream out(g_logFilePath, std::ios::app);
    if (out.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeStr[32];
        snprintf(timeStr, sizeof(timeStr), "[%02d:%02d:%02d.%03d] ",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        out << timeStr << buffer << std::endl;
    }
}

static inline bool IsValidReadable(uintptr_t addr, size_t size = sizeof(uintptr_t)) {
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    if (addr + size > reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize) return false;
    return true;
}

uintptr_t GetModuleBase(const char* moduleName) {
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName));
}

static bool SafeRead64(uintptr_t addr, uintptr_t* out) {
    if (!IsValidReadable(addr, sizeof(uintptr_t))) return false;
    *out = *reinterpret_cast<const uintptr_t*>(addr);
    return IsValidReadable(*out, 1);
}

bool ReadInt32Safe(uintptr_t address, int32_t* out) {
    if (!IsValidReadable(address, sizeof(int32_t))) return false;
    if (out) *out = *reinterpret_cast<const int32_t*>(address);
    return true;
}

uintptr_t ResolveRipRelative(uintptr_t instructionAddr, int32_t instructionLength, int32_t ripOffset) {
    if (!IsValidReadable(instructionAddr, instructionLength)) return 0;
    int32_t rel = 0;
    if (!ReadInt32Safe(instructionAddr + ripOffset, &rel)) return 0;
    return instructionAddr + instructionLength + rel;
}

// Scans executable sections only: data sections of the game image may contain
// uncommitted or guarded pages, and every pattern here targets code.
static size_t ScanCode(uintptr_t base, const uint8_t* pattern, const char* mask,
                       uintptr_t* results, size_t maxResults) {
    if (!base || !maxResults) return 0;

    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!IsValidReadable(base, sizeof(IMAGE_DOS_HEADER)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (!IsValidReadable(reinterpret_cast<uintptr_t>(nt), sizeof(IMAGE_NT_HEADERS)) || nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    const size_t maskLen = strlen(mask);
    size_t found = 0;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections && found < maxResults; ++s, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;

        const uint8_t* start = reinterpret_cast<const uint8_t*>(base + sec->VirtualAddress);
        size_t size = sec->Misc.VirtualSize;
        if (!IsValidReadable(reinterpret_cast<uintptr_t>(start), 1)) continue;

        for (size_t i = 0; i + maskLen <= size && found < maxResults; ++i) {
            if (mask[0] == 'x' && start[i] != pattern[0]) continue;
            bool match = true;
            for (size_t j = 1; j < maskLen; ++j) {
                if (mask[j] == 'x' && start[i + j] != pattern[j]) {
                    match = false;
                    break;
                }
            }
            if (match) results[found++] = reinterpret_cast<uintptr_t>(start + i);
        }
        // Game code lives in the first executable section; later ones are protection stubs.
        if (found) break;
    }
    return found;
}

uintptr_t FindPattern(const char* moduleName, const uint8_t* pattern, const char* mask) {
    uintptr_t result = 0;
    return ScanCode(GetModuleBase(moduleName), pattern, mask, &result, 1) ? result : 0;
}

static bool WritePatch(uintptr_t addr, const uint8_t* bytes, size_t len) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(addr), len, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    memcpy(reinterpret_cast<void*>(addr), bytes, len);
    VirtualProtect(reinterpret_cast<LPVOID>(addr), len, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPCVOID>(addr), len);
    return true;
}

static uintptr_t g_worldChrManAddr = 0;

void InitMemoryScanner() {
    uintptr_t base = GetModuleBase("eldenring.exe");
    if (!base || g_worldChrManAddr) return;

    static const uint8_t patWorldChr[] = {
        0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x85, 0xC0, 0x74, 0x0F, 0x48, 0x39, 0x88
    };
    uintptr_t matchChr = FindPattern("eldenring.exe", patWorldChr, "xxx????xxxxxxxx");
    if (matchChr) {
        g_worldChrManAddr = ResolveRipRelative(matchChr, 7, 3);
        LogMessage("[ErFluidControls] WorldChrMan resolved via pattern at 0x%p", (void*)g_worldChrManAddr);
    } else {
        g_worldChrManAddr = base + 0x3D69FF8;
        LogMessage("[ErFluidControls] WorldChrMan fallback offset used at 0x%p", (void*)g_worldChrManAddr);
    }
}

// Dynamic fallback, used only when the code patch could not be applied.
void MaintainPlayerStamina() {
    if (!g_worldChrManAddr) InitMemoryScanner();
    if (!g_worldChrManAddr) return;

    uintptr_t worldChr = 0, playerChr = 0, moduleBag = 0, statModule = 0;
    if (!SafeRead64(g_worldChrManAddr, &worldChr)) return;
    if (!SafeRead64(worldChr + 0x1E508, &playerChr)) return;
    if (!SafeRead64(playerChr + 0x190, &moduleBag)) return;
    if (!SafeRead64(moduleBag, &statModule)) return;
    if (!IsValidReadable(statModule + 0x138, 0x24)) return;

    auto stats = reinterpret_cast<volatile int32_t*>(statModule);
    int32_t health = stats[0x138 / 4];
    if (health <= 0) return;

    int32_t stamina = stats[0x154 / 4];
    int32_t staminaMax = stats[0x158 / 4];
    if (staminaMax > 0 && staminaMax < 10000 && stamina >= 0 && stamina < staminaMax) {
        stats[0x154 / 4] = staminaMax;
    }
}

bool ApplySprintStaminaPatch(uintptr_t base) {
    if (!base) base = GetModuleBase("eldenring.exe");
    if (!base) return false;

    // Sprint stamina drain (on foot and mounted):
    // mov rax,[rdi+190h]; mov rcx,[rax]; mov edx,[rcx+154h]; movsx eax,word [rip+X]; sub edx,eax; mov r8b,1
    // "sub edx,eax; mov r8b,1" -> "nop; nop; xor r8d,r8d" leaves stamina unchanged.
    static const uint8_t patSprintDrain[] = {
        0x48, 0x8B, 0x87, 0x90, 0x01, 0x00, 0x00,
        0x48, 0x8B, 0x08,
        0x8B, 0x91, 0x54, 0x01, 0x00, 0x00,
        0x0F, 0xBF, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x2B, 0xD0,
        0x41, 0xB0, 0x01
    };
    static const uint8_t patched[] = { 0x90, 0x90, 0x45, 0x31, 0xC0 };

    uintptr_t matches[4] = {};
    size_t count = ScanCode(base, patSprintDrain, "xxxxxxxxxxxxxxxxxxx????xxxxx", matches, 4);
    if (count == 0 || count > 2) {
        LogMessage("[ErFluidControls] Sprint stamina pattern matches: %u (expected 1-2), patch skipped", (unsigned)count);
        return false;
    }

    int patchedCount = 0;
    for (size_t i = 0; i < count; ++i) {
        uintptr_t target = matches[i] + 23;
        if (WritePatch(target, patched, sizeof(patched))) {
            LogMessage("[ErFluidControls] Sprint stamina drain patch APPLIED at 0x%p", (void*)target);
            patchedCount++;
        }
    }
    return patchedCount > 0;
}

// SwitchLockOn is the only routine that changes the locked target (mouse flick and
// auto-switch after the target dies). Returning at entry keeps the current target;
// every caller ignores its result.
bool ApplyDisableTargetSwitchPatch(uintptr_t base) {
    if (!base) base = GetModuleBase("eldenring.exe");
    if (!base) return false;

    static const uint8_t patSwitchLockOn[] = {
        0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0x90
    };
    uintptr_t matches[2] = {};
    if (ScanCode(base, patSwitchLockOn, "xxxxxxxxxxxxxx", matches, 2) != 1) {
        LogMessage("[ErFluidControls] SwitchLockOn pattern not found or not unique");
        return false;
    }

    static const uint8_t ret[] = { 0xC3 };
    if (!WritePatch(matches[0], ret, sizeof(ret))) return false;
    LogMessage("[ErFluidControls] Disable target switch patch APPLIED at 0x%p", (void*)matches[0]);
    return true;
}

// KeepLockOn: [rsi+2978h] accumulates time without line of sight and the lock
// drops once it exceeds [rsi+2974h]. "addss xmm0,[rsi+2978h]" -> "xorps xmm0,xmm0"
// keeps the timer at zero.
bool ApplyPreventLockBreakPatch(uintptr_t base) {
    if (!base) base = GetModuleBase("eldenring.exe");
    if (!base) return false;

    static const uint8_t patKeepLockOn[] = {
        0xF3, 0x0F, 0x10, 0x44, 0x24, 0x34,
        0xF3, 0x0F, 0x58, 0x86, 0x78, 0x29, 0x00, 0x00
    };
    uintptr_t matches[2] = {};
    if (ScanCode(base, patKeepLockOn, "xxxxxxxxxxxxxx", matches, 2) != 1) {
        LogMessage("[ErFluidControls] KeepLockOn pattern not found or not unique");
        return false;
    }

    static const uint8_t patched[] = { 0x0F, 0x57, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90 };
    uintptr_t target = matches[0] + 6;
    if (!WritePatch(target, patched, sizeof(patched))) return false;
    LogMessage("[ErFluidControls] Prevent lock break patch APPLIED at 0x%p", (void*)target);
    return true;
}
