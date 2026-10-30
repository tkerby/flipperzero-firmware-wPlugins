#include "shadow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pb_const.h"
#include "pb_wram.h"

#define RNG_TRACE_MAX 4096

/* ---- WRAM symbol names for reports ---- */

typedef struct {
    uint16_t addr;
    const char* name;
} WramSym;

static WramSym* wsyms;
static unsigned nwsyms;

static int wsym_rank(const char* n) {
    static const char* const pref[] = {
        "wBattle", "wPlayer", "wEnemy", "wParty", "wOT", "wCur", "wLink", "wLast"};
    for(unsigned i = 0; i < sizeof(pref) / sizeof(pref[0]); i++) {
        if(!strncmp(n, pref[i], strlen(pref[i]))) return (int)i;
    }
    return 99;
}

static int cmp_wsym(const void* a, const void* b) {
    const WramSym *x = a, *y = b;
    if(x->addr != y->addr) return x->addr < y->addr ? -1 : 1;
    return wsym_rank(x->name) - wsym_rank(y->name);
}

static void wsyms_load(const SymTable* t) {
    if(wsyms) return;
    wsyms = calloc(t->count, sizeof(WramSym));
    for(unsigned i = 0; i < t->count; i++) {
        const Sym* s = &t->syms[i];
        bool w0 = s->bank == 0 && s->addr >= 0xC000 && s->addr < 0xD000;
        bool w1 = s->bank == 1 && s->addr >= 0xD000 && s->addr < 0xE000;
        if((w0 || w1) && !strchr(s->name, '.')) wsyms[nwsyms++] = (WramSym){s->addr, s->name};
    }
    qsort(wsyms, nwsyms, sizeof(WramSym), cmp_wsym);
}

static void wram_name(uint16_t addr, char* out, size_t size) {
    int lo = 0, hi = (int)nwsyms - 1, best = -1;
    while(lo <= hi) {
        int mid = (lo + hi) / 2;
        if(wsyms[mid].addr <= addr) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    if(best < 0) {
        snprintf(out, size, "$%04X", addr);
        return;
    }
    while(best > 0 && wsyms[best - 1].addr == wsyms[best].addr)
        best--; /* best-ranked alias */
    if(wsyms[best].addr == addr)
        snprintf(out, size, "%s", wsyms[best].name);
    else
        snprintf(out, size, "%s+%u", wsyms[best].name, addr - wsyms[best].addr);
}

/* ---- what is compared ---- */

typedef struct {
    uint16_t start, end; /* [start, end) */
} Range;

static const Range compare_ranges[] = {
    {wBattle, wBattleEnd},
    {wBattleMenuCursorPosition, wCurMoveNum + 1},
    {wBattlePlayerAction, wBattlePlayerAction + 1},
    {wBattleResult, wBattleResult + 1},
    {wCurPartySpecies, wCurPartyMon + 1},
    {wLinkBattleRNs, wEnemyMonEnd},
    {wBattleMode, wForcedSwitch + 1},
    {wCurDamage, wCurDamage + 2},
    {wBattleHasJustStarted, wBattleHasJustStarted + 1},
    {wOTPartyCount, wOTPartyMonNicknames + PARTY_LENGTH* MON_NAME_LENGTH},
    {wPartyCount, wPartyMonNicknames + PARTY_LENGTH* MON_NAME_LENGTH},
};

/* bytes the engine does not model, with the reason */
static bool excluded(uint16_t a) {
    /* scratch species of menus, cries and pictures; battle logic never reads it */
    if(a == wCurPartySpecies) return true;
    /* trainer attributes from GetTrainerAttributes/ReadTrainerParty (items,
     * prize money, AI flags) and class names: link battles never read them */
    if(a >= wEnemyTrainerItem1 && a < wCurOTMon) return true;
    if(a == wOtherTrainerClass || a == wTrainerClass || a == wOtherTrainerID) return true;
    return false;
}

static unsigned battle_reported_points;

static unsigned compare(Shadow* sh, const ShadowSnap* gb, const char* when, uint32_t index) {
    unsigned n = 0;
    char line[160], name[64];
    size_t used = strlen(sh->first_divergence);
    for(unsigned r = 0; r < sizeof(compare_ranges) / sizeof(compare_ranges[0]); r++) {
        for(uint16_t a = compare_ranges[r].start; a < compare_ranges[r].end; a++) {
            uint8_t ours = sh->E.wram[a - 0xC000], theirs = gb->wram[a - 0xC000];
            if(ours == theirs || excluded(a)) continue;
            if(n < 24 && sh->diverged_battles == 0 && battle_reported_points < 1) {
                wram_name(a, name, sizeof(name));
                snprintf(
                    line,
                    sizeof(line),
                    "  %s %u: %-36s engine $%02X  slave $%02X\n",
                    when,
                    index,
                    name,
                    ours,
                    theirs);
                if(used + strlen(line) < sizeof(sh->first_divergence)) {
                    strcat(sh->first_divergence, line);
                    used += strlen(line);
                }
            }
            n++;
        }
    }
    if(n) battle_reported_points++;
    uint8_t ours = sh->E.hram[hBattleTurn - 0xFF80], theirs = gb->hram[hBattleTurn - 0xFF80];
    if(ours != theirs) {
        snprintf(
            line,
            sizeof(line),
            "  %s %u: hBattleTurn engine %u slave %u\n",
            when,
            index,
            ours,
            theirs);
        if(used + strlen(line) < sizeof(sh->first_divergence)) strcat(sh->first_divergence, line);
        n++;
    }
    return n;
}

/* ---- slave hooks ---- */

static int gb_index(Shadow* sh, Gbh* h) {
    return sh->gb[0] == h ? 0 : 1;
}

static bool is_slave(Shadow* sh, Gbh* h) {
    return sh->started && sh->slave == gb_index(sh, h);
}

static void snap_take(Gbh* h, ShadowSnap* s) {
    gbh_snapshot(h, s->wram, s->hram);
    s->player_action = s->wram[wBattlePlayerAction - 0xC000];
    s->move_num = s->wram[wCurMoveNum - 0xC000];
    s->party_mon = s->wram[wCurPartyMon - 0xC000];
}

static void hook_do_battle(Gbh* h, void* ctx) {
    Shadow* sh = ctx;
    if(gbh_peek(h, 0, hSerialConnectionStatus) != USING_EXTERNAL_CLOCK) return;
    sh->slave = gb_index(sh, h);
    sh->started = true;
    sh->entries = sh->exits = 0;
    sh->ended = false;
    snap_take(h, &sh->start);
}

static void hook_exchange_entry(Gbh* h, void* ctx) {
    Shadow* sh = ctx;
    if(!is_slave(sh, h) || sh->entries >= SHADOW_MAX_EXCHANGES) return;
    snap_take(h, &sh->entry[sh->entries++]);
}

static void hook_exchange_exit(Gbh* h, void* ctx) {
    Shadow* sh = ctx;
    if(!is_slave(sh, h) || sh->exits >= SHADOW_MAX_EXCHANGES) return;
    sh->exit_action[sh->exits] = gbh_peek(h, 0, wBattleAction);
    sh->exit_sent[sh->exits] = gbh_peek(h, 0, wLinkBattleSentAction);
    sh->exits++;
}

/* --trace-ex: the slave's BattleRandom calls, by caller */
static void hook_battle_random(Gbh* h, void* ctx) {
    Shadow* sh = ctx;
    if(!sh->trace || !is_slave(sh, h)) return;
    uint16_t sp = gbh_sp(h);
    uint16_t ret = (uint16_t)(gbh_peek(h, 0, sp) | gbh_peek(h, 0, (uint16_t)(sp + 1)) << 8);
    const Sym* s = sym_by_addr(sh->syms, ret < 0x4000 ? 0 : gbh_rom_bank(h), ret);
    printf(
        "      slave random from %s+%u (RNs used %u)\n",
        s ? s->name : "?",
        s ? (unsigned)(ret - s->addr) : 0u,
        gbh_peek(h, 0, wLinkBattleRNCount));
}

static void hook_exit_battle(Gbh* h, void* ctx) {
    Shadow* sh = ctx;
    if(!is_slave(sh, h)) return;
    snap_take(h, &sh->end);
    sh->ended = true;
}

void shadow_init(Shadow* sh, Gbh* a, Gbh* b, const SymTable* syms, ShadowStep step, void* step_ctx) {
    memset(sh, 0, sizeof(*sh));
    sh->gb[0] = a;
    sh->gb[1] = b;
    sh->slave = -1;
    sh->syms = syms;
    sh->step = step;
    sh->step_ctx = step_ctx;
    sh->step_limit = 60 * 60 * 5;
    sh->entry = calloc(SHADOW_MAX_EXCHANGES, sizeof(ShadowSnap));
    wsyms_load(syms);
    const Sym* x = sym_get(syms, "LinkBattleSendReceiveAction");
    for(int i = 0; i < 2; i++) {
        gbh_hook(sh->gb[i], "DoBattle", hook_do_battle, sh);
        gbh_hook(sh->gb[i], "LinkBattleSendReceiveAction", hook_exchange_entry, sh);
        /* the ret after "farcall _LinkBattleSendReceiveAction" */
        gbh_hook_addr(sh->gb[i], x->bank, (uint16_t)(x->addr + 6), hook_exchange_exit, sh);
        gbh_hook(sh->gb[i], "ExitBattle", hook_exit_battle, sh);
        gbh_hook(sh->gb[i], "BattleRandom", hook_battle_random, sh);
    }
}

void shadow_free(Shadow* sh) {
    free(sh->entry);
    sh->entry = NULL;
}

/* ---- engine callbacks ---- */

static bool wait_for(Shadow* sh, bool (*cond)(Shadow*, uint32_t), uint32_t arg) {
    for(uint32_t f = 0; !cond(sh, arg); f++) {
        if(f >= sh->step_limit) return false;
        sh->step(sh->step_ctx);
    }
    return true;
}

static bool have_entry(Shadow* sh, uint32_t k) {
    return sh->entries > k || sh->ended;
}
static bool have_exit(Shadow* sh, uint32_t k) {
    return sh->exits > k || sh->ended;
}
static bool have_end(Shadow* sh, uint32_t k) {
    (void)k;
    return sh->ended;
}

static void diverge(Shadow* sh, const char* fmt, uint32_t k, unsigned a, unsigned b) {
    char line[200];
    snprintf(line, sizeof(line), fmt, k, a, b);
    size_t used = strlen(sh->first_divergence);
    if(used + strlen(line) + 2 < sizeof(sh->first_divergence)) {
        strcat(sh->first_divergence, line);
        strcat(sh->first_divergence, "\n");
    }
}

static const ShadowSnap* entry_for(Shadow* sh, uint32_t k) {
    if(!wait_for(sh, have_entry, k) || sh->entries <= k) return NULL;
    return &sh->entry[k];
}

static void sh_choose_action(void* ctx, const PkbEngine* E, PkbChoice* c) {
    (void)E;
    Shadow* sh = ctx;
    const ShadowSnap* s = entry_for(sh, sh->engine_exchanges);
    if(!s) return;
    c->switch_mon = s->player_action == BATTLEPLAYERACTION_SWITCH;
    c->slot = c->switch_mon ? s->party_mon : s->move_num;
    if(sh->trace) {
        printf(
            "    choose %u: slave action %u move %u party %u -> %s %u\n",
            sh->engine_exchanges,
            s->player_action,
            s->move_num,
            s->party_mon,
            c->switch_mon ? "switch" : "move",
            c->slot);
    }
}

static uint8_t sh_choose_replacement(void* ctx, const PkbEngine* E) {
    (void)E;
    Shadow* sh = ctx;
    const ShadowSnap* s = entry_for(sh, sh->engine_exchanges);
    if(sh->trace && s)
        printf("    replace %u: slave party %u\n", sh->engine_exchanges, s->party_mon);
    return s ? s->party_mon : 0;
}

static unsigned battle_divergences;

static void sh_on_random(void* ctx, const char* site, uint8_t value) {
    Shadow* sh = ctx;
    if(sh->trace) printf("      random %s: $%02X\n", site, value);
}

static void sh_on_exchange(void* ctx, const PkbEngine* E, uint8_t mine) {
    (void)mine;
    Shadow* sh = ctx;
    uint32_t k = sh->engine_exchanges;
    if(sh->trace) {
        printf(
            "  battle %u before exchange %u: mon %u hp %u move %u | enemy %u hp %u move %u | RNs used %u\n",
            sh->battles,
            k,
            pkb_engine_peek(E, wBattleMonSpecies),
            pkb_engine_peek16(E, wBattleMonHP),
            pkb_engine_peek(E, wCurPlayerMove),
            pkb_engine_peek(E, wEnemyMonSpecies),
            pkb_engine_peek16(E, wEnemyMonHP),
            pkb_engine_peek(E, wCurEnemyMove),
            pkb_engine_peek(E, wLinkBattleRNCount));
    }
    const ShadowSnap* s = entry_for(sh, k);
    if(!s) {
        diverge(sh, "  exchange %u: the slave never got there (%u/%u)", k, sh->entries, 0);
        battle_divergences++;
        return;
    }
    battle_divergences += compare(sh, s, "exchange", k);
}

static bool sh_exchange(void* ctx, uint8_t mine, uint8_t* theirs) {
    Shadow* sh = ctx;
    uint32_t k = sh->engine_exchanges++;
    if(!wait_for(sh, have_exit, k) || sh->exits <= k) {
        diverge(sh, "  exchange %u: the slave never completed it (%u exits) %u", k, sh->exits, 0);
        battle_divergences++;
        return false;
    }
    if(mine != sh->exit_sent[k]) {
        diverge(sh, "  exchange %u: engine sent $%X, slave sent $%X", k, mine, sh->exit_sent[k]);
        battle_divergences++;
    }
    *theirs = sh->exit_action[k];
    if(sh->trace)
        printf(
            "  exchange %u: engine $%X slave $%X master $%X\n", k, mine, sh->exit_sent[k], *theirs);
    return true;
}

void shadow_poll(Shadow* sh) {
    if(!sh->started || sh->running) return;
    sh->running = true;
    battle_divergences = 0;
    battle_reported_points = 0;
    if(sh->diverged_battles == 0) sh->first_divergence[0] = 0;

    PkbEngine* E = &sh->E;
    if(sh->use_init) {
        /* as the device does: only the parties and the random numbers */
        PkbLinkPartyData ours, theirs;
        const uint8_t* w = sh->start.wram;
#define SW(a) (w + ((a) - 0xC000))
        memcpy(ours.player_name, SW(wPlayerName), NAME_LENGTH);
        memcpy(ours.player_id, SW(wPlayerID), 2);
        ours.count = *SW(wPartyCount);
        memcpy(ours.species, SW(wPartySpecies), sizeof(ours.species));
        memcpy(ours.mons, SW(wPartyMons), sizeof(ours.mons));
        memcpy(ours.ot_names, SW(wPartyMonOTs), sizeof(ours.ot_names));
        memcpy(ours.nicknames, SW(wPartyMonNicknames), sizeof(ours.nicknames));
        memcpy(theirs.player_name, SW(wOTPlayerName), NAME_LENGTH);
        memcpy(theirs.player_id, SW(wOTPlayerID), 2);
        theirs.count = *SW(wOTPartyCount);
        memcpy(theirs.species, SW(wOTPartySpecies), sizeof(theirs.species));
        memcpy(theirs.mons, SW(wOTPartyMons), sizeof(theirs.mons));
        memcpy(theirs.ot_names, SW(wOTPartyMonOTs), sizeof(theirs.ot_names));
        memcpy(theirs.nicknames, SW(wOTPartyMonNicknames), sizeof(theirs.nicknames));
        pkb_engine_init(E, false, SW(wLinkBattleRNs), &ours, &theirs);
#undef SW
    } else {
        memset(E, 0, sizeof(*E));
        E->unsupported_effect = E->unsupported_command = 0xFF;
        memcpy(E->wram, sh->start.wram, sizeof(E->wram));
        memcpy(E->hram, sh->start.hram, sizeof(E->hram));
    }
    sh->engine_exchanges = 0;
    sh->io = (PkbEngineIo){
        .exchange = sh_exchange,
        .choose_action = sh_choose_action,
        .choose_replacement = sh_choose_replacement,
        .on_exchange = sh_on_exchange,
        .on_random = sh_on_random,
        .ctx = sh,
    };
    PkbBattleResult res = pkb_engine_run(E, &sh->io);
    if(!wait_for(sh, have_end, 0)) {
        diverge(
            sh,
            "  battle %u: the slave never reached ExitBattle (engine result %u) %u",
            sh->battles,
            res,
            0);
        battle_divergences++;
    } else {
        if(sh->engine_exchanges != sh->exits) {
            diverge(
                sh,
                "  battle end: engine made %u exchanges, slave %u%u",
                sh->engine_exchanges,
                sh->exits,
                0);
            battle_divergences++;
        }
        battle_divergences += compare(sh, &sh->end, "end", sh->battles);
    }
    sh->total_exchanges += sh->engine_exchanges;
    if(E->flags & PKB_ENGINE_UNSUPPORTED) sh->unsupported_battles++;
    if(E->flags & PKB_ENGINE_GAME_DESYNC) sh->game_desync_battles++;
    if(battle_divergences) {
        if(sh->diverged_battles == 0) {
            char head[96];
            snprintf(
                head,
                sizeof(head),
                "battle %u diverged (%u differences):\n",
                sh->battles,
                battle_divergences);
            size_t len = strlen(sh->first_divergence), hl = strlen(head);
            if(len + hl >= sizeof(sh->first_divergence)) {
                len = sizeof(sh->first_divergence) - hl - 1;
                sh->first_divergence[len] = 0;
            }
            memmove(sh->first_divergence + hl, sh->first_divergence, len + 1);
            memcpy(sh->first_divergence, head, strlen(head));
        }
        sh->diverged_battles++;
    }
    sh->battles++;
    sh->started = false;
    sh->running = false;
}
