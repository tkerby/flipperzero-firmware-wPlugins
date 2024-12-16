#pragma once
#include "game/icon.h"

// Global variables to store camera position
extern int camera_x;
extern int camera_y;
void draw_background(Canvas *canvas, Vector pos);
void draw_icon_line(Canvas *canvas, Vector pos, int amount, bool horizontal, const Icon *icon);
void draw_icon_half_world(Canvas *canvas, bool right, const Icon *icon);
void spawn_icon(Level *level, const Icon *icon, float x, float y, uint8_t width, uint8_t height);
void spawn_icon_line(Level *level, const Icon *icon, float x, float y, uint8_t width, uint8_t height, uint8_t amount, bool horizontal);

// create custom icons at https://lopaka.app/sandbox