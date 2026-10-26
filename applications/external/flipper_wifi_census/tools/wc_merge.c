// Host tool: merge many captures into one census, printed as CSV on stdout. The Flipper caps a
// scan or merge at WC_CENSUS_MAX_DEVICES for RAM; here there is no such limit, so a big venue
// can be captured in pieces and combined. Accepts .wcen and raw-802.11 .pcap, mixed.
//
//   wc_merge part1.wcen scan.pcap ... > all.csv
//   wc_merge -o all.wcen part1.wcen ...     also writes a capture, which merges again later

#include "tools/wc_load.h"

int main(int argc, char **argv) {
    const char *out_wcen = NULL;
    int first = 1;
    if (argc > 2 && strcmp(argv[1], "-o") == 0) {
        out_wcen = argv[2];
        first = 3;
    }
    if (first >= argc) {
        fprintf(stderr, "usage: %s [-o out.wcen] file1 [file2|scan.pcap ...] > all.csv\n", argv[0]);
        return 2;
    }
    WcCensus *acc = malloc(sizeof(WcCensus));
    if (!acc) {
        return 1;
    }
    wc_census_init(acc);
    wc_census_set_max(acc, WC_TOOL_MAX_DEVICES);

    for (int i = first; i < argc; i++) {
        WcCensus part;
        wc_census_init(&part);
        wc_census_set_max(&part, WC_TOOL_MAX_DEVICES);
        if (wc_tool_load_file(argv[i], &part)) {
            wc_census_merge(acc, &part);
            fprintf(stderr, "+ %s (%u devices)\n", argv[i], part.count);
        } else {
            fprintf(stderr, "! skipped %s (not a .wcen or supported .pcap)\n", argv[i]);
        }
        wc_census_free(&part);
    }

    wc_tool_report_dropped("merge", acc);
    WcCensusStats s = wc_census_stats(acc);
    fprintf(stderr, "= merged: %u devices (stable %u, random %u), networks sought: %u\n", s.total,
            s.unique_stable, s.random_count, s.networks);

    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "%s", out_wcen ? out_wcen : "merged");
    if (out_wcen) {
        if (!wc_tool_write_wcen(out_wcen, &meta, acc)) {
            fprintf(stderr, "! could not write %s\n", out_wcen);
            return 1;
        }
        fprintf(stderr, "= wrote %s (%u devices)\n", out_wcen, acc->count);
    }

    size_t csv_len = wc_capture_to_csv(NULL, 0, &(WcCaptureMeta){0}, acc);
    char *csv = malloc(csv_len + 1);
    if (csv) {
        wc_capture_to_csv(csv, csv_len + 1, &(WcCaptureMeta){0}, acc);
        fputs(csv, stdout);
        free(csv);
    }
    wc_census_free(acc);
    free(acc);
    return 0;
}
