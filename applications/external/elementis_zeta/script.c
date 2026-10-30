// Script VM (contract section 8). Bytecode: map.h ScriptDef table + code blob; opcodes: script.h.
// Furi-free: scenes are reached through scene.h helpers only.
#ifdef NUCLEO_DEVICE
#pragma GCC optimize( \
    "no-forward-propagate,no-cse-follow-jumps") // size flags of this file, measured (contract 44); NOT for a per-pixel loop
#endif
#include "script.h"
#include "world.h"
#include "scene.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void (*script_log_fn)(const char* msg);
void (*script_trace_fn)(const World* w, uint8_t script, uint16_t pc, uint8_t op);

#ifdef NUCLEO_DEVICE
// The device never installs script_log_fn (only the host harness does): slog() compiles to nothing, and with it its ~15 message strings,
// the vsnprintf buffer and the six logging native stubs below (MEM3: -0.9 KB of the resident image). The arguments are still "used".
static inline void slog(const char* fmt, ...) {
    (void)fmt;
}
#else
static void slog(const char* fmt, ...) {
    if(!script_log_fn) return;
    char buf[80];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    script_log_fn(buf);
}
#endif

#ifndef NUCLEO_DEVICE // the host harness runs scripts without natives.c: these logging stubs stand in (the device registers every native 1..6)
// ---------------------------------------------------------------- native hooks (stubs)
// The battle / naming / party tasks replace these with script_register_native(). A stub logs and
// sets plausible variables so scripts can be tested without the game logic.
static int nat_choose_starter(World* w, uint8_t a, uint8_t b) {
    (void)b;
    slog("native CHOOSE_STARTER var%u (stub: 1)", a);
    if(a < SCRIPT_VARS) w->vars[a] = 1;
    return NATIVE_DONE;
}
static int nat_rival_pick(World* w, uint8_t a, uint8_t b) {
    slog("native RIVAL_PICK mine=var%u -> var%u (stub: next species)", a, b);
    if(a < SCRIPT_VARS && b < SCRIPT_VARS) w->vars[b] = (uint8_t)(w->vars[a] % 3 + 1);
    return NATIVE_DONE;
}
static int nat_give(World* w, uint8_t a, uint8_t b) {
    (void)w;
    slog("native GIVE species %u level %u (stub)", a, b);
    return NATIVE_DONE;
}
static int nat_battle(World* w, uint8_t a, uint8_t b) {
    slog("native BATTLE trainer %u -> var%u (stub: won)", a, b);
    if(b < SCRIPT_VARS) w->vars[b] = 1;
    return NATIVE_DONE;
}
static int nat_heal(World* w, uint8_t a, uint8_t b) {
    (void)w;
    (void)a;
    (void)b;
    slog("native HEAL_PARTY (stub)");
    return NATIVE_DONE;
}
static int nat_name_select(World* w, uint8_t a, uint8_t b) {
    (void)w;
    (void)b;
    slog("native NAME_SELECT who=%u (stub)", a);
    return NATIVE_DONE;
}

static ScriptNative g_nat[NAT_MAX] = {
    [NAT_CHOOSE_STARTER] = nat_choose_starter,
    [NAT_RIVAL_PICK] = nat_rival_pick,
    [NAT_GIVE] = nat_give,
    [NAT_BATTLE] = nat_battle,
    [NAT_HEAL_PARTY] = nat_heal,
    [NAT_NAME_SELECT] = nat_name_select,
};

void script_register_native(uint8_t id, ScriptNative fn) {
    if(id > 0 && id < NAT_MAX) g_nat[id] = fn;
}
#else
extern const ScriptNative g_natives
    [NAT_MAX]; // natives.c: every native, const (nothing is registered at run time and there is no RAM copy)
#define g_nat g_natives
#endif

// ---------------------------------------------------------------- start / stop
static bool cond_ok(const World* w, const ScriptDef* d) {
    const uint16_t f = script_cond_flag(d);
    return !f || world_flag_get(w, f) == (d->cond_value != 0);
}

void script_stop(World* w) {
    memset(&w->scr, 0, sizeof(w->scr));
    w->cam_npc = -1;
    w->cam_dy = 0;
}

bool script_start(World* w, uint8_t id) {
    ScriptState* s = &w->scr;
    if(s->active || id >= w->map.n_scripts) return false;
    const ScriptDef* d = &w->map.scripts[id];
    if(!cond_ok(w, d)) return false;
    memset(s, 0, sizeof(*s));
    s->active = true;
    s->locked = true;
    s->id = id;
    s->pc = script_off(d);
    slog("script %u start", id);
    return true;
}

bool script_start_enter(World* w) {
    for(uint8_t k = 0; k < w->map.n_scripts; k++)
        if(w->map.scripts[k].trigger == SCRT_ENTER && script_start(w, k)) return true;
    return false;
}

// ---------------------------------------------------------------- VM
// Total length (opcode + operands) of every instruction.
static const uint8_t OP_LEN[OP__COUNT] = {
    [OP_END] = 1,
    [OP_SAY] = 3,
    [OP_ASK] = 3,
    [OP_JMP] = 3,
    [OP_JF] = 4,
    [OP_JNF] = 4,
    [OP_JV] = 5,
    [OP_SETF] = 2,
    [OP_CLRF] = 2,
    [OP_SETV] = 3,
    [OP_ADDV] = 3,
    [OP_WAIT] = 3,
    [OP_NPC_WALK] = 4,
    [OP_NPC_FACE] = 3,
    [OP_NPC_FACE_HERO] = 2,
    [OP_NPC_SHOW] = 2,
    [OP_NPC_HIDE] = 2,
    [OP_HERO_WALK] = 3,
    [OP_HERO_FACE] = 2,
    [OP_CAM_NPC] = 2,
    [OP_CAM_HERO] = 1,
    [OP_FADE_OUT] = 1,
    [OP_FADE_IN] = 1,
    [OP_WARP] = 5,
    [OP_LOCK] = 1,
    [OP_UNLOCK] = 1,
    [OP_CALL_NATIVE] = 4,
    [OP_NPC_PLACE] = 5,
    [OP_NPC_WALK_NB] = 4,
    [OP_WAIT_IDLE] = 1,
    [OP_JNV] = 5,
    [OP_HERO_FACE_NPC] = 2,
    [OP_CALL] = 2,
};

static void abort_script(World* w, const char* why) {
    slog("script %u ABORT at %u: %s", w->scr.id, w->scr.pc, why);
    script_stop(w);
}

// Ends the running script (or returns from a CALL). Returns true to keep executing.
static __attribute__((always_inline)) inline bool op_end(World* w, bool auto_fade) {
    ScriptState* s = &w->scr;
    if(s->sp > 0) {
        s->sp--;
        s->id = s->stk[s->sp].id;
        s->pc = s->stk[s->sp].pc;
        return true;
    }
    slog("script %u end", s->id);
    script_stop(w);
    if(auto_fade && w->fade_hold && w->sc) scene_fade_in(w->sc); // never leave the screen white
    return false;
}

static bool wait_done(World* w, uint32_t* wake) {
    ScriptState* s = &w->scr;
    switch(s->wait) {
    case SW_NONE:
        return true;
    case SW_YIELD:
        break; // a scene we pushed has closed (the world only updates when it is on top)
    case SW_TIME:
        if((int32_t)(w->wclock - s->wait_until) < 0) {
            // at most WCLOCK_MAX_DT per wake: the world clock advances by min(dt, WCLOCK_MAX_DT) per update, so one long sleep would
            // only count 100 ms of it (a `wait 600` took ~2 s, a `wait 1600` ~14 s)
            uint32_t rem = s->wait_until - w->wclock;
            *wake = rem < WCLOCK_MAX_DT ? rem : WCLOCK_MAX_DT;
            return false;
        }
        break;
    case SW_HERO:
        if(world_hero_busy(w)) return false;
        break;
    case SW_NPC:
        if(world_npc_busy(w, s->wait_npc)) return false;
        break;
    case SW_ALL:
        if(world_hero_busy(w) || world_npcs_busy(w)) return false;
        break;
    }
    s->wait = SW_NONE;
    return true;
}

static inline uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

// Executes one instruction. Returns true to continue with the next one, false to stop for this update
// (yield, blocked on a scene, script ended / aborted).
static bool step(World* w) {
    ScriptState* s = &w->scr;
    const Map* m = &w->map;
    const ScriptDef* d = &m->scripts[s->id];
    const uint16_t base = script_off(d), end = (uint16_t)(base + script_len(d));
    if(s->pc >= end) return op_end(w, true); // running off the end = END
    const uint8_t* p = m->code + s->pc;
    const uint8_t op =
        p[0] &
        0x3F; // bits 6..7 of the opcode byte = flag id bits 8..9 for jf / jnf / setf / clrf (contract 40.3; mapc writes them for no other op)
    if(op >= OP__COUNT || !OP_LEN[op]) {
        abort_script(w, "bad opcode");
        return false;
    }
    const uint8_t len = OP_LEN[op];
    if(s->pc + len > end) {
        abort_script(w, "truncated instruction");
        return false;
    }
    if(script_trace_fn) script_trace_fn(w, s->id, s->pc, op);
    const uint16_t next = (uint16_t)(s->pc + len);
    s->pc = next;

    // operand sanity helpers
#define FID()                  \
    ((uint16_t)(p[1] |         \
                ((p[0] & 0xC0) \
                 << 2))) /* the flag id of jf / jnf / setf / clrf (p[1] only exists for them) */
#define NPC_OK(n) ((n) < m->n_npcs && (n) < WORLD_MAX_NPCS)
#define VAR_OK(v) ((v) < SCRIPT_VARS)
#define JUMP(rel_at)                                      \
    do {                                                  \
        int t_ = (int)next + (int16_t)rd16(p + (rel_at)); \
        if(t_ < base || t_ > end) {                       \
            abort_script(w, "jump out of the script");    \
            return false;                                 \
        }                                                 \
        s->pc = (uint16_t)t_;                             \
    } while(0)

    switch(op) {
    case OP_END:
        return op_end(w, true);
    case OP_SAY:
    case OP_ASK:
        if(w->sc) {
            world_stop(w);
            bool ok = op == OP_SAY ? scene_dialog_open(w->sc, rd16(p + 1), -1) :
                                     scene_ask_open(w->sc, rd16(p + 1), &w->vars[0]);
            if(ok) {
                s->wait = SW_YIELD; // resumes when the dialog (and the choice) closed
                return false;
            }
        }
        if(op == OP_ASK) w->vars[0] = 0;
        slog("%s not shown (text %04x)", op == OP_SAY ? "SAY" : "ASK", rd16(p + 1));
        return true;
    case OP_JMP:
        JUMP(1);
        return true;
    case OP_JF:
        if(world_flag_get(w, FID())) JUMP(2);
        return true;
    case OP_JNF:
        if(!world_flag_get(w, FID())) JUMP(2);
        return true;
    case OP_JV:
    case OP_JNV:
        if(!VAR_OK(p[1])) break;
        if((w->vars[p[1]] == p[2]) == (op == OP_JV)) JUMP(3);
        return true;
    case OP_SETF:
        world_flag_set(w, FID(), true);
        return true;
    case OP_CLRF:
        world_flag_set(w, FID(), false);
        return true;
    case OP_SETV:
        if(!VAR_OK(p[1])) break;
        w->vars[p[1]] = p[2];
        return true;
    case OP_ADDV:
        if(!VAR_OK(p[1])) break;
        w->vars[p[1]] = (uint8_t)(w->vars[p[1]] + p[2]);
        return true;
    case OP_WAIT: {
        uint16_t ms = rd16(p + 1);
        if(ms) {
            s->wait = SW_TIME;
            s->wait_until = w->wclock + ms;
        }
        return true;
    }
    case OP_NPC_WALK:
    case OP_NPC_WALK_NB:
        if(!NPC_OK(p[1])) break;
        if(world_npc_walk(w, p[1], p[2], p[3]) && op == OP_NPC_WALK) {
            s->wait = SW_NPC;
            s->wait_npc = p[1];
        }
        return true;
    case OP_NPC_FACE:
        if(!NPC_OK(p[1])) break;
        world_npc_face(w, p[1], p[2]);
        return true;
    case OP_NPC_FACE_HERO:
        if(!NPC_OK(p[1])) break;
        world_npc_face_hero(w, p[1]);
        return true;
    case OP_NPC_SHOW:
    case OP_NPC_HIDE:
        if(!NPC_OK(p[1])) break;
        world_npc_show(w, p[1], op == OP_NPC_SHOW);
        return true;
    case OP_HERO_WALK:
        if(world_hero_walk(w, p[1], p[2])) s->wait = SW_HERO;
        return true;
    case OP_HERO_FACE:
        w->hero.facing = p[1] & 7;
        w->hero.frame = 0;
        return true;
    case OP_HERO_FACE_NPC:
        if(!NPC_OK(p[1])) break;
        world_hero_face_npc(w, p[1]);
        return true;
    case OP_CAM_NPC:
        if(!NPC_OK(p[1])) break;
        w->cam_npc = (int8_t)p[1];
        return true;
    case OP_CAM_HERO:
        w->cam_npc = -1;
        w->cam_dy = 0;
        return true;
    case OP_FADE_OUT:
        if(!w->fade_hold && w->sc && scene_fade_out(w->sc)) {
            s->wait = SW_YIELD;
            return false;
        }
        return true;
    case OP_FADE_IN:
        if(w->fade_hold && w->sc && scene_fade_in(w->sc)) {
            s->wait = SW_YIELD;
            return false;
        }
        return true;
    case OP_WARP:
        if(!w->sc || !scene_fade_warp(w->sc, p[1], p[2], p[3], p[4])) {
            abort_script(w, "warp failed");
            return false;
        }
        s->sp = 0;
        return op_end(w, false); // the map changes under the script: it ends here
    case OP_LOCK:
        s->locked = true;
        return true;
    case OP_UNLOCK:
        s->locked = false;
        return true;
    case OP_CALL_NATIVE: {
        ScriptNative fn = p[1] < NAT_MAX ? g_nat[p[1]] : NULL;
        if(!fn) {
            slog("native %u not implemented", p[1]);
            return true;
        }
        if(fn(w, p[2], p[3]) == NATIVE_YIELD) {
            s->wait = SW_YIELD;
            return false;
        }
        return true;
    }
    case OP_NPC_PLACE:
        if(!NPC_OK(p[1])) break;
        world_npc_place(w, p[1], p[2], p[3], p[4]);
        return true;
    case OP_WAIT_IDLE:
        s->wait = SW_ALL;
        return true;
    case OP_CALL:
        if(p[1] >= m->n_scripts || s->sp >= SCRIPT_STACK) break;
        s->stk[s->sp].id = s->id;
        s->stk[s->sp].pc = next;
        s->sp++;
        s->id = p[1];
        s->pc = script_off(&m->scripts[p[1]]);
        return true;
    }
    abort_script(w, "bad operand");
    return false;
#undef NPC_OK
#undef VAR_OK
#undef JUMP
}

bool script_run(World* w, uint32_t* wake) {
    *wake = 0;
    bool did = false;
    for(int n = 0; n < SCRIPT_BUDGET; n++) {
        if(!w->scr.active) return true;
        if(!wait_done(w, wake)) return did;
        did = true;
        if(!step(w)) return true;
    }
    abort_script(w, "instruction budget exceeded (endless loop?)");
    return true;
}
