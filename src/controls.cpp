#include "controls.h"
#include "config.h"
#include "memory.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

enum ActionType {
    ACTION_NONE,
    ACTION_ROLL,
    ACTION_UPDATE_SPRINT
};

struct ActionItem {
    ActionType type;
};

static std::atomic<bool> g_serviceRunning{false};
static std::thread g_workerThread;
static std::thread g_hookThread;
static std::thread g_staminaThread;
static HHOOK g_hook = nullptr;

static std::mutex g_queueLock;
static std::condition_variable g_queueCv;
static std::queue<ActionItem> g_actionQueue;

static std::atomic<bool> g_sprintToggled{false};
static std::atomic<bool> g_sprintHeld{false};
static std::atomic<bool> g_chatActive{false};
static std::atomic<bool> g_physicalSpaceDown{false};
static std::atomic<bool> g_physicalAltDown{false};
static std::atomic<ULONGLONG> g_spaceDownTime{0};

static std::atomic<bool> g_keyW{false};
static std::atomic<bool> g_keyA{false};
static std::atomic<bool> g_keyS{false};
static std::atomic<bool> g_keyD{false};

static DWORD g_sprintVk = VK_LMENU;

static DWORD ParseSprintKey(const std::string& name) {
    if (name == "LeftShift") return VK_LSHIFT;
    if (name == "CapsLock") return VK_CAPITAL;
    return VK_LMENU;
}

static inline bool IsSprintKey(DWORD vk) {
    return vk == g_sprintVk || (g_sprintVk == VK_LMENU && vk == VK_MENU);
}

static inline bool IsMoving() {
    return g_keyW.load() || g_keyA.load() || g_keyS.load() || g_keyD.load();
}

static void InjectKey(WORD vk, bool keyUp) {
    INPUT ip = {};
    ip.type = INPUT_KEYBOARD;
    ip.ki.wVk = vk;
    ip.ki.wScan = (WORD)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    ip.ki.dwFlags = (keyUp ? KEYEVENTF_KEYUP : 0);
    if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT || 
        vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END) {
        ip.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    SendInput(1, &ip, sizeof(INPUT));
}

static void QueueAction(ActionType type) {
    std::lock_guard<std::mutex> lock(g_queueLock);
    g_actionQueue.push({type});
    g_queueCv.notify_one();
}

static ULONGLONG g_sprintPressTime = 0;

static void ReleaseSprintSafe() {
    if (!g_sprintHeld.load()) return;

    ULONGLONG now = GetTickCount64();
    ULONGLONG elapsedMs = (now >= g_sprintPressTime) ? (now - g_sprintPressTime) : 0;

    if (elapsedMs < g_config.minHoldDurationMs) {
        DWORD waitMs = static_cast<DWORD>(g_config.minHoldDurationMs - elapsedMs);
        for (DWORD i = 0; i < waitMs; i += 10) {
            if (g_physicalSpaceDown.load() || !g_serviceRunning.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (!g_physicalSpaceDown.load()) {
        InjectKey(VK_SPACE, true);
    }
    g_sprintHeld.store(false);
}

static void EngageSprint() {
    if (g_sprintHeld.load() || g_physicalSpaceDown.load()) return;
    if (!IsMoving()) return;

    InjectKey(VK_SPACE, false);
    g_sprintPressTime = GetTickCount64();
    g_sprintHeld.store(true);
}

static void WorkerThreadFunc() {
    while (g_serviceRunning.load()) {
        ActionItem item = {ACTION_NONE};
        {
            std::unique_lock<std::mutex> lock(g_queueLock);
            g_queueCv.wait_for(lock, std::chrono::milliseconds(50), []() {
                return !g_actionQueue.empty() || !g_serviceRunning.load();
            });
            if (!g_serviceRunning.load()) break;
            if (!g_actionQueue.empty()) {
                item = g_actionQueue.front();
                g_actionQueue.pop();
            }
        }

        if (item.type == ACTION_ROLL) {
            if (g_sprintHeld.load()) {
                InjectKey(VK_SPACE, true);
                g_sprintHeld.store(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            InjectKey(VK_SPACE, false);
            std::this_thread::sleep_for(std::chrono::milliseconds(g_config.tapDurationMs));
            InjectKey(VK_SPACE, true);

            bool shouldResume = g_config.holdSprint ? g_physicalAltDown.load() : g_sprintToggled.load();
            if (g_config.resumeAfterRoll && shouldResume && IsMoving()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(320));
                if (shouldResume && IsMoving() && !g_physicalSpaceDown.load()) {
                    EngageSprint();
                }
            }
        } else if (item.type == ACTION_UPDATE_SPRINT) {
            bool isSprintActive = g_config.holdSprint ? g_physicalAltDown.load() : g_sprintToggled.load();
            bool shouldSprint = isSprintActive && (g_config.smartWasd ? IsMoving() : true);
            if (shouldSprint) {
                EngageSprint();
            } else {
                ReleaseSprintSafe();
            }
        }
    }

    if (g_sprintHeld.load()) {
        InjectKey(VK_SPACE, true);
        g_sprintHeld.store(false);
    }
}

static void SprintStaminaThreadFunc() {
    while (g_serviceRunning.load()) {
        {
            bool isSprinting = false;
            if (g_sprintHeld.load() && IsMoving()) {
                isSprinting = true;
            } else if (g_physicalSpaceDown.load() && IsMoving()) {
                ULONGLONG now = GetTickCount64();
                ULONGLONG downTime = g_spaceDownTime.load();
                if (downTime > 0 && (now - downTime) > 350) {
                    isSprinting = true;
                }
            }

            if (isSprinting) {
                MaintainPlayerStamina();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION) return CallNextHookEx(nullptr, nCode, wParam, lParam);

    auto p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

    if (p->flags & LLKHF_INJECTED) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    HWND fg = GetForegroundWindow();
    if (!fg) return CallNextHookEx(nullptr, nCode, wParam, lParam);

    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    if (fgPid != GetCurrentProcessId()) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    if (g_config.chatSuspend && p->vkCode == VK_RETURN && isKeyDown) {
        g_chatActive.store(!g_chatActive.load());
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    if (g_chatActive.load()) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // 1. Instant Dodge on Space
    if (g_config.instantDodge && p->vkCode == VK_SPACE) {
        if (isKeyDown) {
            if (!g_physicalSpaceDown.exchange(true)) {
                g_spaceDownTime.store(GetTickCount64());
                QueueAction(ACTION_ROLL);
            }
            return 1;
        } else if (isKeyUp) {
            g_physicalSpaceDown.store(false);
            g_spaceDownTime.store(0);
            return 1;
        }
    }

    // Track Space for InfiniteStamina even when InstantDodge is off (passthrough to game)
    if (!g_config.instantDodge && p->vkCode == VK_SPACE) {
        if (isKeyDown) {
            g_physicalSpaceDown.store(true);
            g_spaceDownTime.store(GetTickCount64());
        } else if (isKeyUp) {
            g_physicalSpaceDown.store(false);
            g_spaceDownTime.store(0);
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // 2. Sprint on the configured key (Left Alt by default)
    if ((g_config.toggleSprint || g_config.holdSprint) && IsSprintKey(p->vkCode)) {
        if (g_config.holdSprint) {
            if (isKeyDown) {
                if (!g_physicalAltDown.exchange(true)) {
                    QueueAction(ACTION_UPDATE_SPRINT);
                }
                return 1;
            } else if (isKeyUp) {
                g_physicalAltDown.store(false);
                QueueAction(ACTION_UPDATE_SPRINT);
                return 1;
            }
        } else if (g_config.toggleSprint) {
            if (isKeyDown) {
                if (!g_physicalAltDown.exchange(true)) {
                    g_sprintToggled.store(!g_sprintToggled.load());
                    QueueAction(ACTION_UPDATE_SPRINT);
                }
                return 1;
            } else if (isKeyUp) {
                g_physicalAltDown.store(false);
                return 1;
            }
        }
    }

    // Movement tracking
    if (p->vkCode == 'W' || p->vkCode == 'A' || p->vkCode == 'S' || p->vkCode == 'D') {
        bool wasMoving = IsMoving();
        if (p->vkCode == 'W') g_keyW.store(isKeyDown);
        if (p->vkCode == 'A') g_keyA.store(isKeyDown);
        if (p->vkCode == 'S') g_keyS.store(isKeyDown);
        if (p->vkCode == 'D') g_keyD.store(isKeyDown);
        bool nowMoving = IsMoving();

        if ((g_config.toggleSprint || g_config.holdSprint) && (wasMoving != nowMoving)) {
            QueueAction(ACTION_UPDATE_SPRINT);
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static DWORD g_hookThreadId = 0;

static void HookThreadFunc() {
    g_hookThreadId = GetCurrentThreadId();
    HMODULE hm = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)HookThreadFunc, &hm);

    g_hook = SetWindowsHookExA(WH_KEYBOARD_LL, LowLevelKeyboardProc, hm, 0);

    MSG msg;
    while (g_serviceRunning.load() && GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}

void StartControlsService() {
    if (g_serviceRunning.exchange(true)) return;

    g_sprintVk = ParseSprintKey(g_config.sprintKey);

    bool needStaminaFallback = false;
    if (g_config.infiniteSprintStamina) {
        uintptr_t base = GetModuleBase("eldenring.exe");
        bool ok = ApplySprintStaminaPatch(base);
        LogMessage("[ErFluidControls] Infinite sprint stamina patch: %s",
                   ok ? "APPLIED" : "NOT APPLIED (will rely on dynamic fallback)");
        if (!ok) {
            InitMemoryScanner();
            needStaminaFallback = true;
        }
    }

    if (g_config.disableTargetSwitch) {
        uintptr_t base = GetModuleBase("eldenring.exe");
        bool ok = ApplyDisableTargetSwitchPatch(base);
        LogMessage("[ErFluidControls] Disable target switch patch: %s",
                   ok ? "APPLIED" : "NOT APPLIED");
    }

    if (g_config.preventLockBreak) {
        uintptr_t base = GetModuleBase("eldenring.exe");
        bool ok = ApplyPreventLockBreakPatch(base);
        LogMessage("[ErFluidControls] Prevent lock break patch: %s",
                   ok ? "APPLIED" : "NOT APPLIED");
    }

    g_workerThread = std::thread(WorkerThreadFunc);
    g_hookThread = std::thread(HookThreadFunc);
    if (needStaminaFallback) {
        g_staminaThread = std::thread(SprintStaminaThreadFunc);
    }
}

void StopControlsService() {
    if (!g_serviceRunning.exchange(false)) return;

    g_queueCv.notify_all();

    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }

    if (g_workerThread.joinable()) g_workerThread.join();
    if (g_staminaThread.joinable()) g_staminaThread.join();
    if (g_hookThreadId) {
        PostThreadMessageA(g_hookThreadId, WM_QUIT, 0, 0);
    }
    if (g_hookThread.joinable()) {
        g_hookThread.join();
    }
}
