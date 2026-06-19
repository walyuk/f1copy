#define _CRT_SECURE_NO_WARNINGS
#include "KeyHook.h"
#include <windows.h>

// ---------------------------------------------------------------------------
// CapsLock and ScrollLock remapping is handled at the OS level via Scancode Map
// (set by ScancodeMap::Install).  This hook only deals with F1 and F2.
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
static bool  g_IsCapsDown     = false;
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

static bool IsFromCapsPhysical(const KBDLLHOOKSTRUCT* pKey) {
    // CapsLock physical scancode is 0x3A.
    return pKey->scanCode == 0x3A;
}

// Send plain Fn key while CapsLock-remapped Ctrl is physically held.
// We temporarily release Ctrl to avoid turning Fn into Ctrl+Fn.
static void SendPlainFnBypassingCtrl(WORD vk) {
    INPUT inputs[5] = {
        MakeKeyInput(VK_LCONTROL, false),
        MakeKeyInput(VK_RCONTROL, false),
        MakeKeyInput(vk,          true),
        MakeKeyInput(vk,          false),
        MakeKeyInput(VK_LCONTROL, true),
    };
    SendInput(5, inputs, sizeof(INPUT));
}

// Send Ctrl+key as one atomic SendInput call so no physical key can interleave.
static void SendCtrlKey(WORD vk) {
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        // Ctrl is already physically held (e.g. user pressed real Ctrl+F1).
        // Just send the key itself; the existing Ctrl covers it.
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
    bool  isInjected = (pKey->dwExtraInfo == INJECTED_KEY_INFO)
                    || ((pKey->flags & LLKHF_INJECTED) != 0);

    // Pass through injected events from this app and other keyboard utilities.
    if (isInjected)
        return CallNextHookEx(g_hHook, nCode, wParam, lParam);

    // Track physical CapsLock state (even though it is remapped at OS level).
    if (IsFromCapsPhysical(pKey))
        g_IsCapsDown = isDown;

    // Track real Ctrl keys only (exclude CapsLock-originated remapped Ctrl).
    if ((vk == VK_LCONTROL || vk == VK_RCONTROL) && !IsFromCapsPhysical(pKey)) {
        if (vk == VK_LCONTROL) g_IsLCtrlDown = isDown;
        if (vk == VK_RCONTROL) g_IsRCtrlDown = isDown;
    }

    if (vk == VK_F1 || vk == VK_F2) {
        bool hasWinAlt = (GetAsyncKeyState(VK_LWIN)  & 0x8000) ||
                         (GetAsyncKeyState(VK_RWIN)  & 0x8000) ||
                         (GetAsyncKeyState(VK_LMENU) & 0x8000) ||
                         (GetAsyncKeyState(VK_RMENU) & 0x8000);
        if (hasWinAlt)
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);

        bool& physDown = (vk == VK_F1) ? g_IsF1Down : g_IsF2Down;
        bool realCtrlDown = g_IsLCtrlDown || g_IsRCtrlDown;

        // CapsLock+F1/F2 should behave as native F1/F2.
        if (g_IsCapsDown && !realCtrlDown) {
            if (isDown && !physDown) {
                physDown = true;
                SendPlainFnBypassingCtrl(vk);
            } else if (!isDown) {
                physDown = false;
            }
            return 1; // Suppress raw event; we already emitted plain Fn.
        }

        // Real Ctrl combinations should pass through unchanged.
        if (realCtrlDown)
            return CallNextHookEx(g_hHook, nCode, wParam, lParam);

        if (isDown && !physDown) {
            physDown = true;
            SendCtrlKey(vk == VK_F1 ? 'C' : 'V');
        } else if (!isDown) {
            physDown = false;
        }
        return 1; // Suppress the raw F1/F2 event.
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

bool KeyHook::Install() {
    g_IsF1Down = false;
    g_IsF2Down = false;
    g_IsCapsDown = false;
    g_IsLCtrlDown = false;
    g_IsRCtrlDown = false;

    // Delete the debug log from the previous session (no-op if absent).
    char logPath[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, logPath, MAX_PATH);
    char* p = strrchr(logPath, '\\');
    if (p) strcpy(p + 1, "f1copy_debug.log");
    DeleteFileA(logPath);

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
        g_IsCapsDown = false;
        g_IsLCtrlDown = false;
        g_IsRCtrlDown = false;
    }
}
