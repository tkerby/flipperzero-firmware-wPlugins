#include "player_animation_loader.h"

Sprite* idle[2];
Sprite* sword_swing[7];

void idle_animation_load(GameManager* manager) {
    idle[0] = game_manager_sprite_load(manager, "other/player.fxbm");
    idle[1] = game_manager_sprite_load(manager, "other/idle.fxbm");
}

void swinging_sword_animation_load(GameManager* manager) {
    sword_swing[0] = game_manager_sprite_load(manager, "other/player.fxbm");
    sword_swing[1] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_1.png");
    sword_swing[2] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_2.png");
    sword_swing[3] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_3.png");
    sword_swing[4] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_4.png");
    sword_swing[5] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_5.png");
    sword_swing[6] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_6.png");
}
