#ifndef SCANCODEMAP_H
#define SCANCODEMAP_H

class ScancodeMap {
public:
    // Apply our remappings on -install only.
    // Backs up the current HKLM Scancode Map once, then writes our entries.
    // Requires administrator privileges.
    // Returns true on success.  A system reboot is required for changes to take effect.
    static bool Install();

    // Restore the backed-up Scancode Map on -uninstall.
    // Requires administrator privileges.
    static void Uninstall();

    // Returns true if our remappings are currently present in the registry.
    static bool IsInstalled();
};

#endif
