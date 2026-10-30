/* Host-side unit tests for bleepsentry_logic.c.
 *
 * Compiles and runs on a normal machine with plain gcc -- no Flipper SDK,
 * no device required. Proves the pure math/formatting is correct; it does
 * NOT exercise the GUI, radio, or timer code, which only runs on-device.
 *
 * Build & run:
 *   gcc -std=c11 -Wall -Wextra -I.. test_logic.c ../bleepsentry_logic.c -o test_logic && ./test_logic
 */
#include "../bleepsentry_logic.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if(!(cond)) {                                              \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                            \
        }                                                          \
    } while(0)

static void test_threshold(void) {
    CHECK(fabsf(bs_compute_threshold(-80.0f, 12.0f) - (-68.0f)) < 0.001f);
    CHECK(fabsf(bs_compute_threshold(-100.0f, 0.0f) - (-100.0f)) < 0.001f);
}

static void test_should_alert(void) {
    CHECK(bs_should_alert(-50.0f, -60.0f) == true); /* rssi above threshold */
    CHECK(bs_should_alert(-70.0f, -60.0f) == false); /* rssi below threshold */
    CHECK(bs_should_alert(-60.0f, -60.0f) == false); /* exactly equal: not an alert */
}

static void test_index_wrap(void) {
    CHECK(bs_next_index(0, 4) == 1);
    CHECK(bs_next_index(3, 4) == 0); /* wraps forward */
    CHECK(bs_prev_index(0, 4) == 3); /* wraps backward */
    CHECK(bs_prev_index(2, 4) == 1);
    CHECK(bs_next_index(0, 1) == 0); /* single-element list is stable */
}

static void test_alert_duration(void) {
    CHECK(bs_alert_duration(100, 105) == 5);
    CHECK(bs_alert_duration(100, 100) == 0);
    CHECK(bs_alert_duration(105, 100) == 0); /* end before start: clamp, don't underflow */
}

static void test_format_log_line(void) {
    char buf[128];
    int len = bs_format_log_line(buf, sizeof(buf), 1700000000, 433920000, -80, -68, -40, 7);
    CHECK(len > 0);
    CHECK(strcmp(buf, "1700000000,433920000,-80,-68,-40,7\n") == 0);
}

static void test_format_log_line_truncation(void) {
    char buf[8];
    int len = bs_format_log_line(buf, sizeof(buf), 1700000000, 433920000, -80, -68, -40, 7);
    /* snprintf still reports the untruncated length; buf itself must stay
     * null-terminated within the given size, never overflow it. */
    CHECK(len > (int)sizeof(buf) - 1);
    CHECK(strlen(buf) < sizeof(buf));
}

int main(void) {
    test_threshold();
    test_should_alert();
    test_index_wrap();
    test_alert_duration();
    test_format_log_line();
    test_format_log_line_truncation();

    if(failures == 0) {
        printf("All tests passed.\n");
        return 0;
    } else {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }
}
