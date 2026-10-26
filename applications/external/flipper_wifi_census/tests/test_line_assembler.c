#include "include/domain/wc_line_assembler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINES 8
typedef struct {
    char lines[MAX_LINES][WC_LINE_MAX];
    int count;
} Collector;

static void collect(void *ctx, const char *line, size_t len) {
    Collector *c = ctx;
    if (c->count < MAX_LINES) {
        memcpy(c->lines[c->count], line, len);
        c->lines[c->count][len] = '\0';
        c->count++;
    }
}

static void feed_str(WcLineAsm *a, const char *s, Collector *c) {
    wc_lineasm_feed(a, (const uint8_t *)s, strlen(s), collect, c);
}

static void test_splits_on_newline_and_strips_cr(void) {
    WcLineAsm a;
    wc_lineasm_init(&a);
    Collector c = {0};
    feed_str(&a, "one\r\ntwo\n", &c);
    assert(c.count == 2);
    assert(strcmp(c.lines[0], "one") == 0);
    assert(strcmp(c.lines[1], "two") == 0);
}

static void test_reassembles_across_chunks(void) {
    WcLineAsm a;
    wc_lineasm_init(&a);
    Collector c = {0};
    feed_str(&a, "hel", &c);
    feed_str(&a, "lo wor", &c);
    feed_str(&a, "ld\n", &c);
    assert(c.count == 1);
    assert(strcmp(c.lines[0], "hello world") == 0);
}

static void test_skips_empty_lines(void) {
    WcLineAsm a;
    wc_lineasm_init(&a);
    Collector c = {0};
    feed_str(&a, "\n\nx\n\n", &c);
    assert(c.count == 1);
    assert(strcmp(c.lines[0], "x") == 0);
}

static void test_overlong_line_dropped_then_resyncs(void) {
    WcLineAsm a;
    wc_lineasm_init(&a);
    Collector c = {0};
    char big[WC_LINE_MAX + 50];
    memset(big, 'A', sizeof(big));
    big[sizeof(big) - 1] = '\0';
    feed_str(&a, big, &c); // no newline yet: buffered/overflowed, nothing emitted
    assert(c.count == 0);
    feed_str(&a, "\ngood\n", &c); // newline drops the overlong line, then a clean one
    assert(c.count == 1);
    assert(strcmp(c.lines[0], "good") == 0);
}

int main(void) {
    test_splits_on_newline_and_strips_cr();
    test_reassembles_across_chunks();
    test_skips_empty_lines();
    test_overlong_line_dropped_then_resyncs();
    printf("test_line_assembler: OK\n");
    return 0;
}
