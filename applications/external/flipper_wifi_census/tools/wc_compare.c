// Host tool: cross two or more captures and print who is in which, past the device ceiling the
// Flipper has to keep. Accepts .wcen captures and raw-802.11 .pcap files. Build: `make tool`.
//
//   wc_compare monday.wcen friday.wcen > common.csv
//   wc_compare mon.pcap tue.pcap wed.pcap > seen.csv
//   wc_compare -n a.pcap b.pcap > networks.csv    the networks instead of the devices
//
// Matches are high confidence by construction: an identical stable MAC, or a directed SSID
// shared between non-AP devices with at least one randomized side.
//
// Nothing is excluded, ever. Your own gear travels with you and will be in every capture; so
// will router names common to a whole country. Both are reported with which captures they
// appear in, and you decide what that means - the tool has no business guessing which devices
// are yours.

#include "include/domain/wc_compare.h"
#include "tools/wc_load.h"

#define WC_COMPARE_MAX_FILES 12
#define WC_COMPARE_MAX_NETS 20000

// The distinct networks a capture's devices seek. Measured on real captures, these separate one
// place from another about as well as stable MACs do - and unlike a MAC they survive
// randomization, which is what most of a census is.
typedef struct {
    char (*ssid)[WC_SSID_MAX_LEN + 1];
    uint16_t count;
} NetSet;

static bool net_has(const NetSet *s, const char *ssid) {
    for (uint16_t i = 0; i < s->count; i++) {
        if (strncmp(s->ssid[i], ssid, WC_SSID_MAX_LEN) == 0) {
            return true;
        }
    }
    return false;
}

static void net_add(NetSet *s, const char *ssid) {
    if (ssid[0] == '\0' || s->count >= WC_COMPARE_MAX_NETS || net_has(s, ssid)) {
        return;
    }
    snprintf(s->ssid[s->count], WC_SSID_MAX_LEN + 1, "%s", ssid);
    s->count++;
}

static bool net_collect(NetSet *s, const WcCensus *c) {
    s->ssid = calloc(WC_COMPARE_MAX_NETS, WC_SSID_MAX_LEN + 1);
    if (!s->ssid) {
        return false;
    }
    s->count = 0;
    for (uint16_t i = 0; i < c->count; i++) {
        for (uint8_t j = 0; j < c->devices[i].ssid_count; j++) {
            net_add(s, c->devices[i].ssids[j]);
        }
    }
    return true;
}

static uint16_t net_common(const NetSet *a, const NetSet *b) {
    uint16_t n = 0;
    for (uint16_t i = 0; i < a->count; i++) {
        if (net_has(b, a->ssid[i])) {
            n++;
        }
    }
    return n;
}

int main(int argc, char **argv) {
    bool networks_out = false;
    int first = 1;
    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        networks_out = true;
        first = 2;
    }
    const int n = argc - first;
    if (n < 2 || n > WC_COMPARE_MAX_FILES) {
        fprintf(stderr, "usage: %s [-n] a b [c ...] > seen.csv   (2 to %d captures)\n", argv[0],
                WC_COMPARE_MAX_FILES);
        return 2;
    }

    WcCensus *c = calloc((size_t)n, sizeof(WcCensus));
    NetSet *nets = calloc((size_t)n, sizeof(NetSet));
    if (!c || !nets) {
        return 1;
    }
    for (int i = 0; i < n; i++) {
        wc_census_init(&c[i]);
        wc_census_set_max(&c[i], WC_TOOL_MAX_DEVICES);
        if (!wc_tool_load_file(argv[i + first], &c[i])) {
            fprintf(stderr, "! %s is not a .wcen or a supported .pcap\n", argv[i + first]);
            return 1;
        }
        wc_tool_report_dropped(argv[i + first], &c[i]);
        if (!net_collect(&nets[i], &c[i])) {
            fprintf(stderr, "! out of memory collecting networks\n");
            return 1;
        }
        WcCensusStats s = wc_census_stats(&c[i]);
        fprintf(stderr, "%d: %-28s %5u devices (%u randomized, %u crossable), %u networks\n", i,
                argv[i + first], s.total, s.random_count, s.unique_stable, nets[i].count);
    }

    fputs("\n   in both", stderr);
    for (int b = 1; b < n; b++) {
        fprintf(stderr, "%6d", b);
    }
    fputc('\n', stderr);
    for (int a = 0; a < n - 1; a++) {
        fprintf(stderr, "%10d", a);
        for (int b = 1; b < n; b++) {
            if (b <= a) {
                fputs("      ", stderr);
                continue;
            }
            uint16_t both = 0;
            for (uint16_t i = 0; i < c[a].count; i++) {
                if (wc_compare_find(&c[b], &c[a].devices[i], NULL, NULL)) {
                    both++;
                }
            }
            fprintf(stderr, "%6u", both);
        }
        fputc('\n', stderr);
    }

    fputs("\n networks in common (of the smaller set)\n", stderr);
    fputs("          ", stderr);
    for (int b = 1; b < n; b++) {
        fprintf(stderr, "%8d", b);
    }
    fputc('\n', stderr);
    for (int a = 0; a < n - 1; a++) {
        fprintf(stderr, "%10d", a);
        for (int b = 1; b < n; b++) {
            if (b <= a) {
                fputs("        ", stderr);
                continue;
            }
            const uint16_t both = net_common(&nets[a], &nets[b]);
            const uint16_t smaller = nets[a].count < nets[b].count ? nets[a].count : nets[b].count;
            fprintf(stderr, "%5u%3.0f%%", both, smaller ? 100.0 * both / smaller : 0.0);
        }
        fputc('\n', stderr);
    }

    if (networks_out) {
        // A network sought in more than one capture, and where. Names common to a whole country
        // sit here beside the ones that mean something; which is which is yours to judge.
        printf("network,seen_in,captures\n");
        uint32_t repeats = 0;
        for (uint16_t i = 0; i < nets[0].count; i++) {
            char where[WC_COMPARE_MAX_FILES * 3 + 1];
            size_t w = (size_t)snprintf(where, sizeof(where), "0");
            uint16_t seen = 1;
            for (int b = 1; b < n; b++) {
                if (net_has(&nets[b], nets[0].ssid[i])) {
                    seen++;
                    w += (size_t)snprintf(where + w, sizeof(where) - w, " %d", b);
                }
            }
            if (seen < 2) {
                continue;
            }
            repeats++;
            printf("%s,%u,%s\n", nets[0].ssid[i], seen, where);
        }
        fprintf(stderr, "\n%u networks of capture 0 are sought again elsewhere\n", repeats);
        for (int i = 0; i < n; i++) {
            wc_census_free(&c[i]);
            free(nets[i].ssid);
        }
        free(c);
        free(nets);
        return 0;
    }

    // One row per device of the first capture that turns up in at least one other: the regulars
    // of a place, which is the question several captures are taken to answer.
    printf("mac,random,seen_in,captures\n");
    uint32_t regulars = 0, everywhere = 0;
    for (uint16_t i = 0; i < c[0].count; i++) {
        const WcSignature *d = &c[0].devices[i];
        char where[WC_COMPARE_MAX_FILES * 3 + 1];
        size_t w = 0;
        uint16_t seen = 1;
        w += (size_t)snprintf(where + w, sizeof(where) - w, "0");
        for (int b = 1; b < n; b++) {
            if (wc_compare_find(&c[b], d, NULL, NULL)) {
                seen++;
                w += (size_t)snprintf(where + w, sizeof(where) - w, " %d", b);
            }
        }
        if (seen < 2) {
            continue;
        }
        regulars++;
        if (seen == n) {
            everywhere++;
        }
        printf("%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%s\n", d->mac[0], d->mac[1], d->mac[2],
               d->mac[3], d->mac[4], d->mac[5], d->mac_random ? 1 : 0, seen, where);
    }
    fprintf(stderr, "\n%u devices of capture 0 turn up again; %u are in all %d\n", regulars,
            everywhere, n);

    for (int i = 0; i < n; i++) {
        wc_census_free(&c[i]);
        free(nets[i].ssid);
    }
    free(c);
    free(nets);
    return 0;
}
