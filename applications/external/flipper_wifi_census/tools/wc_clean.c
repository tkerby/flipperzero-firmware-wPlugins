// Host tool: undo MAC rotation in a long capture, and show what that costs before you believe
// it. A phone that rotates its MAC is counted again each time, which is most of the growth in a
// long scan; but many keep counting their 802.11 sequence number straight through the change,
// so two MACs whose counters meet at the moment one stops and the other starts are one phone.
//
//   wc_clean capture.pcap                       # what each threshold would do
//   wc_clean -w 10 -s 4 -o clean.wcen cap.pcap  # write the cleaned census
//
// Nothing is taken on trust: stable MACs are certain identities, so the same rule applied to
// them can only ever be wrong. That count is this capture's own error rate, printed beside the
// result, and it rises sharply once the window opens past a few seconds.

#include "tools/wc_load.h"

typedef struct {
    uint8_t mac[6];
    bool random;
    double first_t, last_t;
    uint16_t first_seq, last_seq;
    int root;
} MacSpan;

static MacSpan *g_span;
static int g_spans, g_span_cap, g_untracked;

// Grown rather than capped: a fixed ceiling here would quietly stop linking once a long capture
// filled it, and hand back a worse answer with the same confident face.
static bool span_room(void) {
    if (g_spans < g_span_cap) {
        return true;
    }
    int cap = g_span_cap ? g_span_cap * 2 : 1024;
    MacSpan *grown = realloc(g_span, (size_t)cap * sizeof(*g_span));
    if (!grown) {
        return false;
    }
    g_span = grown;
    g_span_cap = cap;
    return true;
}

static int span_of(const uint8_t mac[6]) {
    for (int i = 0; i < g_spans; i++) {
        if (memcmp(g_span[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

static int root_of(int i) {
    while (g_span[i].root != i) {
        i = g_span[i].root;
    }
    return i;
}

// Walk the pcap records directly: the per-frame timestamp and sequence number are what the
// linking needs, and neither survives into a census.
static bool scan_spans(const uint8_t *b, size_t sz) {
    if (!wc_tool_is_classic_le_pcap(b, sz)) {
        return false;
    }
    double t0 = -1;
    for (size_t o = 24; o + 16 <= sz;) {
        uint32_t ts = b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | ((uint32_t)b[o + 3] << 24);
        uint32_t us = b[o + 4] | (b[o + 5] << 8) | (b[o + 6] << 16) | ((uint32_t)b[o + 7] << 24);
        uint32_t incl =
            b[o + 8] | (b[o + 9] << 8) | (b[o + 10] << 16) | ((uint32_t)b[o + 11] << 24);
        if (o + 16 + incl > sz) {
            break;
        }
        WcObservation ob;
        if (wc_parse_probe_frame(b + o + 16, incl, &ob)) {
            double t = (double)ts + (double)us / 1e6;
            if (t0 < 0) {
                t0 = t;
            }
            t -= t0;
            int i = span_of(ob.mac);
            if (i < 0 && !span_room()) {
                g_untracked++; // out of memory: say so rather than link less and stay quiet
            } else if (i < 0) {
                i = g_spans++;
                memcpy(g_span[i].mac, ob.mac, 6);
                g_span[i].random = ob.mac_random;
                g_span[i].first_t = t;
                g_span[i].first_seq = ob.seq;
                g_span[i].root = i;
            }
            if (i >= 0) {
                g_span[i].last_t = t;
                g_span[i].last_seq = ob.seq;
            }
        }
        o += 16 + incl;
    }
    return g_spans > 0;
}

static int by_first_t(const void *x, const void *y) {
    double a = ((const MacSpan *)x)->first_t, b = ((const MacSpan *)y)->first_t;
    return (a > b) - (a < b);
}

// Link spans of `want_random` kind whose counters meet. Returns how many links it made.
static int link_spans(double window, int seq_gap, bool want_random, bool apply) {
    int links = 0;
    for (int b = 0; b < g_spans; b++) {
        if (g_span[b].random != want_random) {
            continue;
        }
        int best = -1;
        double best_dt = window + 1;
        for (int a = 0; a < g_spans; a++) {
            if (a == b || g_span[a].random != want_random) {
                continue;
            }
            double dt = g_span[b].first_t - g_span[a].last_t;
            int ds = (int)g_span[b].first_seq - (int)g_span[a].last_seq;
            if (dt >= 0 && dt <= window && ds > 0 && ds <= seq_gap && dt < best_dt) {
                best = a;
                best_dt = dt;
            }
        }
        if (best >= 0 && root_of(best) != root_of(b)) {
            links++;
            if (apply) {
                g_span[root_of(b)].root = root_of(best);
            }
        }
    }
    return links;
}

static void reset_roots(void) {
    for (int i = 0; i < g_spans; i++) {
        g_span[i].root = i;
    }
}

typedef struct {
    WcCensus *raw;
    WcCensus *clean;
} BuildCtx;

static void on_frame(void *ctx, const uint8_t *frame, size_t len) {
    WcObservation o;
    if (!wc_parse_probe_frame(frame, len, &o)) {
        return;
    }
    BuildCtx *b = ctx;
    wc_census_observe(b->raw, &o, 0);
    int i = span_of(o.mac);
    if (i >= 0) {
        memcpy(o.mac, g_span[root_of(i)].mac, 6); // rotations answer to the first MAC seen
    }
    wc_census_observe(b->clean, &o, 0);
}

int main(int argc, char **argv) {
    double window = 10;
    int seq_gap = 4;
    const char *out = NULL, *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) {
            window = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-s") && i + 1 < argc) {
            seq_gap = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            out = argv[++i];
        } else {
            path = argv[i];
        }
    }
    if (!path) {
        fprintf(stderr, "usage: %s [-w seconds] [-s seqgap] [-o clean.wcen] capture.pcap\n",
                argv[0]);
        return 2;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 1;
    }
    fclose(f);

    if (!scan_spans(buf, (size_t)sz)) {
        fprintf(stderr,
                "%s: not a classic little-endian libpcap of linktype 105, or no probe "
                "requests in it\n",
                path);
        return 1;
    }
    qsort(g_span, (size_t)g_spans, sizeof(MacSpan), by_first_t);
    int randoms = 0, stables = 0;
    for (int i = 0; i < g_spans; i++) {
        g_span[i].root = i;
        if (g_span[i].random) {
            randoms++;
        } else {
            stables++;
        }
    }
    if (g_untracked > 0) {
        fprintf(stderr, "! out of memory: %d MACs not tracked, so they cannot be linked\n",
                g_untracked);
    }
    fprintf(stderr, "%s: %d MACs (%d randomized, %d stable)\n\n", path, g_spans, randoms, stables);

    if (!out) {
        fprintf(stderr, "%8s %8s %10s %12s %s\n", "window", "seq gap", "links", "false pos",
                "(on stable MACs, which cannot rotate)");
        const double ws[] = {5, 10, 30, 60, 120};
        const int gs[] = {2, 4, 8, 16, 32};
        for (int k = 0; k < 5; k++) {
            reset_roots();
            int l = link_spans(ws[k], gs[k], true, false);
            reset_roots();
            int fp = link_spans(ws[k], gs[k], false, false);
            fprintf(stderr, "%7.0fs %8d %10d %12d %s\n", ws[k], gs[k], l, fp,
                    stables ? "" : "(no stable MACs to check against)");
        }
        fprintf(stderr, "\npick one with -w and -s, and -o to write the cleaned capture\n");
        free(buf);
        return 0;
    }

    reset_roots();
    int fp = link_spans(window, seq_gap, false, false);
    reset_roots();
    int links = link_spans(window, seq_gap, true, true);

    WcCensus *raw = malloc(sizeof(WcCensus));
    WcCensus *clean = malloc(sizeof(WcCensus));
    if (!raw || !clean) {
        return 1;
    }
    wc_census_init(raw);
    wc_census_set_max(raw, WC_TOOL_MAX_DEVICES);
    wc_census_init(clean);
    wc_census_set_max(clean, WC_TOOL_MAX_DEVICES);
    BuildCtx bc = {.raw = raw, .clean = clean};
    if (!wc_pcap_read(buf, (size_t)sz, on_frame, &bc)) {
        fprintf(stderr, "! %s is not a classic libpcap of linktype 105 - nothing was read\n", path);
        return 1;
    }

    fprintf(stderr, "at %.0fs / %d: %d links, %d of them provably wrong on stable MACs\n", window,
            seq_gap, links, fp);
    wc_tool_report_dropped("raw", raw);
    wc_tool_report_dropped("cleaned", clean);
    fprintf(stderr, "devices: %u raw -> %u cleaned\n", raw->count, clean->count);

    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "%s", out);
    if (!wc_tool_write_wcen(out, &meta, clean)) {
        fprintf(stderr, "! could not write %s\n", out);
        return 1;
    }
    fprintf(stderr, "= wrote %s\n", out);

    wc_census_free(raw);
    wc_census_free(clean);
    free(raw);
    free(clean);
    free(buf);
    return 0;
}
