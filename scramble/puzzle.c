#include "shared_i.h"

static inline int32_t nextBits(int64_t* const seed, const uint8_t bits) {
	return ((uint64_t) (*seed = (*seed * MULTIPLIER + ADDEND) & MASK)) >> (48 - bits);
}

int32_t InternalRandomNextInt(int64_t* const seed, const int32_t bound) {
	if(!seed || !bound) {
		return 0;
	}

	int32_t result = nextBits(seed, 31);
	const int32_t x = bound - 1;

	if(bound & x) {
		for(int32_t y = result; y - (result = y % bound) + x < 0; y = nextBits(seed, 31));
		return result;
	}

	return (((int64_t) result) * bound) >> 31;
}
