#include "engine_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mirror.h"
#include "pb_wram.h"

#define MAX_TURNS 1024

static void hook_do_battle(Gbh* h, void* ctx) {
    (void)h;
    EngineSession* es = ctx;
    es->gb_turns = 0;
}

static void hook_battle_turn(Gbh* h, void* ctx) {
    EngineSession* es = ctx;
    uint8_t hram[0x80];
    if(es->gb_turns < MAX_TURNS) gbh_snapshot(h, es->turn_wram[es->gb_turns], hram);
    es->gb_turns++;
}

void es_init(EngineSession* es, Gbh* gb, GbDriver* d, FlipperModel* fm, uint64_t seed) {
    memset(es, 0, sizeof(*es));
    es->gb = gb;
    es->d = d;
    es->fm = fm;
    es->wait_limit = 60 * 60 * 3;
    es->dump_every = 30;
    es->turn_wram = calloc(MAX_TURNS, 0x2000);
    pkb_policy_rng_seed(&es->rng, seed);
    pkb_ai_init(&es->ai, seed ^ 0xA1);
    gbh_hook(gb, "DoBattle", hook_do_battle, es);
    gbh_hook(gb, "BattleTurn", hook_battle_turn, es);
}

void es_free(EngineSession* es) {
    free(es->turn_wram);
    es->turn_wram = NULL;
}

void es_step(EngineSession* es) {
    char path[512];
    drv_frame(es->d);
    gbh_run_frame(es->gb);
    fm_tick(es->fm, gbh_ms(es->gb));
    if(es->dump && es->frame % es->dump_every == 0) {
        snprintf(
            path,
            sizeof(path),
            "%s/%s_%06llu.ppm",
            es->dump,
            es->gb->name,
            (unsigned long long)es->gb->frames);
        gbh_write_ppm(es->gb, path);
    }
    es->frame++;
}

static void note(EngineSession* es, const char* line) {
    size_t used = strlen(es->first_divergence);
    if(es->diverged == 0 && used + strlen(line) + 1 < sizeof(es->first_divergence)) {
        strcat(es->first_divergence, line);
    }
}

static bool es_exchange(void* ctx, uint8_t mine, uint8_t* theirs) {
    EngineSession* es = ctx;
    uint32_t k = es->posted++;
    pbl_battle_post_action(&es->fm->s, mine);
    for(uint32_t f = 0; !pbl_battle_exchange_result(&es->fm->s, k, theirs); f++) {
        if(f >= es->wait_limit) {
            note(es, "  the GB never completed an exchange\n");
            es->battle_div++;
            return false;
        }
        es_step(es);
    }
    return true;
}

static void es_on_exchange(void* ctx, const PkbEngine* E, uint8_t mine) {
    (void)mine;
    EngineSession* es = ctx;
    if(es->fm->ncrc < sizeof(es->fm->crc) / sizeof(es->fm->crc[0])) {
        es->fm->crc[es->fm->ncrc++] = pkb_engine_state_crc(E);
    }
}

/* the scripted nibble for the upcoming exchange, or -1 past the script */
static int scripted(const EngineSession* es) {
    return es->battles == 0 && es->posted < es->script_len ? es->script[es->posted] : -1;
}

static void es_choose(void* ctx, const PkbEngine* E, PkbChoice* c) {
    EngineSession* es = ctx;
    int n = scripted(es);
    if(n >= 0) {
        c->switch_mon = n >= 4 && n <= 9;
        c->slot = c->switch_mon ? (uint8_t)(n - 4) : (uint8_t)(n & 3);
    } else if(es->use_ai) {
        pkb_ai_choose_action(&es->ai, E, c);
    } else if(
        es->switch_one_in && pkb_policy_rng_below(&es->rng, es->switch_one_in) == 0 &&
        pkb_engine_can_switch(E)) {
        pkb_engine_random_switch(E, &es->rng, c);
    } else {
        pkb_engine_random_move(E, &es->rng, c);
    }
}

static uint8_t es_replace(void* ctx, const PkbEngine* E) {
    EngineSession* es = ctx;
    int n = scripted(es);
    if(n >= 4 && n <= 9) return (uint8_t)(n - 4);
    if(es->use_ai) return pkb_ai_choose_replacement(&es->ai, E);
    return pkb_engine_random_replacement(E, &es->rng);
}

static void es_on_turn(void* ctx, const PkbEngine* E) {
    EngineSession* es = ctx;
    uint32_t t = es->engine_turns++;
    for(uint32_t f = 0; es->gb_turns <= t; f++) {
        if(f >= es->wait_limit) {
            note(es, "  the GB never started that turn\n");
            es->battle_div++;
            return;
        }
        es_step(es);
    }
    if(t >= MAX_TURNS) return;
    char report[1024], head[64];
    unsigned n = mirror_compare(E->wram, es->turn_wram[t], report, sizeof(report), 10);
    if(n) {
        if(es->battle_div == 0) {
            snprintf(head, sizeof(head), "  turn %u (%u bytes):\n", t, n);
            note(es, head);
            note(es, report);
        }
        es->battle_div += n;
    }
}

void es_poll(EngineSession* es) {
    FlipperModel* fm = es->fm;
    if(!fm->battle_ready) return;
    fm->battle_ready = false;

    PblSession* s = &fm->s;
    PkbLinkPartyData ours, theirs;
    uint8_t rns[PKB_RNS_LEN];
    pkb_party_block_parse(fm->party, sizeof(fm->party), fm->patch, sizeof(fm->patch), &ours);
    pkb_party_block_parse(
        s->party_rx, sizeof(s->party_rx), s->patch_rx, sizeof(s->patch_rx), &theirs);
    pbl_master_rns(s, rns);
    pkb_engine_init(&es->E, false, rns, &ours, &theirs);
    es->posted = 0;
    es->engine_turns = 0;
    es->battle_div = 0;
    size_t notes = strlen(es->first_divergence);
    es->io = (PkbEngineIo){
        .exchange = es_exchange,
        .choose_action = es_choose,
        .choose_replacement = es_replace,
        .on_exchange = es_on_exchange,
        .on_turn = es_on_turn,
        .ctx = es,
    };
    PkbBattleResult res = pkb_engine_run(&es->E, &es->io);
    static const uint8_t log_codes[] = {0, 1, 2, 3, 0};
    fm->result = log_codes[res];
    pbl_battle_request_end(s);
    if(res == PkbBattleAborted) {
        note(es, "  the engine aborted\n");
        es->battle_div++;
    }
    es->exchanges += es->posted;
    if(es->E.flags & PKB_ENGINE_UNSUPPORTED) es->unsupported++;
    if(es->E.flags & PKB_ENGINE_GAME_DESYNC) {
        /* the GB computes this battle differently by design: not a port bug */
        es->game_desyncs++;
        es->first_divergence[notes] = 0;
        es->battle_div = 0;
    }
    if(es->battle_div) {
        if(es->diverged == 0) {
            char head[96];
            snprintf(head, sizeof(head), "battle %u diverged:\n", es->battles);
            size_t len = strlen(es->first_divergence), hl = strlen(head);
            if(len + hl < sizeof(es->first_divergence)) {
                memmove(es->first_divergence + hl, es->first_divergence, len + 1);
                memcpy(es->first_divergence, head, hl);
            }
        }
        es->diverged++;
    }
    es->battles++;
}
