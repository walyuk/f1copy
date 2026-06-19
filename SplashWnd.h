#ifndef SPLASHWND_H
#define SPLASHWND_H

#include <windows.h>

class SplashWnd {
public:
    // isStartup=true: 起動時（機能一覧） / false: 終了時（終了メッセージ）
    static void Show(HINSTANCE hInstance, bool isStartup);
};

#endif
