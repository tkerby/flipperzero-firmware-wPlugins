#include "shared_i.h"

#define SCRAMBLE_LENGTH 20

char* PuzzleScrambleThreeByThree(const int64_t seed) {
	int64_t currentSeed = SEED_INITIALIZE(seed);
	uint8_t moves[SCRAMBLE_LENGTH];
	uint8_t index = 0;
	uint8_t length = SCRAMBLE_LENGTH * 2;
	uint8_t move;

	while(index < SCRAMBLE_LENGTH) {
		move = InternalRandomNextInt(&currentSeed, 18);

		if((index > 0 && moves[index - 1] / 3 == move / 3) || (index > 1 && moves[index - 2] / 3 % 3 == move / 3 % 3)) {
			continue;
		}

		moves[index++] = move;

		if(move % 3) {
			length++;
		}
	}

	char* const buffer = malloc(sizeof(char) * length);

	if(!buffer) {
		return 0;
	}

	static const char notations[] = {'U', 'R', 'F', 'D', 'L', 'B'};
	index = 0;

	for(move = 0; move < SCRAMBLE_LENGTH; move++) {
		if(move) {
			buffer[index++] = ' ';
		}

		buffer[index++] = notations[moves[move] / 3];

		switch(moves[move] % 3) {
		case 1:
			buffer[index++] = '2';
			break;
		case 2:
			buffer[index++] = '\'';
			break;
		}
	}

	buffer[length - 1] = 0;
	return buffer;
}
