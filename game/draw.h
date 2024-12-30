#pragma once
#include "game/icon.h"
#include <game/player.h>

// Global variables to store camera position
extern int camera_x;
extern int camera_y;
void draw_background(Canvas *canvas, Vector pos);
void draw_user_stats(Canvas *canvas, Vector pos, GameManager *manager);
void draw_username(Canvas *canvas, Vector pos, char *username);
void draw_icon_line(Canvas *canvas, Vector pos, int amount, bool horizontal, const Icon *icon);
void spawn_icon(Level *level, const char *icon_id, float x, float y);
void spawn_icon_line(Level *level, const char *icon_id, float x, float y, uint8_t amount, bool horizontal);
extern char g_temp_spawn_name[32];
// create custom icons at https://lopaka.app/sandbox