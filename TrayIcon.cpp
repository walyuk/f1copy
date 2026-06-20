#include "TrayIcon.h"
#include "SplashWnd.h"
#include <shellapi.h>
#include "resource.h"

#define WM_APP_TRAYMSG (WM_APP + 1)

static HWND g_hHiddenWnd = NULL;
static NOTIFYICONDATAW g_nid = {0};
static HINSTANCE g_hInst = NULL;
static UINT g_uTaskbarCreatedMsg = 0;

static bool IsShellTrayReady() {
    return FindWindowW(L"Shell_TrayWnd", NULL) != NULL;
}

static bool AddTrayIcon() {
    return Shell_NotifyIconW(NIM_ADD, &g_nid) == TRUE;
}

static bool WaitForShellTray(DWORD timeoutMs) {
    const DWORD pollMs = 100;
    DWORD waited = 0;
    while (waited < timeoutMs) {
        if (IsShellTrayReady())
            return true;
        Sleep(pollMs);
        waited += pollMs;
    }
    return IsShellTrayReady();
}

static bool AddTrayIconWithRetry() {
    WaitForShellTray(60000);

    for (int attempt = 0; attempt < 30; ++attempt) {
        if (AddTrayIcon())
            return true;
        Sleep(1000);
    }
    return false;
}

LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_uTaskbarCreatedMsg) {
        AddTrayIcon();
        return 0;
    }
    if (msg == WM_APP_TRAYMSG) {
        if (lParam == WM_LBUTTONDBLCLK) {
            SplashWnd::Show(g_hInst, SplashMode::Exit);
        }
        return 0;
    }
    if (msg == WM_CLOSE) {
        SplashWnd::Show(g_hInst, SplashMode::Exit);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool TrayIcon::Init(HINSTANCE hInstance, bool keymapPendingReboot) {
    g_hInst = hInstance;
    g_uTaskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = HiddenWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"f1copy_HiddenWnd";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_F1COPY_ICON));
    RegisterClassW(&wc);

    g_hHiddenWnd = CreateWindowW(
        wc.lpszClassName, L"f1copy Hidden Window",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL
    );

    if (!g_hHiddenWnd) return false;

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hHiddenWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP_TRAYMSG;
    g_nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_F1COPY_ICON));
    lstrcpyW(
        g_nid.szTip,
        keymapPendingReboot
            ? L"キーマップが反映されていません。再起動して下さい"
            : L"f1copy (Double click to exit)");

    AddTrayIconWithRetry();
    return true;
}

void TrayIcon::Cleanup() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_hHiddenWnd) {
        DestroyWindow(g_hHiddenWnd);
        g_hHiddenWnd = NULL;
    }
}
