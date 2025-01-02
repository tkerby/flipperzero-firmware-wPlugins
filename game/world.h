#pragma once
#include <game/draw.h>
// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// World size (3x3)
#define WORLD_WIDTH 384
#define WORLD_HEIGHT 192

// Maximum number of world objects
#define MAX_WORLD_OBJECTS 25 // any more than that and we may run out of heap when switching worlds

void draw_bounds(Canvas *canvas);
void draw_town_world(Level *level);
bool draw_json_world_furi(Level *level, const FuriString *json_data);
FuriString *fetch_world(const char *name);