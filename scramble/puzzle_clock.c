#include "shared_i.h"
#include <stdlib.h>

#define SCRAMBLE_LENGTH 66

static const char* const ScrambleTemplate = "UR*x DR*x DL*x UL*x U*x R*x D*x L*x ALL*x y2 U*x R*x D*x L*x ALL*x";

char* PuzzleScrambleClock(const int64_t seed) {
	char* buffer = malloc(sizeof(char) * (SCRAMBLE_LENGTH + 1));

	if(!buffer) {
		return 0;
	}

	int64_t currentSeed = SEED_INITIALIZE(seed);
	int8_t scramble;

	for(uint8_t i = 0; i < SCRAMBLE_LENGTH; i++) {
		if(ScrambleTemplate[i] != '*') {
			buffer[i] = ScrambleTemplate[i];
			continue;
		}

		scramble = ((int8_t) InternalRandomNextInt(&currentSeed, 12)) - 5;
		buffer[i] = scramble * (scramble < 0 ? -1 : 1) + '0';
		buffer[i + 1] = scramble < 0 ? '-' : '+';
	}

	buffer[SCRAMBLE_LENGTH] = 0;
	return buffer;
}
