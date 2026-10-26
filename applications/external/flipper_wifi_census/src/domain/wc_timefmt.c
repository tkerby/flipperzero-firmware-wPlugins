#include "include/domain/wc_timefmt.h"

#include <stdio.h>

// Civil date from a day count since the Unix epoch (Howard Hinnant's algorithm; valid for
// the whole range we care about). Fills year/month/day.
static void civil_from_days(int64_t z, int *y, unsigned *m, unsigned *d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);                          // [0, 146096]
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    int year = (int)(yoe) + (int)(era) * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    unsigned mp = (5 * doy + 2) / 153;                      // [0, 11]
    *d = doy - (153 * mp + 2) / 5 + 1;                      // [1, 31]
    *m = mp < 10 ? mp + 3 : mp - 9;                         // [1, 12]
    *y = year + (*m <= 2);
}

void wc_default_name(uint32_t epoch, const char *prefix, char *out, size_t cap) {
    int64_t days = (int64_t)(epoch / 86400u);
    uint32_t rem = epoch % 86400u;
    unsigned hh = rem / 3600u;
    unsigned mm = (rem % 3600u) / 60u;
    int y;
    unsigned mo, d;
    civil_from_days(days, &y, &mo, &d);
    snprintf(out, cap, "%s_%04d%02u%02u_%02u%02u", prefix, y, mo, d, hh, mm);
}

void wc_default_capture_name(uint32_t epoch, char *out, size_t cap) {
    wc_default_name(epoch, "cap", out, cap);
}
