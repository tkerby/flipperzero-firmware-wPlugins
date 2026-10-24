---
name: build-fap
description: Build the production WM-Bus application into a selftest-free Flipper FAP. Use for normal builds, release artifacts, or restoring production after on-device selftests.
---

Build production from the repository root:

```bash
ufbt
```

Expected artifact:

```text
dist/wmbus_decoder.fap
```

Normal builds define `WMBUS_SELFTESTS=0` and exclude `src/test`. Require strict `APPCHK` success.

Do not set `WMBUS_SELFTESTS=1` for a production artifact. Use the `flipper-selftests` skill for the opt-in test build and rebuild production afterward.
