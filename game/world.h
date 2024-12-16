#pragma once
#include <game/draw.h>
// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// World size (3x3)
#define WORLD_WIDTH 384
#define WORLD_HEIGHT 192

// Maximum number of world objects
#define MAX_WORLD_OBJECTS 100

// Maximum number of world tokens
#define MAX_WORLD_TOKENS 1024
void draw_bounds(Canvas *canvas);
void draw_example_world(Level *level);
void draw_tree_world(Level *level);
bool draw_json_world(Level *level, char *json_data);
bool draw_json_world_furi(Level *level, FuriString *json_data);