F1COPY Ver 0.91 - Readme

■ Download
Get f1copy.exe from GitHub Releases (see Assets):
https://github.com/yukioshibata/f1copy/releases

■ Overview
F1COPY is a utility that streamlines Windows keyboard use.
It provides copy/paste via F1/F2 keys, maps Caps Lock to Ctrl, and more.

■ Main features
・F1 key         -> Acts as Ctrl+C (copy)
・F2 key         -> Acts as Ctrl+V (paste)
・Caps Lock key  -> Acts as Ctrl key
・Scroll Lock key -> Acts as Caps Lock key
・Caps Lock + F1 -> Acts as the original F1 key
・Caps Lock + F2 -> Acts as the original F2 key

■ Usage
1. Normal startup
   Run f1copy.exe to keep the app in the system tray.
   On startup, the current settings are shown at the center of the screen
   for about 2 seconds.

2. Exit
   Double-click the tray icon to exit.
   A short exit message is shown at the center of the screen for about
   2 seconds before the app closes.

3. Install (auto-start at logon)
   Run the following from Command Prompt or similar:
   f1copy.exe -install
   * Registers a Task Scheduler entry so the app runs when you sign in.
   * If not run as administrator, it restarts with administrator rights.
   * After install, the app starts in the tray. Reboot the PC so keyboard
     remapping (Caps Lock / Scroll Lock) takes effect.

4. Uninstall
   Run the following from Command Prompt or similar:
   f1copy.exe -uninstall
   * Removes the Task Scheduler entry and stops the running instance.

■ System requirements
・Windows 11 64-bit
