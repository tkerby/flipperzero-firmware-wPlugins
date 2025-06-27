#include <stdint.h>

#ifndef __PUZZLE_H__
#define __PUZZLE_H__
#ifdef __cplusplus
extern "C" {
#endif
char* PuzzleScrambleClock(const int64_t seed);
char* PuzzleScrambleTwoByTwo(const int64_t seed);
#ifdef __cplusplus
}
#endif
#endif
