// Host tool: turn a raw-802.11 (linktype 105) pcap of probe requests into a WiFi Census,
// printed as CSV on stdout. Lets a laptop without a Flipper build a census from a capture
// taken by Marauder, Kismet, airodump, etc. Build: `make tool` (see Makefile).
//
//   wc_import capture.pcap > census.csv

#include "tools/wc_load.h"

#include <string.h>

int main(int argc, char **argv) {
    const char *out_wcen = NULL;
    if (argc == 4 && strcmp(argv[1], "-o") == 0) {
        out_wcen = argv[2];
        argv[1] = argv[3];
    } else if (argc != 2) {
        fprintf(stderr, "usage: %s [-o out.wcen] capture.pcap > census.csv\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "empty file\n");
        return 1;
    }
    uint8_t *buf = malloc((size_t)size);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        fprintf(stderr, "read error\n");
        return 1;
    }
    fclose(f);

    WcCensus *c = malloc(sizeof(WcCensus));
    if (!c) {
        free(buf);
        return 1;
    }
    wc_census_init(c);
    wc_census_set_max(c, WC_TOOL_MAX_DEVICES); // off-device: no Flipper ceiling applies
    if (!wc_pcap_read(buf, (size_t)size, wc_tool_on_frame, c)) {
        fprintf(stderr, "not a supported pcap (need classic libpcap, linktype 105 IEEE802.11)\n");
        free(buf);
        free(c);
        return 1;
    }

    if (out_wcen) {
        WcCaptureMeta wm;
        memset(&wm, 0, sizeof(wm));
        snprintf(wm.label, sizeof(wm.label), "%s", out_wcen);
        if (!wc_tool_write_wcen(out_wcen, &wm, c)) {
            fprintf(stderr, "! could not write %s\n", out_wcen);
            return 1;
        }
        fprintf(stderr, "= wrote %s (%u devices)\n", out_wcen, c->count);
    }

    wc_tool_report_dropped("import", c);
    WcCensusStats s = wc_census_stats(c);
    fprintf(stderr, "devices: %u (stable %u, random %u), networks sought: %u\n", s.total,
            s.unique_stable, s.random_count, s.networks);

    size_t csv_len = wc_capture_to_csv(NULL, 0, &(WcCaptureMeta){0}, c);
    char *csv = malloc(csv_len + 1);
    if (!csv) {
        fprintf(stderr, "! out of memory rendering the CSV\n");
        return 1;
    }
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "%s", argv[1]);
    wc_capture_to_csv(csv, csv_len + 1, &meta, c);
    fputs(csv, stdout);

    free(csv);
    free(buf);
    free(c);
    return 0;
}
