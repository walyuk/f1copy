#include <windows.h>
#include <string>
#include <shellapi.h>
#include "TaskReg.h"
#include "ScancodeMap.h"
#include "TrayIcon.h"
#include "KeyHook.h"
#include "SplashWnd.h"

static const wchar_t* SINGLE_INSTANCE_MUTEX = L"Global\\f1copy_mutex_2026";
static const wchar_t* HIDDEN_WND_CLASS      = L"f1copy_HiddenWnd";

// Acquire a session-wide mutex visible across integrity levels (admin / standard).
static HANDLE AcquireSingleInstanceMutex() {
    SECURITY_DESCRIPTOR sd;
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
        return NULL;
    if (!SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE))
        return NULL;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), &sd, FALSE };
    HANDLE hMutex = CreateMutexW(&sa, TRUE, SINGLE_INSTANCE_MUTEX);
    if (!hMutex)
        return NULL;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return NULL;
    }
    return hMutex;
}

static bool IsAnotherResidentInstanceRunning() {
    if (FindWindowExW(HWND_MESSAGE, NULL, HIDDEN_WND_CLASS, NULL))
        return true;
    if (FindWindowW(HIDDEN_WND_CLASS, NULL))
        return true;

    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, SINGLE_INSTANCE_MUTEX);
    if (hMutex) {
        CloseHandle(hMutex);
        return true;
    }
    return false;
}

static HWND FindResidentWindow() {
    HWND hWnd = FindWindowExW(HWND_MESSAGE, NULL, HIDDEN_WND_CLASS, NULL);
    if (hWnd)
        return hWnd;
    return FindWindowW(HIDDEN_WND_CLASS, NULL);
}

static void ShowHookInstallError() {
    MessageBoxW(
        NULL,
        L"キーボードフックの設定に失敗しました。\n"
        L"他のキーボード関連ソフトとの競合、またはセキュリティソフトの制限が原因の可能性があります。",
        L"f1copy",
        MB_OK | MB_ICONERROR);
}

static void ShowInstallError(const wchar_t* detail) {
    MessageBoxW(NULL, detail, L"f1copy", MB_OK | MB_ICONERROR);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool install   = false;
    bool uninstall = false;

    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if      (arg == L"-install")   install   = true;
            else if (arg == L"-uninstall") uninstall = true;
        }
        LocalFree(argv);
    }

    // --------------------------------------------------------------------
    // Uninstall: restore Scancode Map + remove Task Scheduler entry, then quit
    // --------------------------------------------------------------------
    if (uninstall) {
        if (!TaskReg::IsAdmin()) {
            TaskReg::RunAsAdmin(L"-uninstall");
            return 0;
        }
        ScancodeMap::Uninstall();
        TaskReg::Uninstall();
        HWND hWnd = FindResidentWindow();
        if (hWnd) PostMessageW(hWnd, WM_CLOSE, 0, 0);
        return 0;
    }

    // --------------------------------------------------------------------
    // Install: write Scancode Map + register Task Scheduler entry
    // --------------------------------------------------------------------
    if (install) {
        if (!TaskReg::IsAdmin()) {
            TaskReg::RunAsAdmin(L"-install");
            return 0;
        }
        if (!ScancodeMap::Install()) {
            ShowInstallError(L"Scancode Map の登録に失敗しました。");
            return 1;
        }
        if (!TaskReg::Install()) {
            ShowInstallError(L"タスクスケジューラへの登録に失敗しました。");
            return 1;
        }
        SplashWnd::ShowAndWait(hInstance, SplashMode::Install);
        TaskReg::LaunchAsInteractiveUser();
        return 0;
    }

    // --------------------------------------------------------------------
    // Resident operation (normal launch or post-install)
    // --------------------------------------------------------------------
    if (IsAnotherResidentInstanceRunning())
        return 0;

    HANDLE hMutex = AcquireSingleInstanceMutex();
    if (!hMutex)
        return 0;

    if (!TrayIcon::Init(hInstance)) {
        CloseHandle(hMutex);
        return 1;
    }

    if (!KeyHook::Install()) {
        ShowHookInstallError();
        TrayIcon::Cleanup();
        CloseHandle(hMutex);
        return 1;
    }

    SplashWnd::Show(hInstance, SplashMode::Startup);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KeyHook::Uninstall();
    TrayIcon::Cleanup();
    CloseHandle(hMutex);
    return (int)msg.wParam;
}
