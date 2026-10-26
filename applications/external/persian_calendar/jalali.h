#pragma once

#include <stdbool.h>

/* Jalali (Persian) <-> Gregorian conversion, jdf 33-year cycle algorithm.
 * Months are 1-based. Valid for Gregorian years ~1800..2200. */

void gregorian_to_jalali(int gy, int gm, int gd, int* jy, int* jm, int* jd);
void jalali_to_gregorian(int jy, int jm, int jd, int* gy, int* gm, int* gd);

bool gregorian_is_leap(int gy);
bool jalali_is_leap(int jy);

int gregorian_month_days(int gy, int gm);
int jalali_month_days(int jy, int jm);

/* 0=Sunday .. 6=Saturday */
int weekday_from_gregorian(int gy, int gm, int gd);
