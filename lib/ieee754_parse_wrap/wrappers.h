#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compact decimal strtof/strtod wrappers, with RM's nan/inf/infinity spellings
 * retained for saved GPS fields and existing callers (case insensitive).
 * Hexadecimal floats are not supported: "0x1p3" consumes its leading zero only.
 * Decimal doubles may differ by up to 4 ulp; integers through 2^53 are exact.
 * Float results from ordinary %f fields match; difficult long mantissas can
 * differ by 1 float ulp. Values near DBL_MAX can round to infinity.
 * errno is set for double overflow/underflow-to-zero and float narrowing range
 * errors. Nonzero double subnormals do not necessarily report ERANGE.
 */
float __wrap_strtof(const char* in, char** tail);
double __wrap_strtod(const char* in, char** tail);

#ifdef __cplusplus
}
#endif
