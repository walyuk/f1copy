// SplashWnd.cpp - グラデーション＋キーバッジスタイル（f1copy.md テキスト準拠）
#include "SplashWnd.h"
#include "Version.h"
#include <stdio.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

static HWND g_hSplashWnd = NULL;
static SplashMode g_SplashMode = SplashMode::Startup;

// Fade state
static BYTE  g_Alpha     = 0;
static bool  g_FadingIn  = true;
static int   g_HoldTicks = 0;

static const int FADE_TIMER    = 1;
static const int FADE_STEP     = 18;
static const int FADE_INTERVAL = 16;
static const int DISPLAY_MS    = 2000; // f1copy.md: 2秒後に消去

// Color palette
static const COLORREF CLR_BG_TOP   = RGB( 18,  24,  48);
static const COLORREF CLR_BG_BOT   = RGB( 28,  42,  80);
static const COLORREF CLR_ACCENT   = RGB( 82, 148, 255);
static const COLORREF CLR_TITLE    = RGB(255, 255, 255);
static const COLORREF CLR_SUB      = RGB(180, 200, 240);
static const COLORREF CLR_SEP      = RGB( 60,  80, 140);
static const COLORREF CLR_KEY_BG   = RGB( 38,  55, 100);
static const COLORREF CLR_KEY_BDR  = RGB( 82, 148, 255);
static const COLORREF CLR_KEY_TEXT = RGB(220, 235, 255);
static const COLORREF CLR_ARROW    = RGB(100, 160, 255);
static const COLORREF CLR_VAL_TEXT = RGB(140, 210, 130);

static void FillGradient(HDC hdc, const RECT& rc, COLORREF cTop, COLORREF cBot) {
    int r1 = GetRValue(cTop), g1 = GetGValue(cTop), b1 = GetBValue(cTop);
    int r2 = GetRValue(cBot), g2 = GetGValue(cBot), b2 = GetBValue(cBot);
    int h  = rc.bottom - rc.top;
    if (h <= 0) return;
    for (int y = 0; y < h; y++) {
        int r = r1 + (r2 - r1) * y / h;
        int g = g1 + (g2 - g1) * y / h;
        int b = b1 + (b2 - b1) * y / h;
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, rc.left, rc.top + y, NULL);
        LineTo  (hdc, rc.right, rc.top + y);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
    }
}

static void DrawRoundRect(HDC hdc, const RECT& rc, int rx, int ry,
                          COLORREF fill, COLORREF border) {
    HBRUSH hBr  = CreateSolidBrush(fill);
    HPEN   hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH hOldBr  = (HBRUSH)SelectObject(hdc, hBr);
    HPEN   hOldPen = (HPEN)  SelectObject(hdc, hPen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, rx, ry);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static void DrawSeparator(HDC hdc, int y, int W) {
    HPEN hPen = CreatePen(PS_SOLID, 1, CLR_SEP);
    HPEN hOld = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, 40, y, NULL);
    LineTo  (hdc, W - 40, y);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);
}

static void DrawKeyBadge(HDC hdc, int x, int y, int w, int h,
                         const wchar_t* label, HFONT hFont) {
    RECT rc = { x, y, x + w, y + h };
    DrawRoundRect(hdc, rc, 8, 8, CLR_KEY_BG, CLR_KEY_BDR);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_KEY_TEXT);
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOld);
}

static void DrawValue(HDC hdc, int x, int y, int w, int h,
                      const wchar_t* label, HFONT hFont) {
    RECT rc = { x, y, x + w, y + h };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_VAL_TEXT);
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, label, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOld);
}

static void DrawArrow(HDC hdc, int x, int y, int w, int h, HFONT hFont) {
    RECT rc = { x, y, x + w, y + h };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_ARROW);
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, L"\u2192", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOld);
}

static void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int W = rc.right;
    int H = rc.bottom;

    HDC     hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbm    = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    FillGradient(hdcMem, rc, CLR_BG_TOP, CLR_BG_BOT);

    RECT rcBar = { 0, 0, W, 4 };
    HBRUSH hAcc = CreateSolidBrush(CLR_ACCENT);
    FillRect(hdcMem, &rcBar, hAcc);
    DeleteObject(hAcc);

    LOGFONTW lfTitle = {};
    lfTitle.lfHeight  = -28;
    lfTitle.lfWeight  = FW_BOLD;
    lfTitle.lfQuality = CLEARTYPE_QUALITY;
    lstrcpyW(lfTitle.lfFaceName, L"Segoe UI");
    HFONT hFontTitle = CreateFontIndirectW(&lfTitle);

    LOGFONTW lfSub = {};
    lfSub.lfHeight  = -14;
    lfSub.lfWeight  = FW_NORMAL;
    lfSub.lfQuality = CLEARTYPE_QUALITY;
    lstrcpyW(lfSub.lfFaceName, L"Segoe UI");
    HFONT hFontSub = CreateFontIndirectW(&lfSub);

    LOGFONTW lfKey = {};
    lfKey.lfHeight  = -15;
    lfKey.lfWeight  = FW_SEMIBOLD;
    lfKey.lfQuality = CLEARTYPE_QUALITY;
    lstrcpyW(lfKey.lfFaceName, L"Segoe UI");
    HFONT hFontKey = CreateFontIndirectW(&lfKey);

    LOGFONTW lfArrow = {};
    lfArrow.lfHeight  = -16;
    lfArrow.lfWeight  = FW_NORMAL;
    lfArrow.lfQuality = CLEARTYPE_QUALITY;
    lstrcpyW(lfArrow.lfFaceName, L"Segoe UI Symbol");
    HFONT hFontArrow = CreateFontIndirectW(&lfArrow);

    if (g_SplashMode == SplashMode::Startup) {
        // 起動時: F1COPY Ver X + キー割り当て一覧
        {
            RECT rcTitle = { 0, 28, W, 68 };
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, CLR_TITLE);
            HFONT hOld = (HFONT)SelectObject(hdcMem, hFontTitle);
            DrawTextW(hdcMem, L"F1COPY", -1, &rcTitle,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMem, hOld);
        }
        {
            wchar_t subLine[64];
            swprintf_s(subLine, L"Ver %s", F1COPY_VERSION);
            RECT rcVer = { 0, 64, W, 88 };
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, CLR_SUB);
            HFONT hOld = (HFONT)SelectObject(hdcMem, hFontSub);
            DrawTextW(hdcMem, subLine, -1, &rcVer,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMem, hOld);
        }
        DrawSeparator(hdcMem, 92, W);

        const int ROW_H   = 36;
        const int KEY_W   = 90;
        const int KEY_H   = 28;
        const int ARW_W   = 30;
        const int VAL_W   = 120;
        const int START_Y = 104;

        struct Entry { const wchar_t* key; const wchar_t* val; };
        Entry entries[] = {
            { L"F1",         L"Ctrl+C"   },
            { L"F2",         L"Ctrl+V"   },
            { L"CapsLock",   L"Ctrl"     },
            { L"ScrollLock", L"CapsLock" },
        };
        int nEntries = 4;
        int blockW = KEY_W + ARW_W + VAL_W;
        int bx     = (W - blockW) / 2;

        for (int i = 0; i < nEntries; i++) {
            int cy = START_Y + i * ROW_H;
            int kx = bx;
            int ax = kx + KEY_W + 2;
            int vx = ax + ARW_W + 2;
            DrawKeyBadge(hdcMem, kx, cy + (ROW_H - KEY_H) / 2, KEY_W, KEY_H,
                         entries[i].key, hFontKey);
            DrawArrow(hdcMem, ax, cy, ARW_W, ROW_H, hFontArrow);
            DrawValue(hdcMem, vx, cy, VAL_W, ROW_H, entries[i].val, hFontKey);
        }
    } else if (g_SplashMode == SplashMode::Exit) {
        DrawSeparator(hdcMem, 48, W);
        {
            RECT rcMsg = { 0, 58, W, 98 };
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, CLR_TITLE);
            HFONT hOld = (HFONT)SelectObject(hdcMem, hFontTitle);
            DrawTextW(hdcMem, L"F1COPY.EXE 終了します", -1, &rcMsg,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMem, hOld);
        }
        DrawSeparator(hdcMem, 108, W);
    } else {
        // -install 完了: 再起動案内
        DrawSeparator(hdcMem, 28, W);
        {
            RECT rcTitle = { 0, 38, W, 76 };
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, CLR_TITLE);
            HFONT hOld = (HFONT)SelectObject(hdcMem, hFontTitle);
            DrawTextW(hdcMem, L"インストール完了", -1, &rcTitle,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMem, hOld);
        }
        {
            RECT rcMsg = { 32, 84, W - 32, 156 };
            SetBkMode(hdcMem, TRANSPARENT);
            SetTextColor(hdcMem, CLR_SUB);
            HFONT hOld = (HFONT)SelectObject(hdcMem, hFontSub);
            DrawTextW(hdcMem,
                      L"PC を再起動してください\n"
                      L"(CapsLock / ScrollLock の設定を反映)",
                      -1, &rcMsg, DT_CENTER | DT_WORDBREAK);
            SelectObject(hdcMem, hOld);
        }
        DrawSeparator(hdcMem, 166, W);
    }

    BitBlt(hdc, 0, 0, W, H, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);

    DeleteObject(hFontTitle);
    DeleteObject(hFontSub);
    DeleteObject(hFontKey);
    DeleteObject(hFontArrow);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK SplashWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_Alpha     = 0;
            g_FadingIn  = true;
            g_HoldTicks = 0;
            SetLayeredWindowAttributes(hwnd, 0, g_Alpha, LWA_ALPHA);
            SetTimer(hwnd, FADE_TIMER, FADE_INTERVAL, NULL);
            break;

        case WM_TIMER: {
            int holdMax = DISPLAY_MS / FADE_INTERVAL;

            if (g_FadingIn) {
                int next = (int)g_Alpha + FADE_STEP;
                if (next >= 240) {
                    next       = 240;
                    g_FadingIn = false;
                }
                g_Alpha = (BYTE)next;
                SetLayeredWindowAttributes(hwnd, 0, g_Alpha, LWA_ALPHA);
            } else {
                g_HoldTicks++;
                if (g_HoldTicks >= holdMax) {
                    int next = (int)g_Alpha - FADE_STEP;
                    if (next <= 0) {
                        KillTimer(hwnd, FADE_TIMER);
                        DestroyWindow(hwnd);
                        return 0;
                    }
                    g_Alpha = (BYTE)next;
                    SetLayeredWindowAttributes(hwnd, 0, g_Alpha, LWA_ALPHA);
                }
            }
            break;
        }

        case WM_KEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            KillTimer(hwnd, FADE_TIMER);
            DestroyWindow(hwnd);
            break;

        case WM_PAINT:
            OnPaint(hwnd);
            break;

        case WM_DESTROY:
            g_hSplashWnd = NULL;
            if (g_SplashMode != SplashMode::Startup)
                PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static int SplashHeight(SplashMode mode) {
    switch (mode) {
    case SplashMode::Startup: return 260;
    case SplashMode::Install: return 200;
    default:                  return 160;
    }
}

void SplashWnd::Show(HINSTANCE hInstance, SplashMode mode) {
    g_SplashMode = mode;

    WNDCLASSW wc = {};
    if (!GetClassInfoW(hInstance, L"f1copy_SplashWnd", &wc)) {
        wc.lpfnWndProc   = SplashWndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"f1copy_SplashWnd";
        RegisterClassW(&wc);
    }

    int w = 520;
    int h = SplashHeight(mode);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    if (g_hSplashWnd)
        DestroyWindow(g_hSplashWnd);

    g_hSplashWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"f1copy_SplashWnd", NULL,
        WS_POPUP | WS_VISIBLE,
        x, y, w, h,
        NULL, NULL, hInstance, NULL);

    if (g_hSplashWnd) {
        DWORD attr = DWMWCP_ROUND;
        DwmSetWindowAttribute(g_hSplashWnd,
            DWMWA_WINDOW_CORNER_PREFERENCE, &attr, sizeof(attr));
        UpdateWindow(g_hSplashWnd);
    }
}

void SplashWnd::ShowAndWait(HINSTANCE hInstance, SplashMode mode) {
    Show(hInstance, mode);
    if (mode == SplashMode::Startup)
        return;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
