#define _CRT_SECURE_NO_WARNINGS
#include "KeyHook.h"
#include <windows.h>

// ---------------------------------------------------------------------------
// CapsLock -> Ctrl and ScrollLock -> CapsLock are handled via Scancode Map
// (ScancodeMap::Install).
//
//   F1  -->  Ctrl+C  (Copy)
//   F2  -->  Ctrl+V  (Paste)
//
// When Win / Alt / Ctrl are already held the key passes through unchanged so
// that native shortcuts (Win+F1 = Help, Ctrl+F1, Alt+F1, etc.) keep working.
// ---------------------------------------------------------------------------

#define INJECTED_KEY_INFO 0x12345678

static HHOOK g_hHook          = NULL;
static bool  g_IsF1Down       = false;
static bool  g_IsF2Down       = false;
static bool  g_IsLCtrlDown    = false;
static bool  g_IsRCtrlDown    = false;

static INPUT MakeKeyInput(WORD vk, bool isDown) {
    INPUT inp      = { 0 };
    inp.type       = INPUT_KEYBOARD;
    inp.ki.wVk     = vk;
    inp.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
    inp.ki.dwExtraInfo = INJECTED_KEY_INFO;
    return inp;
}

static bool IsFromRealCtrlKey(const KBDLLHOOKSTRUCT* pKey) {
    if (pKey->vkCode == VK_LCONTROL)
        return pKey->scanCode == 0x1D;
    if (pKey->vkCode == VK_RCONTROL)
        return (pKey->scanCode == 0x11D) || ((pKey->flags & LLKHF_EXTENDED) != 0);
    return false;
}

// Other keyboard utilities (e.g. SaiKana) may swallow key-up events.
// Reconcile tracked state with the OS async key state.
static void SyncTrackedKeyStates() {
    if (g_IsLCtrlDown && !(GetAsyncKeyState(VK_LCONTROL) & 0x8000))
        g_IsLCtrlDown = false;
    if (g_IsRCtrlDown && !(GetAsyncKeyState(VK_RCONTROL) & 0x8000))
        g_IsRCtrlDown = false;
    if (g_IsF1Down && !(GetAsyncKeyState(VK_F1) & 0x8000))
        g_IsF1Down = false;
    if (g_IsF2Down && !(GetAsyncKeyState(VK_F2) & 0x8000))
        g_IsF2Down = false;
}

static bool IsRealCtrlHeld() {
    return (g_IsLCtrlDown && (GetAsyncKeyState(VK_LCONTROL) & 0x8000))
        || (g_IsRCtrlDown && (GetAsyncKeyState(VK_RCONTROL) & 0x8000));
}

static void SendCtrlKey(WORD vk) {
    if (IsRealCtrlHeld()) {
        INPUT inputs[2] = { MakeKeyInput(vk, true), MakeKeyInput(vk, false) };
        SendInput(2, inputs, sizeof(INPUT));
    } else {
        INPUT inputs[4] = {
            MakeKeyInput(VK_LCONTROL, true),
            MakeKeyInput(vk,          true),
            MakeKeyInput(vk,          false),
            MakeKeyInput(VK_LCONTROL, false)
        };
        SendInput(4, inputs, sizeof(INPUT));
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode != HC_ACTION)
        return CallNextHookEx(g_hHook, nCode, wParam, lParam);

    KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
    WORD  vk         = (WORD)pKey->vkCode;
    bool  isDown     = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

    if (pKey->dwExtraInfo == INJECTED_KEY_INFO)
        return CallNextHookEx(g_hHook, nCode, wParam, lParam);

    SyncTrackedKeyStates();

    if (IsFromRealCtrlKey(pKey)) {
        if (vk == VK_LCONTROL) g_IsLCtrlDown = isDown;
        if (vk == VK_RCONTROL) g_IsRCtrlDown = isDown;
    }

    if (vk == VK_F1 || vk == VK_F2) {
        if ((pKey->flags & LLKHF_INJECTED) != 0)
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);

        bool hasWinAlt = (GetAsyncKeyState(VK_LWIN)  & 0x8000) ||
                         (GetAsyncKeyState(VK_RWIN)  & 0x8000) ||
                         (GetAsyncKeyState(VK_LMENU) & 0x8000) ||
                         (GetAsyncKeyState(VK_RMENU) & 0x8000);
        if (hasWinAlt)
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);

        bool& physDown = (vk == VK_F1) ? g_IsF1Down : g_IsF2Down;
        bool realCtrlDown = IsRealCtrlHeld();

        if (realCtrlDown)
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);

        if (isDown) {
            if (!physDown) {
                physDown = true;
                SendCtrlKey(vk == VK_F1 ? 'C' : 'V');
            }
        } else {
            physDown = false;
        }
        return 1;
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

bool KeyHook::Install() {
    g_IsF1Down = false;
    g_IsF2Down = false;
    g_IsLCtrlDown = false;
    g_IsRCtrlDown = false;

    g_hHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                GetModuleHandle(NULL), 0);
    return g_hHook != NULL;
}

void KeyHook::Uninstall() {
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
        g_hHook    = NULL;
        g_IsF1Down = false;
        g_IsF2Down = false;
        g_IsLCtrlDown = false;
        g_IsRCtrlDown = false;
    }
}
