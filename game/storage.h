#pragma once
#include <game/player.h>
#include <game/game.h>
#include <flip_world.h>
#include <flip_storage/storage.h>

bool save_player_context(PlayerContext *player_context);
PlayerContext *load_player_context();
