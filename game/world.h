#pragma once
#include <game/draw.h>
// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// World size (6x6)
#define WORLD_WIDTH 768
#define WORLD_HEIGHT 384

// Maximum number of world objects
#define MAX_WORLD_OBJECTS 25 // any more than that and we may run out of heap when switching worlds
const LevelBehaviour *training_world();
bool draw_json_world_furi(GameManager *manager, Level *level, const FuriString *json_data);
FuriString *fetch_world(const char *name);