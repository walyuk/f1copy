@echo off
set "VSCMD_START_DIR=%CD%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64

if %errorlevel% neq 0 (
    echo Visual Studio Community not found or VsDevCmd.bat failed
    exit /b %errorlevel%
)

rc.exe f1copy.rc

cl.exe /utf-8 /O2 /EHsc /W3 /D "UNICODE" /D "_UNICODE" main.cpp SplashWnd.cpp TrayIcon.cpp KeyHook.cpp TaskReg.cpp ScancodeMap.cpp f1copy.res user32.lib shell32.lib gdi32.lib advapi32.lib ole32.lib oleaut32.lib dwmapi.lib /Fe:f1copy.exe

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b %errorlevel%
)
echo Build succeeded!
