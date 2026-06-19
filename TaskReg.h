#ifndef TASKREG_H
#define TASKREG_H

class TaskReg {
public:
    static bool IsAdmin();
    static void RunAsAdmin(const wchar_t* args);
    static bool Install();
    static void Uninstall();
    // Start a non-elevated resident instance in the interactive user session.
    static bool LaunchAsInteractiveUser();
};

#endif
