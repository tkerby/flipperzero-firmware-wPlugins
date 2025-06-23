#include "puzzle.h"

#define MULTIPLIER 0x00000005DEECE66DLL
#define ADDEND	   0x000000000000000BLL
#define MASK	   0x0000FFFFFFFFFFFFLL

#define SEED_INITIALIZE(x) (((x) ^ MULTIPLIER) & MASK)

int32_t InternalRandomNextInt(int64_t* const seed, const int32_t bound);
