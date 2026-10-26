#include "include/domain/wc_timefmt.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_known_epochs(void) {
    char buf[32];
    // 2021-01-01 00:00:00 UTC = 1609459200
    wc_default_capture_name(1609459200u, buf, sizeof(buf));
    assert(strcmp(buf, "cap_20210101_0000") == 0);
    // 2026-09-19 13:45:00 UTC = 1789825500
    wc_default_capture_name(1789825500u, buf, sizeof(buf));
    assert(strcmp(buf, "cap_20260919_1345") == 0);
    // epoch 0 = 1970-01-01 00:00
    wc_default_capture_name(0u, buf, sizeof(buf));
    assert(strcmp(buf, "cap_19700101_0000") == 0);
}

static void test_filesystem_safe(void) {
    char buf[32];
    wc_default_capture_name(1789825500u, buf, sizeof(buf));
    // No characters a FAT filename dislikes.
    for (const char *p = buf; *p; p++) {
        assert(*p != '/' && *p != '\\' && *p != ':' && *p != ' ');
    }
}

int main(void) {
    test_known_epochs();
    test_filesystem_safe();
    printf("test_timefmt: OK\n");
    return 0;
}
