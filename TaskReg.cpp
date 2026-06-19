#include "TaskReg.h"
#include <windows.h>
#include <string>
#include <shellapi.h>
#include <taskschd.h>
#include <lmcons.h>
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "advapi32.lib")

bool TaskReg::IsAdmin() {
    BOOL bIsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &bIsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return bIsAdmin == TRUE;
}

void TaskReg::RunAsAdmin(const wchar_t* args) {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.lpParameters = args;
        sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
    }
}

static bool GetExeDirectory(wchar_t* dir, DWORD cch) {
    if (!GetModuleFileNameW(NULL, dir, cch))
        return false;
    wchar_t* lastSlash = wcsrchr(dir, L'\\');
    if (!lastSlash)
        return false;
    *lastSlash = L'\0';
    return true;
}

bool TaskReg::LaunchAsInteractiveUser() {
    wchar_t szPath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)))
        return false;

    DWORD explorerPid = 0;
    HWND hShell = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!hShell)
        hShell = GetShellWindow();
    if (!hShell)
        return false;

    GetWindowThreadProcessId(hShell, &explorerPid);
    if (!explorerPid)
        return false;

    HANDLE hExplorer = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, explorerPid);
    if (!hExplorer)
        return false;

    HANDLE hToken = NULL;
    if (!OpenProcessToken(hExplorer, TOKEN_DUPLICATE, &hToken)) {
        CloseHandle(hExplorer);
        return false;
    }
    CloseHandle(hExplorer);

    HANDLE hDupToken = NULL;
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hDupToken)) {
        CloseHandle(hToken);
        return false;
    }
    CloseHandle(hToken);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    BOOL ok = CreateProcessAsUserW(
        hDupToken, szPath, NULL, NULL, NULL, FALSE,
        0, NULL, NULL, &si, &pi);

    CloseHandle(hDupToken);
    if (!ok)
        return false;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

bool TaskReg::Install() {
    wchar_t szPath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) return false;

    wchar_t szDir[MAX_PATH];
    if (!GetExeDirectory(szDir, ARRAYSIZE(szDir))) return false;

    wchar_t userName[UNLEN + 1] = {0};
    DWORD userNameLen = UNLEN + 1;
    if (!GetUserNameW(userName, &userNameLen)) return false;
    
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;

    ITaskService *pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) { CoUninitialize(); return false; }

    VARIANT varEmpty;
    VariantInit(&varEmpty);

    hr = pService->Connect(varEmpty, varEmpty, varEmpty, varEmpty);
    if (FAILED(hr)) { pService->Release(); CoUninitialize(); return false; }

    ITaskFolder *pRootFolder = NULL;
    BSTR bstrRoot = SysAllocString(L"\\");
    hr = pService->GetFolder(bstrRoot, &pRootFolder);
    SysFreeString(bstrRoot);
    if (FAILED(hr)) { pService->Release(); CoUninitialize(); return false; }

    BSTR bstrTaskName = SysAllocString(L"f1copy");

    ITaskDefinition *pTask = NULL;
    hr = pService->NewTask(0, &pTask);
    pService->Release();
    if (FAILED(hr)) { SysFreeString(bstrTaskName); pRootFolder->Release(); CoUninitialize(); return false; }

    IPrincipal *pPrincipal = NULL;
    pTask->get_Principal(&pPrincipal);
    if (pPrincipal) {
        BSTR bstrAuthorId = SysAllocString(L"Author");
        pPrincipal->put_Id(bstrAuthorId);
        SysFreeString(bstrAuthorId);
        BSTR bstrUserId = SysAllocString(userName);
        pPrincipal->put_UserId(bstrUserId);
        SysFreeString(bstrUserId);
        pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
        pPrincipal->put_RunLevel(TASK_RUNLEVEL_LUA);
        pPrincipal->Release();
    }

    ITaskSettings *pSettings = NULL;
    pTask->get_Settings(&pSettings);
    if (pSettings) {
        pSettings->put_StartWhenAvailable(VARIANT_TRUE);
        pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
        pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
        
        BSTR bstrPt0s = SysAllocString(L"PT0S");
        pSettings->put_ExecutionTimeLimit(bstrPt0s);
        SysFreeString(bstrPt0s);
        
        pSettings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
        pSettings->Release();
    }

    ITriggerCollection *pTriggerCollection = NULL;
    pTask->get_Triggers(&pTriggerCollection);
    if (pTriggerCollection) {
        ITrigger *pTrigger = NULL;
        pTriggerCollection->Create(TASK_TRIGGER_LOGON, &pTrigger);
        if (pTrigger) {
            BSTR bstrTriggerId = SysAllocString(L"Trigger1");
            pTrigger->put_Id(bstrTriggerId);
            SysFreeString(bstrTriggerId);

            ILogonTrigger *pLogonTrigger = NULL;
            if (SUCCEEDED(pTrigger->QueryInterface(IID_ILogonTrigger, (void**)&pLogonTrigger))) {
                BSTR bstrDelay = SysAllocString(L"PT10S");
                pLogonTrigger->put_Delay(bstrDelay);
                SysFreeString(bstrDelay);
                pLogonTrigger->Release();
            }
            pTrigger->Release();
        }
        pTriggerCollection->Release();
    }

    IActionCollection *pActionCollection = NULL;
    pTask->get_Actions(&pActionCollection);
    if (pActionCollection) {
        IAction *pAction = NULL;
        pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
        if (pAction) {
            IExecAction *pExecAction = NULL;
            pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
            if (pExecAction) {
                BSTR bstrExePath = SysAllocString(szPath);
                pExecAction->put_Path(bstrExePath);
                SysFreeString(bstrExePath);
                BSTR bstrWorkDir = SysAllocString(szDir);
                pExecAction->put_WorkingDirectory(bstrWorkDir);
                SysFreeString(bstrWorkDir);
                pExecAction->Release();
            }
            pAction->Release();
        }
        pActionCollection->Release();
    }

    IRegisteredTask *pRegisteredTask = NULL;
    VARIANT varUser;
    VariantInit(&varUser);
    varUser.vt = VT_BSTR;
    varUser.bstrVal = SysAllocString(userName);
    hr = pRootFolder->RegisterTaskDefinition(
        bstrTaskName,
        pTask,
        TASK_CREATE_OR_UPDATE,
        varUser,
        varEmpty,
        TASK_LOGON_INTERACTIVE_TOKEN,
        varEmpty,
        &pRegisteredTask);
    VariantClear(&varUser);

    if (pRegisteredTask) pRegisteredTask->Release();
    pTask->Release();
    pRootFolder->Release();
    SysFreeString(bstrTaskName);
    CoUninitialize();

    return SUCCEEDED(hr);
}

void TaskReg::Uninstall() {
    if (!IsAdmin()) {
        RunAsAdmin(L"-uninstall");
        return;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return;

    ITaskService *pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (SUCCEEDED(hr)) {
        VARIANT varEmpty;
        VariantInit(&varEmpty);
        if (SUCCEEDED(pService->Connect(varEmpty, varEmpty, varEmpty, varEmpty))) {
            ITaskFolder *pRootFolder = NULL;
            BSTR bstrRoot = SysAllocString(L"\\");
            if (SUCCEEDED(pService->GetFolder(bstrRoot, &pRootFolder))) {
                BSTR bstrTaskName = SysAllocString(L"f1copy");
                pRootFolder->DeleteTask(bstrTaskName, 0);
                SysFreeString(bstrTaskName);
                pRootFolder->Release();
            }
            SysFreeString(bstrRoot);
        }
        pService->Release();
    }
    CoUninitialize();
}
