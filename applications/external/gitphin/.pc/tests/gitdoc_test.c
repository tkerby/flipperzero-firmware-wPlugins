/* Host test of the pure helpers of the document store (hub_gitdocutil.c).
 *   gitdoc_test crc <file>     prints the CRC-32 as 8 hex digits (run_write_test.sh
 *                              compares it with python zlib.crc32)
 *   gitdoc_test                run the built-in checks */
#include "../../flipper_app/hub_gitdocutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) \
    do { \
        if(!(c)) { \
            fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #c); \
            fails++; \
        } \
    } while(0)

static void keys(void) {
    const char* good[] = {
        "issues/12.json", "wiki/Home.md", "meta.json", "a", "a/b/c/d.txt", "A_b-c.d", "x.y.z",
        "releases/1/asset-1.0.tar.gz"};
    for(size_t i = 0; i < sizeof(good) / sizeof(*good); i++) {
        if(!hub_gitdoc_key_ok(good[i])) fprintf(stderr, "good key rejected: %s\n", good[i]);
        CHECK(hub_gitdoc_key_ok(good[i]));
    }
    const char* bad[] = {
        "", "/", "/a", "a/", "a//b", "..", ".", ".a", "a/.b", "a/../b", "../a", "a..b", "a b",
        "a\\b", "a/b c", "a%2fb", "a:b", "ä", "a/\x01", "x.part", "a/b.part", "issues/",
        "a/./b"};
    for(size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        if(hub_gitdoc_key_ok(bad[i])) fprintf(stderr, "bad key accepted: %s\n", bad[i]);
        CHECK(!hub_gitdoc_key_ok(bad[i]));
    }
    char k[400];
    memset(k, 'a', 160);
    k[160] = '\0';
    CHECK(hub_gitdoc_key_ok(k));
    k[160] = 'a';
    k[161] = '\0';
    CHECK(!hub_gitdoc_key_ok(k));
    /* 160 chars with slashes */
    memset(k, 'a', 160);
    k[3] = '/';
    k[160] = '\0';
    CHECK(hub_gitdoc_key_ok(k));

    CHECK(hub_gitdoc_prefix_ok(""));
    CHECK(hub_gitdoc_prefix_ok("issues"));
    CHECK(hub_gitdoc_prefix_ok("issues/"));
    CHECK(hub_gitdoc_prefix_ok("a/b/"));
    CHECK(!hub_gitdoc_prefix_ok("/"));
    CHECK(!hub_gitdoc_prefix_ok("a//"));
    CHECK(!hub_gitdoc_prefix_ok("../x"));
    CHECK(!hub_gitdoc_prefix_ok(".x/"));
}

static void crc(void) {
    CHECK(hub_crc32(0, "123456789", 9) == 0xcbf43926u);
    CHECK(hub_crc32(0, "", 0) == 0);
    /* incremental equals one shot */
    uint32_t c = hub_crc32(0, "1234", 4);
    c = hub_crc32(c, "56789", 5);
    CHECK(c == 0xcbf43926u);
    char e[11];
    hub_gitdoc_etag(0xcbf43926u, e);
    CHECK(strcmp(e, "\"cbf43926\"") == 0);
    hub_gitdoc_etag(0x0000002au, e);
    CHECK(strcmp(e, "\"0000002a\"") == 0);
    hub_gitdoc_etag(0, e);
    CHECK(strcmp(e, "\"00000000\"") == 0);
}

static void preconds(void) {
    uint32_t cur = 0xcbf43926u;
    /* create */
    CHECK(hub_gitdoc_precond(NULL, "*", false, false, 0) == 0);
    CHECK(hub_gitdoc_precond(NULL, "*", false, true, cur) == 412);
    CHECK(hub_gitdoc_precond(NULL, " * ", false, false, 0) == 0);
    CHECK(hub_gitdoc_precond(NULL, "\"abc\"", false, false, 0) == 400);
    /* replace */
    CHECK(hub_gitdoc_precond("\"cbf43926\"", NULL, false, true, cur) == 0);
    CHECK(hub_gitdoc_precond("\"CBF43926\"", NULL, false, true, cur) == 0);
    CHECK(hub_gitdoc_precond("cbf43926", NULL, false, true, cur) == 0);
    CHECK(hub_gitdoc_precond("\"cbf43927\"", NULL, false, true, cur) == 412);
    CHECK(hub_gitdoc_precond("\"cbf43926\"", NULL, false, false, 0) == 412);
    CHECK(hub_gitdoc_precond("*", NULL, false, true, cur) == 0);
    CHECK(hub_gitdoc_precond("*", NULL, false, false, 0) == 412);
    CHECK(hub_gitdoc_precond("\"00000001\", \"cbf43926\"", NULL, false, true, cur) == 0);
    CHECK(hub_gitdoc_precond("\"00000001\",\"00000002\"", NULL, false, true, cur) == 412);
    CHECK(hub_gitdoc_precond("W/\"cbf43926\"", NULL, false, true, cur) == 412);
    CHECK(hub_gitdoc_precond("\"cbf4392\"", NULL, false, true, cur) == 412);
    CHECK(hub_gitdoc_precond("\"cbf439260\"", NULL, false, true, cur) == 412);
    CHECK(hub_gitdoc_precond("", NULL, false, true, cur) == 412);
    /* an empty document has crc 0 and still matches */
    CHECK(hub_gitdoc_precond("\"00000000\"", NULL, false, true, 0) == 0);
    /* none */
    CHECK(hub_gitdoc_precond(NULL, NULL, false, true, cur) == 400);
    CHECK(hub_gitdoc_precond(NULL, NULL, true, true, cur) == 400);
    /* delete: only If-Match */
    CHECK(hub_gitdoc_precond("\"cbf43926\"", NULL, true, true, cur) == 0);
    CHECK(hub_gitdoc_precond("\"cbf43927\"", NULL, true, true, cur) == 412);
    CHECK(hub_gitdoc_precond(NULL, "*", true, true, cur) == 400);
}

static void headers(void) {
    CHECK(strcmp(hub_gitdoc_hdr("If-Match:  \"x\"", "if-match"), "\"x\"") == 0);
    CHECK(strcmp(hub_gitdoc_hdr("IF-NONE-MATCH:*", "if-none-match"), "*") == 0);
    CHECK(hub_gitdoc_hdr("If-Match-X: 1", "if-match") == NULL);
    CHECK(hub_gitdoc_hdr("If: (<x>)", "if-match") == NULL);
    CHECK(hub_gitdoc_hdr("If-Match", "if-match") == NULL);

    /* the head as hub_http.c leaves it: NUL where "\r" was, "\n" stays */
    const char head[] = "PUT /api/git/doc?name=x HTTP/1.1\0\nHost: a\0\nContent-Length: 5\0\n"
                        "if-match: \"cbf43926\"\0\nIf-None-Match: *\0";
    const char *im = NULL, *inm = NULL;
    hub_gitdoc_scan_head(head, sizeof(head) - 1, &im, &inm);
    CHECK(im && strcmp(im, "\"cbf43926\"") == 0);
    CHECK(inm && strcmp(inm, "*") == 0);
    im = inm = NULL;
    const char head2[] = "GET / HTTP/1.1\0\nHost: a\0";
    hub_gitdoc_scan_head(head2, sizeof(head2) - 1, &im, &inm);
    CHECK(!im && !inm);
}

int main(int argc, char** argv) {
    if(argc == 3 && strcmp(argv[1], "crc") == 0) {
        FILE* f = fopen(argv[2], "rb");
        if(!f) return 2;
        uint8_t buf[777];
        uint32_t c = 0;
        size_t n;
        while((n = fread(buf, 1, sizeof(buf), f)) > 0) c = hub_crc32(c, buf, n);
        fclose(f);
        printf("%08x\n", c);
        return 0;
    }
    keys();
    crc();
    preconds();
    headers();
    printf(fails ? "gitdoc: %d FAILED\n" : "gitdoc: all checks passed\n", fails);
    return fails != 0;
}
