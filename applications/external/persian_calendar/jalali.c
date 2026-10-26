#include "jalali.h"

void gregorian_to_jalali(int gy, int gm, int gd, int* jy, int* jm, int* jd) {
    static const int g_d_m[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int gy2 = (gm > 2) ? (gy + 1) : gy;
    long days = 355666L + (365L * gy) + ((gy2 + 3) / 4) - ((gy2 + 99) / 100) +
                ((gy2 + 399) / 400) + gd + g_d_m[gm - 1];
    int y = -1595 + 33 * (int)(days / 12053);
    days %= 12053;
    y += 4 * (int)(days / 1461);
    days %= 1461;
    if(days > 365) {
        y += (int)((days - 1) / 365);
        days = (days - 1) % 365;
    }
    if(days < 186) {
        *jm = 1 + (int)(days / 31);
        *jd = 1 + (int)(days % 31);
    } else {
        *jm = 7 + (int)((days - 186) / 30);
        *jd = 1 + (int)((days - 186) % 30);
    }
    *jy = y;
}

void jalali_to_gregorian(int jy, int jm, int jd, int* gy, int* gm, int* gd) {
    int jy2 = jy + 1595;
    long days = -355668L + (365L * jy2) + ((jy2 / 33) * 8) + (((jy2 % 33) + 3) / 4) + jd +
                ((jm < 7) ? (jm - 1) * 31 : ((jm - 7) * 30) + 186);
    int y = 400 * (int)(days / 146097);
    days %= 146097;
    if(days > 36524) {
        y += 100 * (int)(--days / 36524);
        days %= 36524;
        if(days >= 365) days++;
    }
    y += 4 * (int)(days / 1461);
    days %= 1461;
    if(days > 365) {
        y += (int)((days - 1) / 365);
        days = (days - 1) % 365;
    }
    int d = (int)(days + 1);
    const int sal_a[12] =
        {31, gregorian_is_leap(y) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 1;
    while(m <= 12 && d > sal_a[m - 1]) {
        d -= sal_a[m - 1];
        m++;
    }
    *gy = y;
    *gm = m;
    *gd = d;
}

bool gregorian_is_leap(int gy) {
    return (gy % 4 == 0 && gy % 100 != 0) || (gy % 400 == 0);
}

bool jalali_is_leap(int jy) {
    /* Leap years of the 33-year cycle used by the conversion above */
    int r = jy % 33;
    if(r < 0) r += 33;
    return r == 1 || r == 5 || r == 9 || r == 13 || r == 17 || r == 22 || r == 26 || r == 30;
}

int gregorian_month_days(int gy, int gm) {
    static const int md[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(gm == 2 && gregorian_is_leap(gy)) return 29;
    return md[gm - 1];
}

int jalali_month_days(int jy, int jm) {
    if(jm <= 6) return 31;
    if(jm <= 11) return 30;
    return jalali_is_leap(jy) ? 30 : 29;
}

int weekday_from_gregorian(int gy, int gm, int gd) {
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if(gm < 3) gy -= 1;
    return (gy + gy / 4 - gy / 100 + gy / 400 + t[gm - 1] + gd) % 7;
}
