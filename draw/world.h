#pragma once
#include <draw/draw.h>
#include <game.h>
#include <flip_world.h>
void draw_bounds(Canvas *canvas);
void draw_example_world(Level *level);
void draw_tree_world(Level *level);
bool draw_json_world(Level *level, FuriString *json_data);