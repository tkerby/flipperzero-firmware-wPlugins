/*
 * replay_log LOG.pbl
 *
 * Re-runs a battle from an engine battle log (device or harness) in the
 * engine alone: both parties and the GB's random numbers from the logged
 * blocks, our choices and the GB's actions from the logged exchanges. The
 * engine must make the same exchanges and reach the same state CRC at each
 * one. Exit status 0 if it does. -v also prints both active mons' HP at
 * each exchange.
 */
#include <pkb/engine.h>
#include <pkb/log.h>
#include <stdio.h>
#include <string.h>

#define MAX_EX 2048

typedef struct {
    uint8_t rn_rx[PKB_RN_BLOCK_LEN];
    uint8_t party_tx[PKB_PARTY_BLOCK_LEN], party_rx[PKB_PARTY_BLOCK_LEN];
    uint8_t patch_tx[PKB_PATCH_BLOCK_LEN], patch_rx[PKB_PATCH_BLOCK_LEN];
    unsigned have;
    uint8_t mine[MAX_EX], gb[MAX_EX];
    uint32_t crc[MAX_EX];
    uint32_t n;
    int result;
} Log;

typedef struct {
    const Log* L;
    uint32_t k;
    uint32_t crc_ok, crc_bad, crc_missing;
    bool mismatch;
    bool verbose;
} Replay;

static bool read_log(const char* path, Log* L) {
    static uint8_t buf[1 << 16];
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    size_t len = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    memset(L, 0, sizeof(*L));
    L->result = -1;
    if(len < PKB_LOG_HEADER_LEN || memcmp(buf, "PBL1", 4)) return false;
    for(size_t off = PKB_LOG_HEADER_LEN; off + 3 <= len;) {
        uint8_t type = buf[off];
        uint16_t n = (uint16_t)(buf[off + 1] | buf[off + 2] << 8);
        const uint8_t* p = buf + off + 3;
        if(off + 3 + n > len) return false;
        uint8_t* dst = NULL;
        size_t size = 0;
        switch(type) {
        case PkbLogRnRx:
            dst = L->rn_rx, size = sizeof(L->rn_rx);
            break;
        case PkbLogPartyTx:
            dst = L->party_tx, size = sizeof(L->party_tx);
            break;
        case PkbLogPartyRx:
            dst = L->party_rx, size = sizeof(L->party_rx);
            break;
        case PkbLogPatchTx:
            dst = L->patch_tx, size = sizeof(L->patch_tx);
            break;
        case PkbLogPatchRx:
            dst = L->patch_rx, size = sizeof(L->patch_rx);
            break;
        case PkbLogExchange:
            if(n == 8 && L->n < MAX_EX) {
                L->mine[L->n] = p[2];
                L->gb[L->n] = p[3];
                L->crc[L->n] = (uint32_t)(p[4] | p[5] << 8 | p[6] << 16 | (uint32_t)p[7] << 24);
                L->n++;
            }
            break;
        case PkbLogBattleEnd:
            if(n >= 1) L->result = p[0];
            break;
        default:
            break;
        }
        if(dst && n == size) {
            memcpy(dst, p, n);
            L->have |= 1u << type;
        }
        off += 3 + n;
    }
    return true;
}

static void rp_choose(void* ctx, const PkbEngine* E, PkbChoice* c) {
    (void)E;
    Replay* r = ctx;
    uint8_t n = r->k < r->L->n ? r->L->mine[r->k] : 0;
    c->switch_mon = n >= 4 && n <= 9;
    c->slot = c->switch_mon ? (uint8_t)(n - 4) : (uint8_t)(n & 3);
}

static uint8_t rp_replace(void* ctx, const PkbEngine* E) {
    (void)E;
    Replay* r = ctx;
    uint8_t n = r->k < r->L->n ? r->L->mine[r->k] : 4;
    return (uint8_t)(n - 4);
}

static void rp_on_exchange(void* ctx, const PkbEngine* E, uint8_t mine) {
    Replay* r = ctx;
    if(r->k >= r->L->n) return;
    if(mine != r->L->mine[r->k]) {
        printf("exchange %u: engine sends $%X, the log has $%X\n", r->k, mine, r->L->mine[r->k]);
        r->mismatch = true;
    }
    if(r->verbose) {
        uint16_t hp, max, ehp, emax;
        uint8_t sp = pkb_engine_active_mon(E, false, &hp, &max);
        uint8_t esp = pkb_engine_active_mon(E, true, &ehp, &emax);
        printf(
            "exchange %3u: ours #%u %u/%u, theirs #%u %u/%u; sends $%X, gets $%X\n",
            r->k,
            sp,
            hp,
            max,
            esp,
            ehp,
            emax,
            mine,
            r->L->gb[r->k]);
    }
    uint32_t crc = pkb_engine_state_crc(E);
    if(!r->L->crc[r->k])
        r->crc_missing++;
    else if(crc == r->L->crc[r->k])
        r->crc_ok++;
    else {
        if(!r->crc_bad)
            printf("exchange %u: state CRC %08X, the log has %08X\n", r->k, crc, r->L->crc[r->k]);
        r->crc_bad++;
    }
}

static bool rp_exchange(void* ctx, uint8_t mine, uint8_t* theirs) {
    (void)mine;
    Replay* r = ctx;
    if(r->k >= r->L->n) return false; /* the log ends here */
    *theirs = r->L->gb[r->k++];
    return true;
}

int main(int argc, char** argv) {
    static Log L;
    static PkbEngine E;
    bool verbose = argc > 1 && !strcmp(argv[1], "-v");
    if(verbose) argc--, argv++;
    if(argc < 2 || !read_log(argv[1], &L)) {
        fprintf(stderr, "usage: replay_log [-v] LOG.pbl (an engine battle log)\n");
        return 2;
    }
    PkbLinkPartyData ours, theirs;
    uint8_t rns[PKB_RNS_LEN];
    if(!pkb_party_block_parse(
           L.party_tx, sizeof(L.party_tx), L.patch_tx, sizeof(L.patch_tx), &ours) ||
       !pkb_party_block_parse(
           L.party_rx, sizeof(L.party_rx), L.patch_rx, sizeof(L.patch_rx), &theirs) ||
       !pkb_rn_block_parse(L.rn_rx, sizeof(L.rn_rx), rns)) {
        fprintf(stderr, "%s: missing or bad blocks\n", argv[1]);
        return 2;
    }
    Replay r = {.L = &L, .verbose = verbose};
    PkbEngineIo io = {
        .exchange = rp_exchange,
        .choose_action = rp_choose,
        .choose_replacement = rp_replace,
        .on_exchange = rp_on_exchange,
        .ctx = &r,
    };
    pkb_engine_init(&E, false, rns, &ours, &theirs);
    PkbBattleResult res = pkb_engine_run(&E, &io);
    static const int log_codes[] = {0, 1, 2, 3, 0};
    bool result_ok = L.result < 0 || L.result == 0 || log_codes[res] == L.result;
    bool ok = !r.mismatch && r.k == L.n && res != PkbBattleAborted && r.crc_bad == 0 && result_ok;
    printf(
        "%s: %u/%u exchanges, CRC %u matched, %u differ, %u not logged; result %d (log %d)%s",
        argv[1],
        r.k,
        L.n,
        r.crc_ok,
        r.crc_bad,
        r.crc_missing,
        log_codes[res],
        L.result,
        (E.flags & PKB_ENGINE_UNSUPPORTED) ? "; unsupported paths hit" : "");
    if(E.flags & PKB_ENGINE_GAME_DESYNC) printf("; game desync %u", E.game_desync);
    printf("\n");
    printf("%s\n", ok ? "REPLAY MATCHES" : "REPLAY DIFFERS");
    return ok ? 0 : 1;
}
