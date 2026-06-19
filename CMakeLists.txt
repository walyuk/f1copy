cmake_minimum_required(VERSION 3.15)
project(f1copy VERSION 0.91 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)

add_executable(f1copy WIN32
    main.cpp
    SplashWnd.cpp SplashWnd.h
    TrayIcon.cpp TrayIcon.h
    KeyHook.cpp KeyHook.h
    TaskReg.cpp TaskReg.h
    ScancodeMap.cpp ScancodeMap.h
    f1copy.rc
)

target_link_libraries(f1copy PRIVATE user32 shell32 gdi32 advapi32 ole32 oleaut32 taskschd dwmapi)
