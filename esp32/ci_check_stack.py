#!/usr/bin/env python3
# ci_check_stack.py — loop-task stack budget check. CI runs it after each build.
#
# Everything the firmware does at run time happens on the Arduino loop task:
# CAN dispatch, the HTTP server, the WebSocket server. v2.16-beta.37 and .38
# overflowed its stack whenever a browser opened the dashboard: loop() ->
# web_dashboard_update() -> WebSockets -> ws_event() -> build_json() ->
# snprintf("%f") needed about 8.8 KB of an 8 KB stack, and nothing noticed
# before release. This adds up the frames along the deepest known call chains
# and fails when one gets too close to the configured stack size.
#
#   pio run -e m5stack-atom
#   python3 ci_check_stack.py .pio/build/m5stack-atom
#
# Inputs:
#   <build_dir>/src/*.su   per-function frame sizes, written by GCC beside each
#                          object (-fstack-usage, build_src_flags in
#                          platformio.ini)
#   .firmware/main.cpp     SET_LOOP_TASK_STACK_SIZE(...); without it the
#                          Arduino default applies
#
# A chain is the sum of its functions' frames plus fixed allowances for the
# library and libc code in between, which has no .su here. A function the table
# names but the .su files do not have fails the check: it was renamed, removed
# or inlined into its caller, and the table has to follow. --allow-missing
# skips such a chain instead; that is for running against an old tag and CI
# never passes it.
import argparse
import os
import re
import sys

# CONFIG_ARDUINO_LOOP_STACK_SIZE in the pinned Arduino core (ESP32 and S3).
_DEFAULT_STACK = 8192

# Share of the stack a chain may use. The rest has to hold what the sums leave
# out: the coprocessor save area FreeRTOS keeps at the top of the stack (100
# bytes on the ESP32, 320 on the S3), loopTask(), one interrupt frame (192
# bytes) and libc's bigint helpers below _dtoa_r (about 0.1 KB, 0.5 KB when one
# has to malloc). On the 8192-byte stack the old releases had, beta.36 sums to
# 92% and ran in the field; beta.37 and beta.38 sum to 107% and rebooted.
_FAIL_FRACTION = 0.93
_WARN_FRACTION = 0.80

# Bytes for code on a chain that has no .su under <build_dir>/src. Measured on
# espressif32@6.9.0 with WebSockets 2.7.3; the library and libc frames are the
# same on the ESP32 and ESP32-S3 envs. Re-measure after bumping either pin:
# library frames from a build with PLATFORMIO_BUILD_FLAGS=-fstack-usage (.su
# files appear under <build_dir>/lib*/), libc and ESP-IDF frames from
# `xtensa-esp32-elf-objdump -d firmware.elf`, the `entry a1, N` at each symbol.
_ALLOWANCES = {
    # snprintf 192 + _svfprintf_r 784 + _dtoa_r 160, from the ELF.
    "printf %f": 1136,
    # WebSocketsServer::loop 32 + WebSocketsServerCore::loop 32 +
    # handleClientData 64 + handleHeader 128 + runCbEvent 48 + the
    # std::function invoker 32, from the library's .su files.
    "WebSockets connect": 336,
    # WebSocketsServer::loop 32 + WebSocketsServerCore::loop 32 +
    # handleClientData 64 + handleWebsocket 32 + handleWebsocketCb 64; the
    # frame header is then read in up to three parts (a masked frame with a
    # 16-bit length), each nesting handleWebsocketWaitFor 64 + readCb 32 +
    # std::function call 48 + 32 + handleWebsocketCb 64 = 240; then readCb 32 +
    # std::function call 48 + 32 + handleWebsocketPayloadCb 48 +
    # messageReceived 32 + runCbEvent 48 + invoker 32.
    "WebSockets message": 1216,
    # WebServer::handleClient 80 + _handleRequest 112 +
    # FunctionRequestHandler::handle 64 + std::function call 32 + 32, from the
    # framework library's .su files.
    "WebServer": 320,
    # Estimate. Preferences -> NVS -> SPI flash is precompiled ESP-IDF, so this
    # is the deepest path the m5stack-atom ELF's call graph shows below
    # prefs_save(): Preferences::begin() -> nvs_flash_init_partition ->
    # nvs::Storage::init -> ... -> esp_flash_read and its error log, 2256
    # bytes, with the virtual calls in nvs:: and the flash driver followed by
    # hand and the abort and unwind paths left out. Preferences::put*() comes
    # to 2128 the same way; a write that hits no error path is nearer 1.4 KB.
    "NVS write": 2304,
}

# (name, functions with a frame in the .su files, allowances). prefs_save() is
# the deepest thing web_command() and process_frame() call.
_CHAINS = [
    ("dashboard connect",
     ["loop", "web_dashboard_update", "ws_event", "build_json"],
     ["WebSockets connect", "printf %f"]),
    ("dashboard command",
     ["loop", "web_dashboard_update", "ws_event", "web_command", "prefs_save"],
     ["WebSockets message", "NVS write"]),
    ("HTTP status",
     ["loop", "web_dashboard_update", "handle_status", "build_json"],
     ["WebServer", "printf %f"]),
    ("HTTP command",
     ["loop", "web_dashboard_update", "handle_cmd", "web_command", "prefs_save"],
     ["WebServer", "NVS write"]),
    ("CAN frame",
     ["loop", "drain_can_bus_index", "process_frame", "prefs_save"],
     ["NVS write"]),
]


def _say(text):
    print("[stack] %s" % text)


def _annotate(kind, text):
    # GitHub Actions turns these lines into annotations on the run.
    if os.environ.get("GITHUB_ACTIONS") == "true":
        for raw, escaped in (("%", "%25"), ("\r", "%0D"), ("\n", "%0A")):
            text = text.replace(raw, escaped)
        print("::%s title=Loop-task stack budget::%s" % (kind, text))


def loop_stack_size(main_cpp):
    """Bytes of loop-task stack main.cpp asks for, or the Arduino default."""
    with open(main_cpp, encoding="utf-8", errors="replace") as f:
        text = f.read()
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    sizes = []
    for rest in re.findall(r"\bSET_LOOP_TASK_STACK_SIZE\b([^\n]*)", text):
        # "(16 * 1024);" -> 16384. Anything else is an error, not the default.
        m = re.fullmatch(r"\s*\((.*)\)\s*;?\s*", rest)
        size = 1
        for factor in m.group(1).split("*") if m else [""]:
            n = re.fullmatch(r"\s*(0[xX][0-9a-fA-F]+|[1-9][0-9]*)[uUlL]*\s*", factor)
            if not n:
                raise ValueError("cannot evaluate SET_LOOP_TASK_STACK_SIZE%s in %s"
                                 % (rest.rstrip(), main_cpp))
            size *= int(n.group(1), 0)
        sizes.append(size)
    # More than one (an #if per board): hold every env to the smallest.
    return min(sizes) if sizes else _DEFAULT_STACK


def _bare_name(signature):
    # "void process_frame(CanBusId, const CanFrame&)" -> "process_frame".
    # C objects carry the bare name already.
    head = signature.split("(", 1)[0].split()
    return head[-1].lstrip("*&") if head else ""


def read_frames(build_dir, src_dir):
    """({function: [(bytes, qualifier), ...]}, files read) from <build_dir>/src."""
    su_root = os.path.join(build_dir, "src")
    frames = {}
    files = 0
    for root, _dirs, names in os.walk(su_root):
        for name in sorted(names):
            if not name.endswith(".su"):
                continue
            rel = os.path.relpath(os.path.join(root, name), su_root)
            # A cached build dir keeps the .su of a source file that has since
            # been deleted or renamed. Its frames are not in this firmware.
            if not os.path.isfile(os.path.join(src_dir, rel[:-len(".su")])):
                _say("ignoring %s: its source file is gone" % rel)
                continue
            files += 1
            with open(os.path.join(root, name), encoding="utf-8", errors="replace") as f:
                for line in f:
                    # file:line:col:signature<TAB>bytes<TAB>static|dynamic|dynamic,bounded
                    fields = line.rstrip("\n").split("\t")
                    where = fields[0].split(":", 3)
                    if len(fields) != 3 or len(where) != 4 or not fields[1].isdigit():
                        continue
                    frames.setdefault(_bare_name(where[3]), []).append(
                        (int(fields[1]), fields[2]))
    return frames, files


def check(build_dir, src_dir, allow_missing):
    """Print the budget table; return the list of failures (empty = pass)."""
    main_cpp = os.path.join(src_dir, "main.cpp")
    try:
        stack = loop_stack_size(main_cpp)
    except OSError as e:
        return ["cannot read %s: %s" % (main_cpp, e.strerror)]
    except ValueError as e:
        return [str(e)]

    frames, files = read_frames(build_dir, src_dir)
    su_glob = os.path.join(build_dir, "src", "*.su")
    if not files:
        return ["no .su files in %s. Build the env first; -fstack-usage comes "
                "from build_src_flags in platformio.ini, and objects built "
                "without it need a `pio run -t clean`" % su_glob]
    if not frames:
        return ["no frame could be parsed from the %d .su file(s) in %s"
                % (files, su_glob)]

    errors = []
    sizes = {}
    for _name, functions, _allowances in _CHAINS:
        for fn in functions:
            if fn in sizes or fn not in frames:
                continue
            # GCC lists a clone (.part, .constprop) under the function's own
            # name, and a .part nests inside it: count them all.
            sizes[fn] = sum(size for size, _qualifier in frames[fn])
            if len(frames[fn]) > 1:
                _say("note: %s() has %d frames in the .su files, summed"
                     % (fn, len(frames[fn])))
            for _size, qualifier in frames[fn]:
                if qualifier not in ("static", "dynamic,bounded"):
                    errors.append(
                        "%s() has a %s frame (alloca or a variable-length "
                        "array): the %d bytes in its .su are not an upper bound"
                        % (fn, qualifier, sizes[fn]))

    limit = int(stack * _FAIL_FRACTION)
    warn = int(stack * _WARN_FRACTION)
    _say("loop task stack %d bytes, chain limit %d (%d%%), warning from %d (%d%%)"
         % (stack, limit, round(_FAIL_FRACTION * 100), warn, round(_WARN_FRACTION * 100)))
    _say("frames: %s" % ", ".join("%s %d" % item for item in sizes.items()))
    # Shown, not checked: a big frame here that runs below a chain function
    # belongs in _CHAINS.
    others = sorted(((sum(size for size, _qualifier in hits), fn)
                     for fn, hits in frames.items() if fn and fn not in sizes),
                    reverse=True)[:5]
    _say("largest frames on no chain: %s"
         % ", ".join("%s %d" % (fn, size) for size, fn in others))
    _say("%-18s %6s  %8s" % ("chain", "bytes", "of stack"))

    warnings = []
    evaluated = 0
    for name, functions, allowances in _CHAINS:
        missing = ["%s()" % fn for fn in functions if fn not in sizes]
        if missing:
            text = "no stack frame for %s in %s" % (", ".join(missing), su_glob)
            if allow_missing:
                _say("%-18s %6s  %8s  skipped: %s" % (name, "-", "-", text))
            else:
                _say("%-18s %6s  %8s" % (name, "?", "?"))
                errors.append(
                    "%s: %s. Renamed, removed or inlined into its caller: "
                    "update _CHAINS in %s" % (name, text, os.path.basename(__file__)))
            continue
        parts = [(fn, sizes[fn]) for fn in functions]
        parts += [(allowance, _ALLOWANCES[allowance]) for allowance in allowances]
        total = sum(size for _label, size in parts)
        evaluated += 1
        _say("%-18s %6d  %7.1f%%" % (name, total, 100.0 * total / stack))
        text = ("%s needs %d bytes, %.1f%% of the %d-byte loop task stack "
                "(limit %d, %d%%): %s"
                % (name, total, 100.0 * total / stack, stack, limit,
                   round(_FAIL_FRACTION * 100),
                   " + ".join("%s %d" % part for part in parts)))
        if total > limit:
            errors.append(text + ". Shrink a frame on that chain or raise "
                                 "SET_LOOP_TASK_STACK_SIZE in main.cpp")
        elif total > warn:
            warnings.append(text)
    if not evaluated:
        errors.append("no chain could be evaluated")

    for text in warnings:
        _say("WARNING: %s" % text)
        _annotate("warning", text)
    return errors


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(
        description="Fail when a loop-task call chain gets too close to the "
                    "loop task's stack size.")
    ap.add_argument("build_dir", help="PlatformIO build dir, .pio/build/<env>")
    ap.add_argument("--src-dir", default=os.path.join(here, ".firmware"),
                    help="firmware sources (default: .firmware beside this script)")
    ap.add_argument("--allow-missing", action="store_true",
                    help="skip a chain whose functions this tree does not have "
                         "(old tags only)")
    args = ap.parse_args()

    errors = check(args.build_dir, args.src_dir, args.allow_missing)
    for text in errors:
        _say("FAIL: %s" % text)
        _annotate("error", text)
    if errors:
        return 1
    _say("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
