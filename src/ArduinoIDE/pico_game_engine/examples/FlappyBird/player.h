#pragma once
#include <Arduino.h>
#include "PicoGameEngine.h"
#ifndef TFT_DARKGREEN
#define TFT_DARKGREEN 0x07E0
#endif
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif
#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif
void player_spawn(Level *level, Game *game);