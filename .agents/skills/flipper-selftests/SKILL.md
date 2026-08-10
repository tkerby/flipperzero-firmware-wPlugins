---
name: flipper-selftests
description: Build, deploy, and verify the opt-in WM-Bus on-device selftest suite, then restore the production FAP. Use when running or checking Flipper selftests, protocol fixtures, Mode 5 crypto tests, capture tests, or parser tests.
---

From the repository root, build and launch the test variant:

```bash
WMBUS_SELFTESTS=1 ufbt launch
```

Record the `/dev/ttyACM*` port reported by `ufbt launch`. Wait at least two seconds for the suite to finish.

Fetch the report with the uFBT storage tool, replacing `<port>`:

```bash
FBT_TOOLCHAIN_PATH="${HOME}/.ufbt" source "${HOME}/.ufbt/current/scripts/toolchain/fbtenv.sh"
python3 "${HOME}/.ufbt/current/scripts/storage.py" \
    -p /dev/<port> \
    receive \
    /ext/apps_data/wmbus_decoder/selftest.txt \
    /tmp/wmbus_selftest.txt
```

If the device reports readiness but returns no data, wait briefly and retry the fetch once. Treat any other serial or storage error as a failure to obtain results.

Inspect the complete report. Require:

- no line beginning with `FAIL`
- the summary has `failed=0`
- `total` equals `passed`; do not hardcode the test count because the suite may grow

The summary format is:

```text
selftests done total=<N> passed=<N> failed=0
```

For protocol-sensitive changes, confirm the report includes the relevant Mode 5, configured-key ordering, wrong-key rejection, CRC, T/C capture, FIFO, and parser-ordering cases.

Always restore the production variant after testing:

```bash
ufbt
```

If the test FAP was installed on a device, replace it with production and launch it:

```bash
ufbt launch
```

Confirm the final build uses `WMBUS_SELFTESTS=0` and contains no selftest sources or symbols. The production artifact is:

```text
dist/wmbus_decoder.fap
```

## Sandbox / Device Access

Flipper serial devices and the uFBT build cache may require elevated access. Request access when needed; do not work around device permissions.
