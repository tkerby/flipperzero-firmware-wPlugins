#ifndef __CUBERZERO_H__
#define __CUBERZERO_H__

#include <furi.h>

#define CUBERZERO_TAG	  "CuberZero"
#define CUBERZERO_VERSION "0.0.1"

#define CUBERZERO_INFO(text, ...) furi_log_print_format(FuriLogLevelInfo, CUBERZERO_TAG, text, ##__VA_ARGS__)

typedef struct {
	int x;
} CUBERZERO, *PCUBERZERO;

#endif
