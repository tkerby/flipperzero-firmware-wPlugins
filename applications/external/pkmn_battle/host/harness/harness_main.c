/*
 * pkb_harness: SameBoy-based test harness (needs your own Crystal ROM).
 *
 *   boot    --rom R (--sav S | --state S) [--frames N] [--keys K] [--dump DIR] [--out S]
 *           run a save or state, optionally pressing keys; prints where the
 *           player is
 *   fixture --rom R --sav S --out colosseum.state
 *           boot the save (saved facing the Colosseum receptionist), get
 *           past the title screen and save the state the other commands use
 *   run     --rom R --state S [--battles N] [--seed X] [--out DIR] [--trace F] [--dump DIR]
 *           [--gb NIBBLES]
 *           the GB against our session (the Flipper app model)
 *   twin    --rom R --state S [--battles N] [--trace F] [--dump DIR] [--drift D]
 *           two GBs linked; gb2's clock runs D slower (default 0.001)
 *   replay  LOG.pbl --rom R --state S [--trace F] [--dump DIR]
 *           replay a device log: the Flipper's blocks, the GB's RNs, party
 *           and choices from the log; checks the exchanges match (engine
 *           logs: runs the engine with the logged choices, mirror- and
 *           CRC-checked)
 *   golden  --rom R --state S [--seed X] [--trace PREFIX] [--drift D]
 *           the same battle twice, the GB against our session and against a
 *           second GB playing the Flipper's part; their byte streams must
 *           agree apart from timing (see golden_compare)
 *   compare SESSION_TRACE --trace TWIN_TRACE
 *           golden_compare on two saved traces
 *   engine  --rom R --state S [--battles N] [--seed X] [--out DIR]
 *           the GB (random parties and choices) against our session and
 *           battle engine, as on the device; mirror-checked every turn.
 *           --ai lets the Flipper's AI choose; --levels LO-HI draws its
 *           random teams at those levels; --boss N [--boss-ace L] fields
 *           boss team N (pb_bosses order) instead
 *           --music trace|return|champion overlays the outgoing nicknames
 *           (see pkb/music_test.h) and checks the Game Boy's song id
 *           --sprite N installs trainer class N's picture (1..0x43) and checks
 *           the class, vTiles2, and that gym happiness was restored
 *   shadow  --rom R --state S [--battles N] [--seed X]
 *           two GBs with random parties battle; our engine runs alongside
 *           the slave from its perspective and must match its WRAM at
 *           every exchange (see shadow.h)
 *
 * --no-items gives the random parties no held items (the battles the seeds
 * gave before held items were added). --beat-up adds Beat Up to their move pool.
 *
 * run --repeat N runs the same battles N times and checks the traces agree.
 *
 * NIBBLES: comma-separated BATTLEACTION_* values for the GB, one per exchange
 * (0-3 move, 4-9 switch to party slot 0-5); random usable moves after them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge.h"
#include "driver.h"
#include "engine_session.h"
#include "flipper.h"
#include "fuzz_party.h"
#include "shadow.h"
#include "gbh.h"
#include "trace.h"

#include <Core/gb.h>

#include <pkb/charset.h>
#include <pkb/music_test.h>
#include <pkb/picture.h>

#define DEFAULT_MAX_FRAMES (60 * 60 * 30) /* 30 emulated minutes */

typedef struct {
    const char* rom;
    const char* sav;
    const char* state;
    const char* out;
    const char* dump;
    const char* keys;
    const char* sym;
    const char* trace;
    const char* gb;
    const char* log;
    unsigned frames;
    unsigned dump_every;
    unsigned max_frames;
    int battles;
    uint64_t seed;
    double drift;
    unsigned repeat;
    bool quiet;
    bool trace_ex;
    bool init;
    bool ai; /* engine: the Flipper's AI instead of random choices */
    unsigned level_lo, level_hi; /* engine: random teams at these levels */
    int boss; /* engine: this boss team (-1: random teams) */
    unsigned boss_ace;
    int music; /* engine: PKB_MUSIC_TRACE / RETURN / CHAMPION, or 0 */
    int sprite; /* engine: Crystal trainer class, or 0 */
} Args;

static const char* arg_val(int* i, int argc, char** argv) {
    if(*i + 1 >= argc) {
        fprintf(stderr, "%s needs a value\n", argv[*i]);
        exit(2);
    }
    return argv[++*i];
}

static void parse(Args* a, int argc, char** argv) {
    memset(a, 0, sizeof(*a));
    a->frames = 600;
    a->dump_every = 30;
    a->max_frames = DEFAULT_MAX_FRAMES;
    a->battles = 1;
    a->seed = 1;
    a->boss = -1;
    a->drift = 0.001;
    a->repeat = 1;
    for(int i = 2; i < argc; i++) {
        if(!strcmp(argv[i], "--rom"))
            a->rom = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--sav"))
            a->sav = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--state"))
            a->state = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--out"))
            a->out = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--dump"))
            a->dump = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--dump-every"))
            a->dump_every = (unsigned)atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--keys"))
            a->keys = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--sym"))
            a->sym = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--trace"))
            a->trace = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--gb"))
            a->gb = arg_val(&i, argc, argv);
        else if(!strcmp(argv[i], "--frames"))
            a->frames = (unsigned)atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--max-frames"))
            a->max_frames = (unsigned)atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--battles"))
            a->battles = atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--seed"))
            a->seed = strtoull(arg_val(&i, argc, argv), NULL, 0);
        else if(!strcmp(argv[i], "--drift"))
            a->drift = atof(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--repeat"))
            a->repeat = (unsigned)atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--quiet"))
            a->quiet = true;
        else if(!strcmp(argv[i], "--trace-ex"))
            a->trace_ex = true;
        else if(!strcmp(argv[i], "--init"))
            a->init = true;
        else if(!strcmp(argv[i], "--no-items"))
            fuzz_party_items = false;
        else if(!strcmp(argv[i], "--beat-up"))
            fuzz_party_beat_up = true;
        else if(!strcmp(argv[i], "--ai"))
            a->ai = true;
        else if(!strcmp(argv[i], "--levels"))
            sscanf(arg_val(&i, argc, argv), "%u-%u", &a->level_lo, &a->level_hi);
        else if(!strcmp(argv[i], "--boss"))
            a->boss = atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--boss-ace"))
            a->boss_ace = (unsigned)atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--sprite"))
            a->sprite = atoi(arg_val(&i, argc, argv));
        else if(!strcmp(argv[i], "--music")) {
            const char* m = arg_val(&i, argc, argv);
            if(!strcmp(m, "trace"))
                a->music = PKB_MUSIC_TRACE;
            else if(!strcmp(m, "return"))
                a->music = PKB_MUSIC_RETURN;
            else if(!strcmp(m, "champion"))
                a->music = PKB_MUSIC_CHAMPION;
            else {
                fprintf(stderr, "--music is trace, return or champion\n");
                exit(2);
            }
        } else if(argv[i][0] != '-' && !a->log)
            a->log = argv[i];
        else {
            fprintf(stderr, "unknown option %s\n", argv[i]);
            exit(2);
        }
    }
}

static void where(Gbh* h) {
    const Sym* at = sym_by_addr(h->syms, gbh_rom_bank(h), gbh_pc(h));
    printf(
        "%s: frame %llu  pc %02X:%04X (%s)  map %u:%u  x %u y %u  battle %u\n",
        h->name,
        (unsigned long long)h->frames,
        gbh_rom_bank(h),
        gbh_pc(h),
        at ? at->name : "?",
        gbh_rd(h, "wMapGroup", 0),
        gbh_rd(h, "wMapNumber", 0),
        gbh_rd(h, "wXCoord", 0),
        gbh_rd(h, "wYCoord", 0),
        gbh_rd(h, "wBattleMode", 0));
}

static void dump_frame(Gbh* h, const char* dir) {
    char path[512];
    if(!dir) return;
    snprintf(path, sizeof(path), "%s/%s_%06llu.ppm", dir, h->name, (unsigned long long)h->frames);
    gbh_write_ppm(h, path);
}

/* Key script: comma-separated "KEYS:frames" items, KEYS from "ABSTUDLR" (S =
 * select, T = start, U/D/L/R = d-pad) or "-" for none. */
static uint8_t key_mask(const char* k, size_t n) {
    uint8_t m = 0;
    for(size_t i = 0; i < n; i++) {
        switch(k[i]) {
        case 'A':
            m |= GBH_KEY_A;
            break;
        case 'B':
            m |= GBH_KEY_B;
            break;
        case 'S':
            m |= GBH_KEY_SELECT;
            break;
        case 'T':
            m |= GBH_KEY_START;
            break;
        case 'U':
            m |= GBH_KEY_UP;
            break;
        case 'D':
            m |= GBH_KEY_DOWN;
            break;
        case 'L':
            m |= GBH_KEY_LEFT;
            break;
        case 'R':
            m |= GBH_KEY_RIGHT;
            break;
        default:
            break;
        }
    }
    return m;
}

static void run_keys(Gbh* h, const char* script, const char* dump) {
    const char* p = script;
    while(p && *p) {
        const char* colon = strchr(p, ':');
        const char* comma = strchr(p, ',');
        if(!colon || (comma && comma < colon)) break;
        gbh_set_keys(h, key_mask(p, (size_t)(colon - p)));
        gbh_run_frames(h, (unsigned)atoi(colon + 1));
        dump_frame(h, dump);
        p = comma ? comma + 1 : NULL;
    }
    gbh_set_keys(h, 0);
}

static bool open_gb(Gbh* h, const Args* a, const SymTable* syms, const char* name) {
    if(!a->rom) {
        fprintf(stderr, "--rom is required\n");
        return false;
    }
    if(!gbh_open(h, a->rom, syms, name)) return false;
    if(!a->dump) gbh_set_rendering(h, false);
    if(a->sav && !gbh_load_battery(h, a->sav)) {
        fprintf(stderr, "cannot load %s\n", a->sav);
        return false;
    }
    return !a->state || gbh_load_state(h, a->state);
}

static int cmd_boot(Args* a, SymTable* syms) {
    Gbh h;
    if(!open_gb(&h, a, syms, "gb")) return 1;
    for(unsigned i = 0; i < a->frames; i += a->dump_every) {
        gbh_run_frames(&h, a->dump_every);
        dump_frame(&h, a->dump);
    }
    run_keys(&h, a->keys, a->dump);
    where(&h);
    int rc = 0;
    if(a->out && !gbh_save_state(&h, a->out)) {
        fprintf(stderr, "cannot write %s\n", a->out);
        rc = 1;
    }
    gbh_close(&h);
    return rc;
}

/* Title screen: Start skips the intro, Start opens the menu, A on CONTINUE,
 * A past the player summary; then the overworld after the fade-in. */
#define FIXTURE_KEYS "T:5,-:150,T:5,-:150,A:5,-:150,A:5,-:150"

static int cmd_fixture(Args* a, SymTable* syms) {
    Gbh h;
    if(!a->sav || !a->out) {
        fprintf(stderr, "fixture needs --sav and --out\n");
        return 2;
    }
    if(!open_gb(&h, a, syms, "gb")) return 1;
    gbh_run_frames(&h, 300);
    run_keys(&h, a->keys ? a->keys : FIXTURE_KEYS, a->dump);
    gbh_run_frames(&h, 60);
    where(&h);
    bool ok = gbh_rd(&h, "wMapGroup", 0) == 20 && gbh_rd(&h, "wMapNumber", 0) == 1;
    if(!ok) {
        fprintf(
            stderr, "not in a Pokecenter 2F: save the game facing the Colosseum receptionist\n");
    } else if(!gbh_save_state(&h, a->out)) {
        fprintf(stderr, "cannot write %s\n", a->out);
        ok = false;
    } else {
        printf("wrote %s\n", a->out);
    }
    gbh_close(&h);
    return ok ? 0 : 1;
}

static uint32_t parse_nibbles(const char* s, uint8_t* out, uint32_t cap) {
    uint32_t n = 0;
    while(s && *s && n < cap) {
        out[n++] = (uint8_t)strtoul(s, NULL, 0);
        s = strchr(s, ',');
        if(s) s++;
    }
    return n;
}

static void print_session(const FlipperModel* fm, const Trace* t) {
    printf(
        "flipper: battles %d, phase %s, link bytes %u, trace %zu bytes, hash %016llx\n",
        fm->battles,
        pbl_phase_name(atomic_load(&fm->s.phase)),
        fm->s.bytes,
        t->n,
        (unsigned long long)trace_hash(t));
}

static bool write_trace(const Args* a, const Trace* t) {
    if(!a->trace) return true;
    if(!trace_write(t, a->trace)) {
        fprintf(stderr, "cannot write %s\n", a->trace);
        return false;
    }
    return true;
}

/* GB vs the Flipper app model until the driver is done or time runs out. */
static bool run_session(const Args* a, Gbh* gb, GbDriver* d, FlipperModel* fm, Trace* t) {
    SessionBridge br;
    bridge_session_attach(&br, gb, &fm->s, t, 0x02 /* handshake reply, as pkb_link_start */);
    for(unsigned f = 0; f < a->max_frames && !d->done && !fm->error; f++) {
        drv_frame(d);
        gbh_run_frame(gb);
        fm_tick(fm, gbh_ms(gb));
        if(a->dump && f % a->dump_every == 0) dump_frame(gb, a->dump);
        if(!a->quiet && fm->last_log[0]) {
            printf("battle %d logged: %s\n", fm->battles, fm->last_log);
            fm->last_log[0] = 0;
        }
    }
    where(gb);
    if(fm->error) fprintf(stderr, "flipper model error\n");
    if(!d->done) fprintf(stderr, "GB driver not done after %u frames\n", a->max_frames);
    return d->done && !fm->error && fm->battles == d->battles;
}

static int cmd_run(Args* a, SymTable* syms) {
    static uint8_t script[256];
    static FlipperModel fm;
    uint64_t first_hash = 0;
    bool ok = true;
    for(unsigned rep = 0; rep < a->repeat && ok; rep++) {
        Gbh gb;
        GbDriver d;
        Trace t = {0};
        if(!open_gb(&gb, a, syms, "gb")) return 1;
        drv_init(&d, &gb, a->battles, a->seed ^ 0x6762);
        d.prof[DRV_AS_MASTER].script = script;
        d.prof[DRV_AS_MASTER].script_len = parse_nibbles(a->gb, script, sizeof(script));
        fm_init(&fm, a->seed, a->out);
        ok = run_session(a, &gb, &d, &fm, &t);
        print_session(&fm, &t);
        if(rep == 0) {
            first_hash = trace_hash(&t);
            ok = write_trace(a, &t) && ok;
        } else if(trace_hash(&t) != first_hash) {
            printf("run %u differs from run 1\n", rep + 1);
            ok = false;
        }
        trace_free(&t);
        gbh_close(&gb);
    }
    if(ok && a->repeat > 1) printf("%u runs identical\n", a->repeat);
    printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}

/* ---- golden trace: our session vs a real (emulated) slave GB ---- */

/*
 * The two runs differ in timing only if our session is right: a real slave
 * GB answers later (its code runs once per frame and its shift register
 * lags one transfer), idles on $FE/$00 and repeats values a different number
 * of times. So each side's byte stream is reduced to its values: idle bytes
 * ($00, $FE) dropped and repeats collapsed. The master's stream must come
 * out the same (the GB cannot tell the difference), and so must ours and
 * the real slave's, payloads included (the slave GB holds the Flipper's
 * party, trainer and RNs in a golden run).
 */
static size_t reduce(const Trace* t, int by, bool slave, uint8_t* out, size_t cap) {
    size_t n = 0;
    for(size_t i = 0; i < t->n && n < cap; i++) {
        if(by >= 0 && t->e[i].by != by) continue;
        uint8_t v = slave ? t->e[i].slave : t->e[i].master;
        if(v == 0x00 || v == PKB_SERIAL_NO_DATA_BYTE) continue;
        if(n && out[n - 1] == v) continue;
        out[n++] = v;
    }
    return n;
}

static bool
    reduced_equal(const char* what, const uint8_t* a, size_t na, const uint8_t* b, size_t nb) {
    size_t i = 0;
    while(i < na && i < nb && a[i] == b[i])
        i++;
    if(i == na && i == nb) {
        printf("%s: %zu values, identical\n", what, na);
        return true;
    }
    printf("%s: session %zu values, twin %zu; first difference at %zu:\n", what, na, nb, i);
    for(int k = 0; k < 2; k++) {
        const uint8_t* v = k ? b : a;
        size_t n = k ? nb : na;
        printf("  %-7s", k ? "twin" : "session");
        for(size_t j = i >= 4 ? i - 4 : 0; j < n && j < i + 8; j++)
            printf(" %s%02X", j == i ? ">" : "", v[j]);
        printf("\n");
    }
    return false;
}

static bool golden_compare(const Trace* session, const Trace* twin) {
    enum {
        CAP = 1 << 16
    };
    static uint8_t a[CAP], b[CAP];
    uint64_t by[2] = {0, 0};
    for(size_t i = 0; i < twin->n; i++)
        by[twin->e[i].by & 1]++;
    int master = by[1] > by[0]; /* the GB that clocked the session */
    size_t na = reduce(session, -1, false, a, CAP), nb = reduce(twin, master, false, b, CAP);
    bool ok = reduced_equal("master bytes", a, na, b, nb);
    na = reduce(session, -1, true, a, CAP);
    nb = reduce(twin, master, true, b, CAP);
    return reduced_equal("slave bytes ", a, na, b, nb) && ok;
}

/* Configure a driver for a golden run: as master it sends `gb_rns`; as
 * slave it plays the Flipper's part (its party, trainer, RNs, move 1). */
static void
    golden_profiles(GbDriver* d, const uint8_t gb_rns[PKB_RNS_LEN], const FlipperModel* fm) {
    static const uint8_t move1[255] = {0};
    DrvProfile* m = &d->prof[DRV_AS_MASTER];
    DrvProfile* s = &d->prof[DRV_AS_SLAVE];
    m->inject_rns = true;
    memcpy(m->rns, gb_rns, PKB_RNS_LEN);
    s->script = move1;
    s->script_len = sizeof(move1);
    s->inject_rns = pkb_rn_block_parse(fm->rn, sizeof(fm->rn), s->rns);
    s->inject_party = pkb_party_block_parse(
        fm->party, sizeof(fm->party), fm->patch, sizeof(fm->patch), &s->party);
    s->inject_trainer = true;
}

static bool run_twin(const Args* a, Gbh gb[2], GbDriver d[2], TwinBridge* br, Trace* t) {
    bridge_twin_attach(br, &gb[0], &gb[1], t);
    br->rate1 = 1.0 + a->drift;
    for(unsigned f = 0; f < a->max_frames && !(d[0].done && d[1].done); f++) {
        for(int i = 0; i < 2; i++)
            drv_frame(&d[i]);
        bridge_twin_run_frame(br, 0);
        if(a->dump && f % a->dump_every == 0) {
            dump_frame(&gb[0], a->dump);
            dump_frame(&gb[1], a->dump);
        }
    }
    where(&gb[0]);
    where(&gb[1]);
    printf(
        "twin: battles %d/%d, transfers clocked by gb1 %llu, gb2 %llu, hash %016llx\n",
        d[0].battles,
        d[1].battles,
        (unsigned long long)br->bytes_by[0],
        (unsigned long long)br->bytes_by[1],
        (unsigned long long)trace_hash(t));
    return d[0].done && d[1].done;
}

static int cmd_golden(Args* a, SymTable* syms) {
    static FlipperModel fm;
    static const char* names[2] = {"gb1", "gb2"};
    uint8_t gb_rns[PKB_RNS_LEN];
    PkbPolicyRng r;
    char path[512];
    pkb_policy_rng_seed(&r, a->seed ^ 0x524E);
    for(unsigned i = 0; i < PKB_RNS_LEN; i++)
        gb_rns[i] = (uint8_t)pkb_policy_rng_below(&r, PKB_SERIAL_PREAMBLE_BYTE);
    fm_init(&fm, a->seed, NULL);

    /* B first: the twin, while fm still holds the blocks it will send in A */
    Gbh gb[2];
    GbDriver d[2];
    TwinBridge br;
    Trace tb = {0};
    for(int i = 0; i < 2; i++) {
        if(!open_gb(&gb[i], a, syms, names[i])) return 1;
        drv_init(&d[i], &gb[i], 1, a->seed ^ 0x6762);
        golden_profiles(&d[i], gb_rns, &fm);
    }
    bool ok_b = run_twin(a, gb, d, &br, &tb);
    gbh_close(&gb[0]);
    gbh_close(&gb[1]);

    /* A: the GB against our session */
    Gbh g;
    GbDriver dm;
    Trace ta = {0};
    if(!open_gb(&g, a, syms, "gb")) return 1;
    drv_init(&dm, &g, 1, a->seed ^ 0x6762);
    golden_profiles(&dm, gb_rns, &fm);
    bool ok_a = run_session(a, &g, &dm, &fm, &ta);
    print_session(&fm, &ta);
    gbh_close(&g);

    if(a->trace) {
        snprintf(path, sizeof(path), "%s.session.txt", a->trace);
        trace_write(&ta, path);
        snprintf(path, sizeof(path), "%s.twin.txt", a->trace);
        trace_write(&tb, path);
    }
    bool ok = ok_a && ok_b && golden_compare(&ta, &tb);
    trace_free(&ta);
    trace_free(&tb);
    printf("%s\n", ok ? "GOLDEN MATCHES" : "GOLDEN DIFFERS");
    return ok ? 0 : 1;
}

static int cmd_compare(Args* a) {
    Trace ta = {0}, tb = {0};
    if(!a->log || !a->trace || !trace_read(&ta, a->log) || !trace_read(&tb, a->trace)) {
        fprintf(stderr, "compare SESSION_TRACE --trace TWIN_TRACE\n");
        return 2;
    }
    bool ok = golden_compare(&ta, &tb);
    trace_free(&ta);
    trace_free(&tb);
    printf("%s\n", ok ? "GOLDEN MATCHES" : "GOLDEN DIFFERS");
    return ok ? 0 : 1;
}

static int cmd_twin(Args* a, SymTable* syms) {
    Gbh gb[2];
    GbDriver d[2];
    TwinBridge br;
    Trace t = {0};
    static const char* names[2] = {"gb1", "gb2"};
    for(int i = 0; i < 2; i++) {
        if(!open_gb(&gb[i], a, syms, names[i])) return 1;
        drv_init(&d[i], &gb[i], a->battles, a->seed + (uint64_t)i);
    }
    bool ok = run_twin(a, gb, d, &br, &t);
    ok = write_trace(a, &t) && ok;
    trace_free(&t);
    gbh_close(&gb[0]);
    gbh_close(&gb[1]);
    printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}

/* ---- shadow engine fuzzing ---- */

typedef struct {
    Gbh* gb;
    GbDriver* d;
    TwinBridge* br;
} TwinCtx;

static void twin_step(void* ctx) {
    TwinCtx* t = ctx;
    for(int i = 0; i < 2; i++)
        drv_frame(&t->d[i]);
    bridge_twin_run_frame(t->br, 0);
}

static void make_fuzz_party(void* ctx, PkbLinkPartyData* p) {
    fuzz_party(ctx, p);
}

static int cmd_shadow(Args* a, SymTable* syms) {
    static Gbh gb[2];
    static GbDriver d[2];
    static TwinBridge br;
    static Shadow sh;
    static PkbPolicyRng party_rng[2];
    static const char* names[2] = {"gb1", "gb2"};
    for(int i = 0; i < 2; i++) {
        if(!open_gb(&gb[i], a, syms, names[i])) return 1;
        drv_init(&d[i], &gb[i], a->battles, a->seed * 2 + (uint64_t)i);
        d[i].switch_one_in = 6;
        pkb_policy_rng_seed(&party_rng[i], a->seed * 977 + (uint64_t)i);
        for(int r = 0; r < 2; r++) {
            d[i].prof[r].inject_party = true;
            d[i].prof[r].make_party = make_fuzz_party;
            d[i].prof[r].make_party_ctx = &party_rng[i];
        }
    }
    bridge_twin_attach(&br, &gb[0], &gb[1], NULL);
    br.rate1 = 1.0 + a->drift;
    TwinCtx tc = {gb, d, &br};
    shadow_init(&sh, &gb[0], &gb[1], syms, twin_step, &tc);
    sh.verbose = !a->quiet;
    sh.trace = a->trace_ex;
    sh.use_init = a->init;
    unsigned limit = a->max_frames != DEFAULT_MAX_FRAMES ? a->max_frames :
                                                           (unsigned)a->battles * 30000u + 20000u;
    uint32_t reported = 0;
    for(unsigned f = 0; f < limit && !(d[0].done && d[1].done); f++) {
        twin_step(&tc);
        shadow_poll(&sh);
        if(sh.battles != reported && sh.battles % 10 == 0) {
            reported = sh.battles;
            printf(
                "%u battles, %u exchanges, %u diverged, %u game desyncs, %u hit unsupported paths\n",
                sh.battles,
                sh.total_exchanges,
                sh.diverged_battles,
                sh.game_desync_battles,
                sh.unsupported_battles);
            fflush(stdout);
        }
    }
    where(&gb[0]);
    where(&gb[1]);
    if(a->dump) {
        dump_frame(&gb[0], a->dump);
        dump_frame(&gb[1], a->dump);
    }
    if(br.lost_irqs[0] + br.lost_irqs[1]) {
        printf(
            "link: re-requested %u + %u serial interrupts the game's VBlank lost\n",
            br.lost_irqs[0],
            br.lost_irqs[1]);
    }
    printf(
        "shadow: %u battles, %u exchanges, %u diverged, %u game desyncs, %u hit unsupported paths\n",
        sh.battles,
        sh.total_exchanges,
        sh.diverged_battles,
        sh.game_desync_battles,
        sh.unsupported_battles);
    if(sh.first_divergence[0]) printf("first divergence:\n%s", sh.first_divergence);
    bool ok = sh.battles >= (uint32_t)a->battles && sh.diverged_battles == 0;
    shadow_free(&sh);
    gbh_close(&gb[0]);
    gbh_close(&gb[1]);
    printf("%s\n", ok ? "ZERO DIVERGENCE" : "DIVERGED");
    return ok ? 0 : 1;
}

/* ---- Colosseum music overlay (engine --music) ---- */

typedef struct {
    int mode;
    uint8_t nicks[PKB_NAME_LEN * PKB_PARTY_LENGTH];
    uint8_t ots[PKB_NAME_LEN * PKB_PARTY_LENGTH];
    int prints;
    uint16_t first_de;
    uint8_t music_at_print; /* wChannel1MusicID, the song PlayMusic loaded */
    uint8_t music_high;
    uint8_t bank_at_print;
    int flag_at_second; /* -1 until a second print of the nickname */
    int play_calls; /* PlayMusic after DoBattle starts */
    int play_n;
    uint16_t play_song[12];
    uint16_t play_ret[12];
    uint16_t song_at_battle; /* de of the last PlayMusic before the nickname print */
    bool in_battle;
    uint8_t music_turn0, music_later;
    int turns;
    bool party_nicks_ok, enemy_nick_ok, ots_ok;
} MusicWatch;

static bool music_eq(Gbh* h, const char* sym, const uint8_t* want, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(gbh_rd(h, sym, (unsigned)i) != want[i]) return false;
    }
    return true;
}

static bool music_is_nick(Gbh* h, uint16_t de, const uint8_t* nick) {
    uint8_t bank = (uint8_t)(gbh_peek(h, 0, 0xFF70) & 7);
    for(int i = 0; i < PKB_NAME_LEN && nick[i] != PKB_GB_TERMINATOR; i++) {
        if(gbh_peek(h, bank, (uint16_t)(de + i)) != nick[i]) return false;
    }
    return true;
}

static void music_on_place(Gbh* h, void* ctx) {
    MusicWatch* w = ctx;
    uint16_t de = gbh_de(h);
    if(!music_is_nick(h, de, w->nicks)) return;
    w->prints++;
    if(w->prints == 1) {
        w->first_de = de;
        w->music_at_print = gbh_rd(h, "wChannel1MusicID", 0);
        w->music_high = gbh_rd(h, "wChannel1MusicID", 1);
        w->bank_at_print = (uint8_t)(gbh_peek(h, 0, 0xFF70) & 7);
        w->party_nicks_ok = music_eq(h, "wOTPartyMonNicknames", w->nicks, sizeof(w->nicks));
        w->ots_ok = music_eq(h, "wOTPartyMonOTs", w->ots, sizeof(w->ots));
    } else if(w->prints == 2) {
        w->flag_at_second = gbh_rd(h, "wOTPartyMonOTs", PKB_NAME_LEN * PKB_PARTY_LENGTH - 1);
    }
}

static void music_on_play(Gbh* h, void* ctx) {
    MusicWatch* w = ctx;
    uint16_t song = gbh_de(h);
    uint16_t sp = gbh_sp(h);
    uint16_t ret = (uint16_t)(gbh_peek(h, 1, sp) | (gbh_peek(h, 1, (uint16_t)(sp + 1)) << 8));
    if(w->play_n < (int)(sizeof(w->play_song) / sizeof(w->play_song[0]))) {
        w->play_song[w->play_n] = song;
        w->play_ret[w->play_n] = ret;
        w->play_n++;
    }
    if(w->prints == 0) w->song_at_battle = song;
    if(w->in_battle) w->play_calls++;
}

static void music_on_battle(Gbh* h, void* ctx) {
    (void)h;
    ((MusicWatch*)ctx)->in_battle = true;
}

static void music_on_turn(Gbh* h, void* ctx) {
    MusicWatch* w = ctx;
    uint8_t id = gbh_rd(h, "wChannel1MusicID", 0);
    if(w->turns == 0) {
        w->music_turn0 = id;
        w->enemy_nick_ok = music_eq(h, "wEnemyMonNickname", w->nicks, PKB_NAME_LEN);
        if(!w->party_nicks_ok)
            w->party_nicks_ok = music_eq(h, "wOTPartyMonNicknames", w->nicks, sizeof(w->nicks));
        if(!w->ots_ok) w->ots_ok = music_eq(h, "wOTPartyMonOTs", w->ots, sizeof(w->ots));
    } else if(w->turns == 1) {
        w->music_later = id;
    }
    w->turns++;
}

static void music_watch_init(MusicWatch* w, const FlipperModel* fm) {
    PkbLinkPartyData d;
    memset(w, 0, sizeof(*w));
    w->flag_at_second = -1;
    w->mode = fm->music_test;
    pkb_party_block_parse(fm->party, PKB_PARTY_BLOCK_LEN, fm->patch, PKB_PATCH_BLOCK_LEN, &d);
    memcpy(w->nicks, d.nicknames, sizeof(w->nicks));
    memcpy(w->ots, d.ot_names, sizeof(w->ots));
}

static int music_from_wram(const MusicWatch* w, int song) {
    int n = 0;
    for(int i = 0; i < w->play_n; i++) {
        if(w->play_ret[i] < 0xC000) continue;
        if(song < 0 || w->play_song[i] == (uint16_t)song) n++;
    }
    return n;
}

static bool music_watch_ok(const MusicWatch* w) {
    /* Channel 1 keeps the song PlayMusic loaded. wMusicID does not: cries reuse it.
     * The trigger is the OT player name, so the nickname prints are the real
     * species names. The once-flag lives in hUnusedByte or the music cave, not
     * in a byte the nickname compare can see after the first visit. */
    bool names = w->prints >= 1 && w->first_de == 0xC616 && w->party_nicks_ok && w->enemy_nick_ok;
    if(w->mode == PKB_MUSIC_TRACE) {
        return names && w->ots_ok && w->music_at_print == (uint8_t)w->song_at_battle &&
               w->music_at_print != 0 && w->music_turn0 == w->music_at_print &&
               music_from_wram(w, -1) == 0;
    }
    if(w->mode == PKB_MUSIC_RETURN) {
        return names && music_from_wram(w, -1) == 0 &&
               w->music_turn0 == (uint8_t)w->song_at_battle;
    }
    return names && music_from_wram(w, -1) == 1 && music_from_wram(w, PKB_SONG_CHAMPION) == 1 &&
           w->music_turn0 == PKB_SONG_CHAMPION &&
           (w->turns < 2 || w->music_later == PKB_SONG_CHAMPION);
}

static void music_watch_report(const MusicWatch* w) {
    printf(
        "music: prints %d first-de $%04X ch1-at-print $%02X (high $%02X) bank %u "
        "play-calls-in-battle %d flag-at-2nd %d turns %d ch1-turn0 $%02X ch1-later $%02X "
        "song-before-print $%04X\n",
        w->prints,
        w->first_de,
        w->music_at_print,
        w->music_high,
        w->bank_at_print,
        w->play_calls,
        w->flag_at_second,
        w->turns,
        w->music_turn0,
        w->music_later,
        w->song_at_battle);
    printf(
        "music bytes: party-nicks %s enemy-nick %s ot-names %s\n",
        w->party_nicks_ok ? "ok" : "NO",
        w->enemy_nick_ok ? "ok" : "NO",
        w->ots_ok ? "ok" : "NO");
    for(int i = 0; i < w->play_n; i++) {
        printf("  PlayMusic de=$%04X ret=$%04X\n", w->play_song[i], w->play_ret[i]);
    }
}

/* ---- trainer picture (--sprite) ---- */

typedef struct {
    uint8_t want;
    uint8_t happy_at_music[6];
    uint8_t bhp_at_music;
    uint8_t unused_at_music;
    int saw_music, saw_pic, saw_graphic, bad_class;
    uint8_t tclass;
    uint16_t de;
    uint32_t vram;
    uint8_t happy_at_battle[6];
    uint8_t bhp_at_battle;
    uint8_t bhp_at_turn;
    int saw_battle, saw_turn;
} SpriteWatch;

static uint32_t sprite_vram_hash(Gbh* h) {
    size_t size = 0;
    uint16_t bank = 0;
    uint8_t* vram = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_VRAM, &size, &bank);
    /* Bank 0 is the first 8KB. vTiles2 is $9000, offset $1000. 7*7 tiles. */
    if(!vram || size < 0x1000 + 7 * 7 * 16) return 0;
    uint32_t hsh = 2166136261u;
    for(unsigned i = 0; i < 7 * 7 * 16; i++) {
        hsh ^= vram[0x1000 + i];
        hsh *= 16777619u;
    }
    return hsh;
}

static void sprite_on_music(Gbh* h, void* ctx) {
    SpriteWatch* w = ctx;
    if(w->saw_music) return;
    w->saw_music = 1;
    w->unused_at_music = gbh_rd(h, "hUnusedByte", 0);
    w->bhp_at_music = gbh_rd(h, "wBattleMonHappiness", 0);
    for(int i = 0; i < 6; i++)
        w->happy_at_music[i] = gbh_rd(h, "wPartyMon1Happiness", (unsigned)(i * 0x30));
}

static void sprite_on_pic(Gbh* h, void* ctx) {
    SpriteWatch* w = ctx;
    uint8_t tclass = gbh_rd(h, "wTrainerClass", 0);
    if(!w->saw_pic) {
        w->saw_pic = 1;
        w->tclass = tclass;
        w->de = gbh_de(h);
    }
    if(tclass != w->want) w->bad_class = 1;
}

static void sprite_on_graphic(Gbh* h, void* ctx) {
    SpriteWatch* w = ctx;
    if(!w->saw_pic || w->saw_graphic) return;
    w->saw_graphic = 1;
    w->vram = sprite_vram_hash(h);
}

static void sprite_on_battle(Gbh* h, void* ctx) {
    SpriteWatch* w = ctx;
    if(w->saw_battle) return;
    w->saw_battle = 1;
    w->bhp_at_battle = gbh_rd(h, "wBattleMonHappiness", 0);
    for(int i = 0; i < 6; i++)
        w->happy_at_battle[i] = gbh_rd(h, "wPartyMon1Happiness", (unsigned)(i * 0x30));
}

static void sprite_on_turn(Gbh* h, void* ctx) {
    SpriteWatch* w = ctx;
    if(w->saw_turn) return;
    w->saw_turn = 1;
    w->bhp_at_turn = gbh_rd(h, "wBattleMonHappiness", 0);
}

/* Direct VRAM hash of CalPic at vTiles2 for this ROM, from --sprite 0x0C. */
static const uint32_t k_cal_vram = 0x995EA1CAu;

static bool sprite_watch_ok(const SpriteWatch* w) {
    bool happy = w->saw_battle;
    for(int i = 0; i < 6 && happy; i++)
        happy = w->happy_at_battle[i] == w->happy_at_music[i];
    /* InitBattleMon copies party happiness into wBattleMonHappiness after
     * DoBattle starts. The cave restores the party bytes before that. The
     * value at DoBattle's first instruction is still the gym-battle write;
     * the first turn is the one the CRC sees. */
    bool tiles = w->vram != 0 && (w->want == PKB_TRAINER_CLASS_CAL ? w->vram == k_cal_vram :
                                                                     w->vram != k_cal_vram);
    return w->saw_pic && !w->bad_class && w->tclass == w->want && w->de == 0x9000 &&
           w->saw_graphic && tiles && happy && w->saw_turn &&
           w->bhp_at_turn == w->happy_at_music[0];
}

static void sprite_watch_report(const SpriteWatch* w) {
    printf(
        "sprite: unused $%02X class $%02X (want $%02X) de $%04X vram %08X\n",
        w->unused_at_music,
        w->tclass,
        w->want,
        w->de,
        w->vram);
    printf("sprite happy music");
    for(int i = 0; i < 6; i++)
        printf(" %02X", w->happy_at_music[i]);
    printf(" bhp %02X\nsprite happy battle", w->bhp_at_music);
    for(int i = 0; i < 6; i++)
        printf(" %02X", w->happy_at_battle[i]);
    printf(" bhp %02X turn-bhp %02X\n", w->bhp_at_battle, w->bhp_at_turn);
    printf("sprite items: not restored (the 66-byte cave has no room)\n");
}

/* ---- the engine against a GB through our session (the device setup) ---- */

static int cmd_engine(Args* a, SymTable* syms) {
    static FlipperModel fm;
    static EngineSession es;
    static PkbPolicyRng party_rng;
    Gbh gb;
    GbDriver d;
    if(!open_gb(&gb, a, syms, "gb")) return 1;
    drv_init(&d, &gb, a->battles, a->seed ^ 0x6762);
    d.switch_one_in = 6;
    pkb_policy_rng_seed(&party_rng, a->seed * 977);
    if(!a->sav) { /* random GB parties unless running the save's own party */
        d.prof[DRV_AS_MASTER].inject_party = true;
        d.prof[DRV_AS_MASTER].make_party = make_fuzz_party;
        d.prof[DRV_AS_MASTER].make_party_ctx = &party_rng;
    }
    fm_init_engine(&fm, a->seed, a->out);
    SessionBridge br;
    bridge_session_attach(&br, &gb, &fm.s, NULL, 0x02);
    es_init(&es, &gb, &d, &fm, a->seed ^ 0x45);
    es.switch_one_in = 6;
    es.use_ai = a->ai;
    if(a->level_lo) {
        fm.level_lo = (uint8_t)a->level_lo;
        fm.level_hi = (uint8_t)(a->level_hi ? a->level_hi : a->level_lo);
    }
    fm.boss = a->boss;
    fm.boss_ace = (uint8_t)a->boss_ace;
    SpriteWatch sprite;
    memset(&sprite, 0, sizeof(sprite));
    if(a->sprite) {
        const Sym* pm = sym_find(syms, "PlayMusic");
        if(!pm || pm->bank != 0 || pm->addr != PKB_PLAY_MUSIC_ADDR) {
            fprintf(
                stderr,
                "PlayMusic is %02X:%04X; this payload is built for 00:%04X (Crystal UE 1.0)\n",
                pm ? pm->bank : 0,
                pm ? pm->addr : 0,
                PKB_PLAY_MUSIC_ADDR);
            es_free(&es);
            gbh_close(&gb);
            return 1;
        }
        if(a->sprite < 1 || a->sprite > (int)PKB_TRAINER_CLASS_MAX ||
           !fm_set_sprite(&fm, (uint8_t)a->sprite)) {
            fprintf(stderr, "sprite class did not fit the party block\n");
            es_free(&es);
            gbh_close(&gb);
            return 1;
        }
        sprite.want = (uint8_t)a->sprite;
        gbh_hook(&gb, "PlayBattleMusic", sprite_on_music, &sprite);
        gbh_hook(&gb, "GetTrainerPic", sprite_on_pic, &sprite);
        gbh_hook(&gb, "PlaceGraphic", sprite_on_graphic, &sprite);
        gbh_hook(&gb, "DoBattle", sprite_on_battle, &sprite);
        gbh_hook(&gb, "BattleTurn", sprite_on_turn, &sprite);
    }
    MusicWatch watch;
    memset(&watch, 0, sizeof(watch));
    if(a->music) {
        const Sym* pm = sym_find(syms, "PlayMusic");
        if(!pm || pm->bank != 0 || pm->addr != PKB_PLAY_MUSIC_ADDR) {
            fprintf(
                stderr,
                "PlayMusic is %02X:%04X; this payload is built for 00:%04X (Crystal UE 1.0)\n",
                pm ? pm->bank : 0,
                pm ? pm->addr : 0,
                PKB_PLAY_MUSIC_ADDR);
            es_free(&es);
            gbh_close(&gb);
            return 1;
        }
        if(!fm_set_music(&fm, a->music)) {
            fprintf(stderr, "music overlay did not fit the party block\n");
            es_free(&es);
            gbh_close(&gb);
            return 1;
        }
        music_watch_init(&watch, &fm);
        gbh_hook(&gb, "PlaceString", music_on_place, &watch);
        gbh_hook(&gb, "PlayMusic", music_on_play, &watch);
        gbh_hook(&gb, "DoBattle", music_on_battle, &watch);
        gbh_hook(&gb, "BattleTurn", music_on_turn, &watch);
    }
    es.dump = a->dump;
    es.dump_every = a->dump_every;
    unsigned limit = a->max_frames != DEFAULT_MAX_FRAMES ? a->max_frames :
                                                           (unsigned)a->battles * 30000u + 20000u;
    uint32_t reported = 0;
    for(unsigned f = 0; f < limit && !d.done && !fm.error; f++) {
        es_step(&es);
        es_poll(&es);
        if(!a->quiet && fm.last_log[0]) {
            printf("battle %d logged: %s\n", fm.battles, fm.last_log);
            fm.last_log[0] = 0;
        }
        if(es.battles != reported && es.battles % 10 == 0) {
            reported = es.battles;
            printf(
                "%u battles, %u exchanges, %u diverged\n", es.battles, es.exchanges, es.diverged);
            fflush(stdout);
        }
    }
    where(&gb);
    printf(
        "engine: %u battles, %u exchanges, %u diverged, %u game desyncs, %u hit unsupported paths, %d logged\n",
        es.battles,
        es.exchanges,
        es.diverged,
        es.game_desyncs,
        es.unsupported,
        fm.battles);
    if(es.first_divergence[0]) printf("first divergence:\n%s", es.first_divergence);
    bool music_ok = true;
    if(a->music) {
        music_watch_report(&watch);
        music_ok = music_watch_ok(&watch);
        printf("music check: %s\n", music_ok ? "ok" : "FAILED");
    }
    bool sprite_ok = true;
    if(a->sprite) {
        sprite_watch_report(&sprite);
        sprite_ok = sprite_watch_ok(&sprite);
        printf("sprite check: %s\n", sprite_ok ? "ok" : "FAILED");
    }
    bool ok = d.done && !fm.error && es.battles >= (uint32_t)a->battles && es.diverged == 0 &&
              music_ok && sprite_ok;
    es_free(&es);
    gbh_close(&gb);
    printf("%s\n", ok ? "ZERO DIVERGENCE" : "DIVERGED");
    return ok ? 0 : 1;
}

/* ---- device log replay ---- */

typedef struct {
    uint8_t rn_tx[PKB_RN_BLOCK_LEN], rn_rx[PKB_RN_BLOCK_LEN];
    uint8_t party_tx[PKB_PARTY_BLOCK_LEN], party_rx[PKB_PARTY_BLOCK_LEN];
    uint8_t patch_tx[PKB_PATCH_BLOCK_LEN], patch_rx[PKB_PATCH_BLOCK_LEN];
    unsigned have; /* bit per record type */
    uint8_t mine[256], gb[256];
    uint32_t crc[256]; /* engine state CRC per exchange (0 in smoke-test logs) */
    uint32_t exchanges;
    bool engine; /* written by the battle engine (note "engine") */
} DeviceLog;

static bool read_device_log(const char* path, DeviceLog* L) {
    static uint8_t buf[1 << 16];
    FILE* f = fopen(path, "rb");
    if(!f) return false;
    size_t len = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    memset(L, 0, sizeof(*L));
    if(len < PKB_LOG_HEADER_LEN || memcmp(buf, "PBL1", 4)) return false;
    for(size_t off = PKB_LOG_HEADER_LEN; off + 3 <= len;) {
        uint8_t type = buf[off];
        uint16_t n = (uint16_t)(buf[off + 1] | buf[off + 2] << 8);
        const uint8_t* p = buf + off + 3;
        if(off + 3 + n > len) return false;
        struct {
            uint8_t* dst;
            size_t size;
        } blocks[] = {
            [PkbLogRnTx] = {L->rn_tx, sizeof(L->rn_tx)},
            [PkbLogRnRx] = {L->rn_rx, sizeof(L->rn_rx)},
            [PkbLogPartyTx] = {L->party_tx, sizeof(L->party_tx)},
            [PkbLogPartyRx] = {L->party_rx, sizeof(L->party_rx)},
            [PkbLogPatchTx] = {L->patch_tx, sizeof(L->patch_tx)},
            [PkbLogPatchRx] = {L->patch_rx, sizeof(L->patch_rx)},
        };
        if(type >= PkbLogRnTx && type <= PkbLogPatchRx && n == blocks[type].size) {
            memcpy(blocks[type].dst, p, n);
            L->have |= 1u << type;
        } else if(type == PkbLogExchange && n == 8 && L->exchanges < sizeof(L->mine)) {
            L->mine[L->exchanges] = p[2];
            L->gb[L->exchanges] = p[3];
            L->crc[L->exchanges] =
                (uint32_t)(p[4] | p[5] << 8 | p[6] << 16 | (uint32_t)p[7] << 24);
            L->exchanges++;
        } else if(type == PkbLogNote && n == 6 && !memcmp(p, "engine", 6)) {
            L->engine = true;
        }
        off += 3 + n;
    }
    return true;
}

static int cmd_replay(Args* a, SymTable* syms) {
    static FlipperModel fm;
    static DeviceLog L;
    Gbh gb;
    GbDriver d;
    Trace t = {0};
    unsigned need = 0x7E; /* the six block records */
    if(!a->log || !read_device_log(a->log, &L) || (L.have & need) != need) {
        fprintf(stderr, "cannot read a complete log from %s\n", a->log ? a->log : "(none)");
        return 2;
    }
    if(!open_gb(&gb, a, syms, "gb")) return 1;
    drv_init(&d, &gb, 1, a->seed);
    DrvProfile* gp = &d.prof[DRV_AS_MASTER];
    gp->script = L.gb;
    gp->script_len = L.exchanges;
    gp->inject_rns = pkb_rn_block_parse(L.rn_rx, sizeof(L.rn_rx), gp->rns);
    gp->inject_party = pkb_party_block_parse(
        L.party_rx, sizeof(L.party_rx), L.patch_rx, sizeof(L.patch_rx), &gp->party);
    bool ok, engine_ok = true;
    if(L.engine) {
        /* the device's engine, with our choices from the log */
        static EngineSession es;
        fm_init_engine(&fm, a->seed, a->out);
        fm_set_blocks(&fm, L.rn_tx, L.party_tx, L.patch_tx);
        SessionBridge br;
        bridge_session_attach(&br, &gb, &fm.s, &t, 0x02);
        es_init(&es, &gb, &d, &fm, a->seed);
        es.script = L.mine;
        es.script_len = L.exchanges;
        for(unsigned f = 0; f < a->max_frames && !d.done && !fm.error; f++) {
            es_step(&es);
            es_poll(&es);
        }
        where(&gb);
        ok = d.done && !fm.error && es.battles == 1;
        uint32_t crc_bad = 0;
        for(uint32_t i = 0; i < L.exchanges && i < fm.ncrc; i++)
            crc_bad += L.crc[i] && L.crc[i] != fm.crc[i];
        printf(
            "engine: %u exchanges, %u CRCs differ from the device, mirror %s%s%s\n",
            es.exchanges,
            crc_bad,
            es.diverged ? "DIVERGED" : "matches",
            es.unsupported ? ", hit unsupported paths" : "",
            es.game_desyncs ? ", game desync" : "");
        if(es.first_divergence[0]) printf("first divergence:\n%s", es.first_divergence);
        engine_ok = crc_bad == 0 && es.diverged == 0;
        es_free(&es);
    } else {
        fm_init(&fm, a->seed, a->out);
        fm_set_blocks(&fm, L.rn_tx, L.party_tx, L.patch_tx);
        ok = run_session(a, &gb, &d, &fm, &t);
    }
    print_session(&fm, &t);

    /* compare with the device */
    uint32_t done = pbl_battle_exchanges_done(&fm.s);
    bool same = done == L.exchanges;
    printf("exchanges: device %u, replay %u\n", L.exchanges, done);
    for(uint32_t i = 0; i < done && i < 16; i++) {
        uint8_t mine, theirs;
        if(!pbl_battle_history(&fm.s, i, &mine, &theirs)) continue;
        bool eq = i < L.exchanges && mine == L.mine[i] && theirs == L.gb[i];
        same = same && eq;
        printf(
            "  %2u: flipper $%02X gb $%02X%s\n",
            i,
            0x80 | mine,
            0x80 | theirs,
            eq              ? "" :
            i < L.exchanges ? "  <- device differs" :
                              "  <- extra");
    }
    bool party_same = !memcmp(fm.s.party_rx, L.party_rx, sizeof(L.party_rx)) &&
                      !memcmp(fm.s.patch_rx, L.patch_rx, sizeof(L.patch_rx));
    bool rn_same = !memcmp(fm.s.rn_rx, L.rn_rx, sizeof(L.rn_rx));
    printf(
        "GB blocks as on the device: RNs %s, party %s\n",
        rn_same ? "yes" : "NO",
        party_same ? "yes" : "NO");
    ok = ok && engine_ok && same && party_same && rn_same && write_trace(a, &t);
    trace_free(&t);
    gbh_close(&gb);
    printf("%s\n", ok ? "REPLAY MATCHES" : "REPLAY DIFFERS");
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    Args a;
    SymTable syms;
    if(argc < 2) {
        fprintf(
            stderr, "usage: pkb_harness boot|fixture|run|twin|replay ... (see harness_main.c)\n");
        return 2;
    }
    parse(&a, argc, argv);
    if(!sym_load(&syms, a.sym)) return 1;
    int rc = 2;
    if(!strcmp(argv[1], "boot"))
        rc = cmd_boot(&a, &syms);
    else if(!strcmp(argv[1], "fixture"))
        rc = cmd_fixture(&a, &syms);
    else if(!strcmp(argv[1], "run"))
        rc = cmd_run(&a, &syms);
    else if(!strcmp(argv[1], "twin"))
        rc = cmd_twin(&a, &syms);
    else if(!strcmp(argv[1], "replay"))
        rc = cmd_replay(&a, &syms);
    else if(!strcmp(argv[1], "golden"))
        rc = cmd_golden(&a, &syms);
    else if(!strcmp(argv[1], "compare"))
        rc = cmd_compare(&a);
    else if(!strcmp(argv[1], "shadow"))
        rc = cmd_shadow(&a, &syms);
    else if(!strcmp(argv[1], "engine"))
        rc = cmd_engine(&a, &syms);
    else
        fprintf(stderr, "unknown command %s\n", argv[1]);
    sym_free(&syms);
    return rc;
}
