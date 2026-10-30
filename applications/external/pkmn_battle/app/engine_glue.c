#include "engine_glue.h"

#define POLL_MS 2

static bool glue_exchange(void* ctx, uint8_t mine, uint8_t* theirs) {
    PkbEngineGlue* g = ctx;
    uint32_t k = g->posted++;
    pbl_battle_post_action(g->session, mine);
    while(!pbl_battle_exchange_result(g->session, k, theirs)) {
        if(atomic_load(&g->abort)) return false;
        furi_delay_ms(POLL_MS);
    }
    return true;
}

static void glue_on_exchange(void* ctx, const PkbEngine* E, uint8_t mine) {
    (void)mine;
    PkbEngineGlue* g = ctx;
    unsigned n = atomic_load(&g->ncrc);
    if(n < PKB_ENGINE_CRC_CAP) {
        g->crc[n] = pkb_engine_state_crc(E);
        atomic_store(&g->ncrc, n + 1);
    }
}

/* the AI (lib/pkb_core/ai) plays the Flipper's side */
static void glue_choose(void* ctx, const PkbEngine* E, PkbChoice* c) {
    PkbEngineGlue* g = ctx;
    pkb_ai_choose_action(&g->ai, E, c);
}

static uint8_t glue_replace(void* ctx, const PkbEngine* E) {
    PkbEngineGlue* g = ctx;
    return pkb_ai_choose_replacement(&g->ai, E);
}

static int32_t glue_thread(void* ctx) {
    PkbEngineGlue* g = ctx;
    pkb_engine_init(g->E, false, g->rns, &g->ours, &g->theirs);
    g->result = pkb_engine_run(g->E, &g->io);
    g->stack_free = furi_thread_get_stack_space(furi_thread_get_current_id());
    pbl_battle_request_end(g->session);
    atomic_store(&g->done, true);
    return 0;
}

bool pkb_engine_glue_start(
    PkbEngineGlue* g,
    PblSession* s,
    const uint8_t party_tx[PKB_PARTY_BLOCK_LEN],
    const uint8_t patch_tx[PKB_PATCH_BLOCK_LEN],
    uint64_t seed) {
    pkb_engine_glue_stop(g);
    if(!pkb_party_block_parse(
           party_tx, PKB_PARTY_BLOCK_LEN, patch_tx, PKB_PATCH_BLOCK_LEN, &g->ours) ||
       !pkb_party_block_parse(
           s->party_rx, PKB_PARTY_BLOCK_LEN, s->patch_rx, PKB_PATCH_BLOCK_LEN, &g->theirs) ||
       !pbl_master_rns(s, g->rns)) {
        return false;
    }
    if(!g->E) g->E = malloc(sizeof(PkbEngine));
    g->session = s;
    g->posted = 0;
    atomic_store(&g->ncrc, 0);
    atomic_store(&g->abort, false);
    atomic_store(&g->done, false);
    g->result = PkbBattleUnknown;
    g->stack_free = 0;
    pkb_ai_init(&g->ai, seed);
    g->io = (PkbEngineIo){
        .exchange = glue_exchange,
        .choose_action = glue_choose,
        .choose_replacement = glue_replace,
        .on_exchange = glue_on_exchange,
        .ctx = g,
    };
    g->thread = furi_thread_alloc_ex("PkbEngine", PKB_ENGINE_STACK, glue_thread, g);
    furi_thread_start(g->thread);
    return true;
}

void pkb_engine_glue_stop(PkbEngineGlue* g) {
    if(g->thread) {
        atomic_store(&g->abort, true);
        furi_thread_join(g->thread);
        furi_thread_free(g->thread);
        g->thread = NULL;
    }
}

void pkb_engine_glue_free(PkbEngineGlue* g) {
    pkb_engine_glue_stop(g);
    free(g->E);
    g->E = NULL;
}

bool pkb_engine_glue_running(const PkbEngineGlue* g) {
    return g->thread && !atomic_load(&g->done);
}

bool pkb_engine_glue_finished(const PkbEngineGlue* g) {
    return g->thread && atomic_load(&g->done);
}
