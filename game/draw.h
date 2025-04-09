#pragma once
#include "game.h"
#include "game/icon.h"
#include <game/player.h>
extern int draw_camera_x;
extern int draw_camera_y;
void draw_user_stats(Canvas *canvas, Vector pos, GameManager *manager);
void draw_username(Canvas *canvas, Vector pos, char *username);
void draw_spawn_icon(GameManager *manager, Level *level, const char *icon_id, float x, float y);
void draw_spawn_icon_line(GameManager *manager, Level *level, const char *icon_id, float x, float y, uint8_t amount, bool horizontal, uint8_t spacing);
void draw_background_render(Canvas *canvas, GameManager *manager);