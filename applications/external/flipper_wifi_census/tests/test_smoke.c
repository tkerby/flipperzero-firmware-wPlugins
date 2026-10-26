#include "include/domain/wc_version_info.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_version_is_non_empty(void) {
    const char *v = wc_version();
    assert(v != NULL);
    assert(strlen(v) > 0);
}

int main(void) {
    test_version_is_non_empty();
    printf("test_smoke: OK\n");
    return 0;
}
