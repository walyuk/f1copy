#include "ScancodeMap.h"
#include <windows.h>
#include <vector>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// Registry location for Scancode Map
// ---------------------------------------------------------------------------
static const wchar_t* REG_KEY  = L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout";
static const wchar_t* REG_VAL  = L"Scancode Map";

// Backup of the pre-install Scancode Map (restored on -uninstall).
static const wchar_t* BACKUP_KEY = L"SOFTWARE\\f1copy";
static const wchar_t* BACKUP_VAL = L"OriginalScancodeMap";
static const wchar_t* BACKUP_PRESENT_VAL = L"OriginalScancodeMapPresent";

// ---------------------------------------------------------------------------
// Our remappings (CapsLock -> Ctrl is handled in KeyHook):
//   ScrollLock(scan 0x0046) --> CapsLock  (scan 0x003A)
//
// Entry format (little-endian WORDs packed into a DWORD):
//   high WORD = source scancode, low WORD = target scancode
// ---------------------------------------------------------------------------
struct ScEntry {
    WORD target; // "to"
    WORD source; // "from"
};

// CapsLock -> Ctrl is handled in KeyHook (hook must see physical scan 0x3A).
static const ScEntry k_OurEntries[] = {
    { 0x003A, 0x0046 },  // ScrollLock -> CapsLock
};

// Legacy install used Scancode Map for CapsLock -> LCtrl; remove on apply/uninstall.
static const WORD k_LegacyCapsLockSource = 0x003A;

// ---------------------------------------------------------------------------
// Low-level helpers to parse / build the binary Scancode Map blob
//
// Binary layout:
//   [0x00000000]      DWORD version  = 0
//   [0x00000000]      DWORD flags    = 0
//   [count]           DWORD N        = number of entries INCLUDING terminator
//   [to:WORD,from:WORD] ...          N-1 remapping entries
//   [0x00000000]      DWORD          terminator
// ---------------------------------------------------------------------------

static std::vector<ScEntry> ParseBlob(const std::vector<BYTE>& blob) {
    std::vector<ScEntry> entries;
    if (blob.size() < 16) return entries;

    DWORD count = 0;
    memcpy(&count, blob.data() + 8, sizeof(DWORD));
    if (count < 1) return entries;

    size_t numEntries = count - 1;
    size_t needed = 12 + numEntries * 4;
    if (blob.size() < needed) return entries;

    for (size_t i = 0; i < numEntries; ++i) {
        ScEntry e;
        memcpy(&e, blob.data() + 12 + i * 4, sizeof(ScEntry));
        if (e.target != 0 || e.source != 0)
            entries.push_back(e);
    }
    return entries;
}

static std::vector<BYTE> BuildBlob(const std::vector<ScEntry>& entries) {
    DWORD count = (DWORD)(entries.size() + 1);
    std::vector<BYTE> blob(8 + 4 + entries.size() * 4 + 4, 0);
    memcpy(blob.data() + 8, &count, sizeof(DWORD));
    for (size_t i = 0; i < entries.size(); ++i)
        memcpy(blob.data() + 12 + i * 4, &entries[i], sizeof(ScEntry));
    return blob;
}

static std::vector<BYTE> ReadRegistry() {
    std::vector<BYTE> blob;
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return blob;

    DWORD type = 0, size = 0;
    if (RegQueryValueExW(hKey, REG_VAL, NULL, &type, NULL, &size) == ERROR_SUCCESS
        && type == REG_BINARY && size > 0) {
        blob.resize(size);
        RegQueryValueExW(hKey, REG_VAL, NULL, &type, blob.data(), &size);
    }
    RegCloseKey(hKey);
    return blob;
}

static bool WriteRegistry(const std::vector<BYTE>& blob) {
    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_SET_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    res = RegSetValueExW(hKey, REG_VAL, 0, REG_BINARY, blob.data(), (DWORD)blob.size());
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}

static void DeleteRegistry() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, REG_VAL);
        RegCloseKey(hKey);
    }
}

static bool HasBackupStored() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, BACKUP_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type = 0, size = 0;
    LONG res = RegQueryValueExW(hKey, BACKUP_PRESENT_VAL, NULL, &type, NULL, &size);
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS && type == REG_DWORD;
}

static bool SaveOriginalBackup(const std::vector<BYTE>& original, bool hadOriginal) {
    HKEY hKey = NULL;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, BACKUP_KEY, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &hKey, &disp) != ERROR_SUCCESS)
        return false;

    LONG res = ERROR_SUCCESS;
    if (hadOriginal) {
        res = RegSetValueExW(hKey, BACKUP_VAL, 0, REG_BINARY,
                             original.data(), (DWORD)original.size());
    } else {
        RegDeleteValueW(hKey, BACKUP_VAL);
    }

    DWORD present = hadOriginal ? 1u : 0u;
    if (res == ERROR_SUCCESS) {
        res = RegSetValueExW(hKey, BACKUP_PRESENT_VAL, 0, REG_DWORD,
                               (const BYTE*)&present, sizeof(present));
    }

    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}

static bool LoadOriginalBackup(std::vector<BYTE>* original, bool* hadOriginal) {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, BACKUP_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD present = 0, size = sizeof(present), type = 0;
    if (RegQueryValueExW(hKey, BACKUP_PRESENT_VAL, NULL, &type,
                         (LPBYTE)&present, &size) != ERROR_SUCCESS
        || type != REG_DWORD) {
        RegCloseKey(hKey);
        return false;
    }

    *hadOriginal = (present != 0);
    original->clear();
    if (*hadOriginal) {
        size = 0;
        if (RegQueryValueExW(hKey, BACKUP_VAL, NULL, &type, NULL, &size) != ERROR_SUCCESS
            || type != REG_BINARY || size == 0) {
            RegCloseKey(hKey);
            return false;
        }
        original->resize(size);
        RegQueryValueExW(hKey, BACKUP_VAL, NULL, &type, original->data(), &size);
    }

    RegCloseKey(hKey);
    return true;
}

static void DeleteOriginalBackup() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, BACKUP_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, BACKUP_VAL);
        RegDeleteValueW(hKey, BACKUP_PRESENT_VAL);
        RegCloseKey(hKey);
    }
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, BACKUP_KEY);
}

static void RemoveLegacyCapsLockMap(std::vector<ScEntry>* entries) {
    entries->erase(
        std::remove_if(entries->begin(), entries->end(),
            [&](const ScEntry& e) { return e.source == k_LegacyCapsLockSource; }),
        entries->end());
}

static std::vector<ScEntry> ApplyOurEntries(const std::vector<ScEntry>& base) {
    std::vector<ScEntry> entries = base;
    RemoveLegacyCapsLockMap(&entries);
    for (const auto& ours : k_OurEntries) {
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [&](const ScEntry& e) { return e.source == ours.source; }),
            entries.end());
    }
    for (const auto& e : k_OurEntries)
        entries.push_back(e);
    return entries;
}

static void RemoveOurEntriesLegacy(std::vector<ScEntry>* entries) {
    RemoveLegacyCapsLockMap(entries);
    for (const auto& ours : k_OurEntries) {
        entries->erase(
            std::remove_if(entries->begin(), entries->end(),
                [&](const ScEntry& e) {
                    return e.source == ours.source && e.target == ours.target;
                }),
            entries->end());
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool ScancodeMap::Install() {
    // Backup the current map only on the first -install (before we modify anything).
    if (!HasBackupStored()) {
        auto original = ReadRegistry();
        bool hadOriginal = !original.empty();
        if (!SaveOriginalBackup(original, hadOriginal))
            return false;
    }

    auto entries = ApplyOurEntries(ParseBlob(ReadRegistry()));
    return WriteRegistry(BuildBlob(entries));
}

void ScancodeMap::Uninstall() {
    std::vector<BYTE> original;
    bool hadOriginal = false;

    if (LoadOriginalBackup(&original, &hadOriginal)) {
        if (hadOriginal)
            WriteRegistry(original);
        else
            DeleteRegistry();
        DeleteOriginalBackup();
        return;
    }

    // Fallback for installs that predate backup support.
    auto entries = ParseBlob(ReadRegistry());
    RemoveOurEntriesLegacy(&entries);
    if (entries.empty())
        DeleteRegistry();
    else
        WriteRegistry(BuildBlob(entries));
}

bool ScancodeMap::IsInstalled() {
    auto entries = ParseBlob(ReadRegistry());

    for (const auto& ours : k_OurEntries) {
        bool found = false;
        for (const auto& e : entries)
            if (e.source == ours.source && e.target == ours.target) { found = true; break; }
        if (!found) return false;
    }
    return true;
}
