#ifndef SPLASHWND_H
#define SPLASHWND_H

#include <windows.h>

enum class SplashMode {
    Startup,  // 常駐起動時（機能一覧）
    Exit,     // 終了時
    Install,  // -install 完了時（再起動案内）
};

class SplashWnd {
public:
    static void Show(HINSTANCE hInstance, SplashMode mode);
    // スプラッシュが消えるまでメッセージループを回す（Exit / Install 用）
    static void ShowAndWait(HINSTANCE hInstance, SplashMode mode);
};

#endif
