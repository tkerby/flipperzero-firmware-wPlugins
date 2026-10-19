// Forced-included (`-include short_calls.h`, application.fam: SHORT_CALLS / SHORT_CALLS_PLUGIN) into every translation unit of the app and of
// the menu and UI plugins on the device; the host builds never see it. MEM3 / "calls": ufbt compiles a FAP with -mlong-calls, so EVERY call is
// `ldr rX, =f; blx rX` + a literal-pool word + a load-time relocation, even a call between two of our own functions. Only the firmware (flash,
// ~400 MB away from the heap block the module is loaded into) needs that. A call to one of OUR functions (or, for a plugin, to the app's API
// table: app and plugin are both in RAM, a few hundred KB apart) can be a plain `bl`: 4 bytes, no literal word.
//
// How: this header first declares everything the firmware (or newlib / libgcc, which the firmware also provides) offers, which keeps the
// command line's -mlong-calls for those declarations, and then switches the compiler to short calls with the ARM pragma no_long_calls, which
// marks every function declared or defined AFTER it (all of ours, in our headers and .c files) short_call. Compiler-generated library calls
// (__aeabi_*, memcpy / memset expansions) have no declaration and stay long calls.
//
// THE RULE: a source file compiled with this header must not include an SDK / libc header that is not listed below (it would be declared after
// the pragma and its functions would become short calls to the firmware: a 10-byte heap trampoline per function at load time, or a failed
// load). Add the header HERE, above the pragma (not <flipper_application/...> or <loader/firmware_api/...>: they pull in <elf.h>, whose
// EV_NONE macro breaks battle_core.h; plugin_loader.c brackets them with long_calls pragmas instead).
// `python3 tools/check_short_calls.py` (after `ufbt build`) fails if a linked module has a direct branch to a symbol it does not define
// (other than the app's API table, for a plugin).
#ifndef __cplusplus
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stm32wbxx.h>
#pragma no_long_calls
#endif
