#pragma once
#include "game.h"
#include "game/icon.h"
#include <game/player.h>

// Global variables to store camera position
extern int camera_x;
extern int camera_y;
void draw_user_stats(Canvas *canvas, Vector pos, GameManager *manager);
void draw_username(Canvas *canvas, Vector pos, char *username);
void spawn_icon(GameManager *manager, Level *level, const char *icon_id, float x, float y);
void spawn_icon_line(GameManager *manager, Level *level, const char *icon_id, float x, float y, uint8_t amount, bool horizontal, uint8_t spacing);
extern char g_name[32];
void background_render(Canvas *canvas, GameManager *manager);