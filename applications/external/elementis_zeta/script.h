#pragma once
// Script VM (contract section 8): bytecode of a map's script section, run by world_update().
// No furi.h (host-compilable). The VM state lives in World (World.scr); this header has the opcode
// numbers (shared with tools/mapc.py OPS), the state struct and the API.
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

struct World;

// ---- opcodes: 1 byte, then fixed operands (u8 unless noted; u16 are little-endian). `rel` = signed
// 16-bit offset from the END of the instruction. Keep in sync with tools/mapc.py (OPS).
// Flag ids are 10 bits: JF / JNF / SETF / CLRF carry bits 0..7 in the operand byte and bits 8..9 in bits 6..7 of the OPCODE byte (contract 40.3); every other opcode is < 0x40.
enum {
    OP_END = 0x00, //                       end this script (return to the caller after CALL)
    OP_SAY = 0x01, // text16                dialog; yields until it closes
    OP_ASK = 0x02, // text16                dialog + YES/NO; var 0 = 1 yes / 0 no; yields
    OP_JMP = 0x03, // rel16
    OP_JF = 0x04, // flag rel16            jump if flag set
    OP_JNF = 0x05, // flag rel16            jump if flag clear
    OP_JV = 0x06, // var val rel16         jump if var == val
    OP_SETF = 0x07, // flag
    OP_CLRF = 0x08, // flag
    OP_SETV = 0x09, // var val
    OP_ADDV = 0x0A, // var val               var = (var + val) & 255
    OP_WAIT = 0x0B, // ms16
    OP_NPC_WALK = 0x0C, // npc dir steps         blocks until that NPC stops (done or blocked)
    OP_NPC_FACE = 0x0D, // npc dir
    OP_NPC_FACE_HERO = 0x0E, // npc
    OP_NPC_SHOW = 0x0F, // npc                   until the next map load (flags decide after)
    OP_NPC_HIDE = 0x10, // npc
    OP_HERO_WALK = 0x11, // dir steps             blocks; ignores collisions (stays inside the map)
    OP_HERO_FACE = 0x12, // dir
    OP_CAM_NPC = 0x13, // npc                   camera follows the NPC (cut)
    OP_CAM_HERO = 0x14, //
    OP_FADE_OUT = 0x15, //                       fade to white and HOLD it until FADE_IN
    OP_FADE_IN = 0x16, //
    OP_WARP = 0x17, // map i j facing        fade (if not held) + load; ENDS the script
    OP_LOCK = 0x18, //                       player input locked (the default while a script runs)
    OP_UNLOCK = 0x19, //
    OP_CALL_NATIVE = 0x1A, // id a b                game-specific hook, see script_native_id
    OP_NPC_PLACE = 0x1B, // npc i j facing        teleport an NPC
    OP_NPC_WALK_NB = 0x1C, // npc dir steps         starts walking, does not block
    OP_WAIT_IDLE = 0x1D, //                       wait until the hero and every NPC stopped walking
    OP_JNV = 0x1E, // var val rel16         jump if var != val
    OP_HERO_FACE_NPC = 0x1F, // npc
    OP_CALL = 0x20, // script                run another script of this map, then continue
    OP__COUNT
};

// CALL_NATIVE ids (the second operand byte is `id`; a, b are the two u8 arguments)
enum {
    NAT_NONE = 0,
    NAT_CHOOSE_STARTER = 1, // a = var that receives the chosen species (1..3)
    NAT_RIVAL_PICK = 2, // a = var holding the player's species, b = var that receives the rival's
    NAT_GIVE = 3, // a = species, b = level
    NAT_BATTLE = 4, // a = trainer id, b = var that receives the result (1 won, 0 lost)
    NAT_HEAL_PARTY = 5,
    NAT_NAME_SELECT = 6, // a = 0 player name, 1 rival name
    NAT_NAME_VAR = 7, // a = text variable 0..3, b = var holding a species: {va} = its name
    NAT_MON_SHOW = 8, // a = var holding a species: picture popup during the next say / ask
    NAT_MAX =
        24 // table size = the last native id + 1 (ids 1..23: 9..23 are in natives.h / natives_shop.h). The device's table is a const array of NAT_MAX pointers in natives.c
        // (g_natives, no registration code and no RAM copy: C3-A memory trim): adding a native past it is a compile error until NAT_MAX grows
};

// A hook returns NATIVE_DONE (continue now) or NATIVE_YIELD (it pushed a scene: the script resumes
// the next time the world updates, i.e. after that scene popped).
enum {
    NATIVE_DONE = 0,
    NATIVE_YIELD = 1
};
typedef int (*ScriptNative)(struct World* w, uint8_t a, uint8_t b);
// Host harness only (the device's table is the const g_natives of natives.c): installs/replaces the hook of `id` (1..NAT_MAX-1). The table starts with logging stubs.
#ifndef NUCLEO_DEVICE
void script_register_native(uint8_t id, ScriptNative fn);
#endif

// Debug hooks (host harness / firmware logger). Both default to NULL.
extern void (*script_log_fn)(const char* msg);
// SCRIPT_LOG("text"): a native tells the host harness why it did nothing. The device never installs script_log_fn, so there the calls (and their ~13
// strings, ~0.5 KB of resident rodata + code) compile to nothing (M5 memory recovery, like slog() in script.c).
#ifdef NUCLEO_DEVICE
#define SCRIPT_LOG(msg) ((void)0)
#else
#define SCRIPT_LOG(msg)                       \
    do {                                      \
        if(script_log_fn) script_log_fn(msg); \
    } while(0)
#endif
extern void (*script_trace_fn)(const struct World* w, uint8_t script, uint16_t pc, uint8_t op);

enum {
    SW_NONE = 0,
    SW_YIELD,
    SW_TIME,
    SW_HERO,
    SW_NPC,
    SW_ALL
};

typedef struct {
    bool active;
    bool locked; // player input locked
    uint8_t id; // running script
    uint8_t wait; // SW_*
    uint8_t wait_npc; // SW_NPC: the NPC waited on
    uint8_t sp;
    uint16_t pc; // absolute offset into Map.code
    uint32_t wait_until; // SW_TIME: wclock
    struct {
        uint8_t id;
        uint16_t pc;
    } stk[SCRIPT_STACK];
    uint8_t hdir, hsteps; // scripted hero walk in progress (steps left, including the current one)
} ScriptState;

// Starts script `id` of the loaded map if no script runs and its condition holds. Returns true when
// it started (the caller then stops the hero). Triggers call this; it runs from the next update.
bool script_start(struct World* w, uint8_t id);
// Starts the first on_enter script whose condition holds (after a map load).
bool script_start_enter(struct World* w);
// Aborts the running script (map change, error).
void script_stop(struct World* w);
// Executes instructions until the script yields / blocks / ends. Called once per world_update().
// Returns true when anything happened that may need a redraw; *wake = ms until it needs to run
// again (0 = none, i.e. waiting for a scene or for walks, which wake the world themselves).
bool script_run(struct World* w, uint32_t* wake);

static inline bool script_active(const ScriptState* s) {
    return s->active;
}
static inline bool script_input_locked(const ScriptState* s) {
    return s->active && s->locked;
}
