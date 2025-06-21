#include "random.h"

#define MULTIPLIER 0x00000005DEECE66DLL
#define ADDEND	   0x000000000000000BLL
#define MASK	   0x0000FFFFFFFFFFFFLL

static inline int32_t nextBits(int64_t* const seed, const uint8_t bits) {
	return !seed || bits > 48 ? 0 : ((int32_t) (((uint64_t) (*seed = (*seed * MULTIPLIER + ADDEND) & MASK)) >> (48 - bits)));
}
