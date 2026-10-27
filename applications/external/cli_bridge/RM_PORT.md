# CLI-GUI Bridge on current RM

This port uses ranchordo/flipperzero-cli-bridge main at 262f2d59255efaa53a8c822061737ff15150d01f. The source manifest already uses the valid cli_gui ID. Its implicit SDK version 0.1 is made explicit for RM metadata conventions; it is not an upstream release claim.

Build/update the firmware together with the FAP. The firmware owns the pipe-based shell session and exposes it through a versioned local record. The GUI reports that a firmware update is needed when that support is absent. Native exported API remains 88.23; no new ELF API symbol is required.

The upstream controls, command lowercasing, scrolling, short Back/Ctrl-C and long Back/close are retained. Long OK on the output view detaches the GUI while leaving its firmware-owned command/session alive. Relaunch reattaches and clears old output. Output is drained while detached so a full GUI buffer cannot stall a background command. The firmware owns all persistent shell/pipe/timer state and code; it never retains GUI FAP callbacks or context. Timer creation runs in the resident timer service, keeping RM's retained timer-name pointer valid after the GUI thread is freed.

A private registry copies current registered commands and loads the standard external CLI plugins. Per-shell built-ins have their own context. The GUI-only shell mode allows commands alongside this GUI, replacing the obsolete global flag manipulation; USB CLI behavior and the global command registry are preserved. Commands that ignore interruption can still delay normal close, as with native CLI disconnection.

The original upstream README remains intact as provenance. Its legacy USB-disruption warning describes the old hijack implementation. This port uses its own shell and does not switch the USB session. Device/RF/UI testing is still required; compilation and host lifecycle tests do not establish hardware behavior.
