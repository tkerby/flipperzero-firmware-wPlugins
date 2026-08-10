---
name: deploy-to-flipper
description: Build, install, and launch the production WM-Bus FAP on a connected Flipper. Use for production deployment or restoring the device after an opt-in selftest build.
---

Build and deploy the selftest-free production app from the repository root:

```bash
ufbt launch
```

Require strict `APPCHK` success and record the port reported by the launcher:

```
        APPCHK  ~/.ufbt/build/wmbus_decoder.fap
                Target: 7, API: 87.1
2026-07-04 21:07:32,311 [INFO] Using flip_<name> on /dev/<port>
```

Do not set `WMBUS_SELFTESTS=1` for production deployment. Use the `flipper-selftests` skill when tests are requested, then run `ufbt launch` without that environment variable so production is installed last.

## Sandbox / Device Access

Flipper serial devices and the uFBT build cache may require elevated access. Request access when needed; do not work around device permissions.
