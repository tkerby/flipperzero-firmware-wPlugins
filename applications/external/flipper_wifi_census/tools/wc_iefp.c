// Host tool: does the IE fingerprint tell devices apart, or only models? And how much does a
// long capture inflate the count? Build: `make tool`.
//
//   wc_iefp capture.pcap
//
// Two stable MACs sharing a fingerprint are provably two devices, which is what makes the
// false-positive rate measurable rather than guessed.

#include "tools/wc_load.h"

#include <string.h>

static void on_frame(void *ctx, const uint8_t *frame, size_t len) {
    WcObservation o;
    if (wc_parse_probe_frame(frame, len, &o)) {
        wc_census_observe(ctx, &o, 0);
    }
}

// Seconds between the first and last record of the pcap.
static uint32_t pcap_span(const uint8_t *buf, size_t sz) {
    if (!wc_tool_is_classic_le_pcap(buf, sz)) {
        return 0;
    }
    uint32_t t0 = 0, last = 0;
    for (size_t o = 24; o + 16 <= sz;) {
        uint32_t ts =
            buf[o] | (buf[o + 1] << 8) | (buf[o + 2] << 16) | ((uint32_t)buf[o + 3] << 24);
        uint32_t incl =
            buf[o + 8] | (buf[o + 9] << 8) | (buf[o + 10] << 16) | ((uint32_t)buf[o + 11] << 24);
        if (o + 16 + incl > sz) {
            break;
        }
        if (t0 == 0) {
            t0 = ts;
        }
        last = ts;
        o += 16 + incl;
    }
    return last - t0;
}

// Fill a census from the frames whose timestamp is within `window` seconds of the first one
// (0 = the whole file). Walks the pcap records directly for the timestamps.
static void read_window(const uint8_t *buf, size_t sz, uint32_t window, WcCensus *c) {
    uint32_t t0 = 0;
    for (size_t o = 24; o + 16 <= sz;) {
        uint32_t ts =
            buf[o] | (buf[o + 1] << 8) | (buf[o + 2] << 16) | ((uint32_t)buf[o + 3] << 24);
        uint32_t incl =
            buf[o + 8] | (buf[o + 9] << 8) | (buf[o + 10] << 16) | ((uint32_t)buf[o + 11] << 24);
        if (o + 16 + incl > sz) {
            break;
        }
        if (t0 == 0) {
            t0 = ts;
        }
        if (window == 0 || ts - t0 <= window) {
            WcObservation ob;
            if (wc_parse_probe_frame(buf + o + 16, incl, &ob)) {
                wc_census_observe(c, &ob, 0);
            }
        }
        o += 16 + incl;
    }
}

static uint16_t distinct_random_fingerprints(const WcCensus *c) {
    uint16_t distinct = 0;
    for (uint16_t i = 0; i < c->count; i++) {
        if (!c->devices[i].mac_random) {
            continue;
        }
        bool seen = false;
        for (uint16_t j = 0; j < i && !seen; j++) {
            seen = c->devices[j].mac_random && c->devices[j].ie_hash == c->devices[i].ie_hash;
        }
        if (!seen) {
            distinct++;
        }
    }
    return distinct;
}

static WcCensus *census_alloc(void) {
    WcCensus *c = malloc(sizeof(WcCensus));
    if (c) {
        wc_census_init(c);
        wc_census_set_max(c, WC_TOOL_MAX_DEVICES);
    }
    return c;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s capture.pcap\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
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

    WcCensus *c = census_alloc();
    if (!c || !wc_pcap_read(buf, (size_t)sz, on_frame, c)) {
        fprintf(stderr, "%s: not a linktype-105 (raw 802.11) pcap\n", argv[1]);
        return 1;
    }

    wc_tool_report_dropped(argv[1], c);
    printf("=== %s: %u devices ===\n", argv[1], c->count);

    // 1. Does the fingerprint identify a device?
    uint16_t clusters = 0, covered = 0, biggest = 0, bad_fp = 0, wrongly_merged = 0;
    uint8_t *seen = calloc(c->count ? c->count : 1, 1);
    if (!seen) {
        fprintf(stderr, "! out of memory\n");
        return 1;
    }
    for (uint16_t i = 0; i < c->count; i++) {
        if (seen[i] || c->devices[i].ie_hash == 0) {
            continue;
        }
        uint16_t members = 0, stable = 0;
        for (uint16_t j = i; j < c->count; j++) {
            if (c->devices[j].ie_hash != c->devices[i].ie_hash) {
                continue;
            }
            seen[j] = 1;
            members++;
            if (!c->devices[j].mac_random) {
                stable++;
            }
        }
        if (members < 2) {
            continue;
        }
        clusters++;
        covered += members;
        if (members > biggest) {
            biggest = members;
        }
        if (stable >= 2) {
            bad_fp++;
            wrongly_merged += (uint16_t)(stable - 1);
        }
    }
    free(seen);
    printf("fingerprints shared by 2+ devices: %u (covering %u, biggest cluster %u)\n", clusters,
           covered, biggest);
    printf("PROVEN FALSE POSITIVES: %u fingerprints hold 2+ different STABLE MACs\n", bad_fp);
    printf("  -> %u certain devices would be merged away (%.1f%%) if the fingerprint matched\n",
           wrongly_merged, c->count ? 100.0 * wrongly_merged / c->count : 0.0);

    uint16_t lo = 0, hi = 0;
    if (wc_census_phone_bound(c, &lo, &hi)) {
        printf("phone bound: %u-%u (the app reports %u devices)\n", lo, hi, c->count);
    }
    wc_census_free(c);
    free(c);

    // 2. How much does duration inflate the count? Same file, growing windows.
    const uint32_t span = pcap_span(buf, (size_t)sz);
    if (span == 0) {
        fprintf(stderr, "! the capture spans no time: the window table below says nothing\n");
    }
    printf("\ncapture span: %us\nwindow  devices  stable  random  distinct fp  random/fp\n", span);
    for (uint32_t w = 60; w <= 3600; w += (w < 300 ? 60 : 120)) {
        WcCensus *win = census_alloc();
        if (!win) {
            break;
        }
        read_window(buf, (size_t)sz, w, win);
        WcCensusStats s = wc_census_stats(win);
        uint16_t fps = distinct_random_fingerprints(win);
        printf("%4us %8u %7u %7u %12u %9.2f\n", w, s.total, s.unique_stable, s.random_count, fps,
               fps ? (double)s.random_count / fps : 0.0);
        wc_census_free(win);
        free(win);
        if (w >= span) {
            break; // past the end of the capture: every later row would repeat this one
        }
    }
    free(buf);
    return 0;
}
