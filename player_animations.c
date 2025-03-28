#include "player_animations.h"

Sprite* idle[2];
Sprite* sword_swing[7];
Sprite* walking[8];

int idle_current_frame = 0;
int swinging_sword_current_frame = 0;
int walking_current_frame = 0;

void Idle_animation_load(GameManager* manager) {
    idle[0] = game_manager_sprite_load(manager, "other/player.fxbm");
    idle[1] = game_manager_sprite_load(manager, "other/idle.fxbm");
}

void Swinging_sword_animation_load(GameManager* manager) {
    sword_swing[0] = game_manager_sprite_load(manager, "other/player.fxbm");
    sword_swing[1] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_1.png");
    sword_swing[2] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_2.png");
    sword_swing[3] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_3.png");
    sword_swing[4] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_4.png");
    sword_swing[5] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_5.png");
    sword_swing[6] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_6.png");
}

void Walking_animation_load(GameManager* manager) {
    walking[0] = game_manager_sprite_load(manager, "walking/walking_0.png");
    walking[1] = game_manager_sprite_load(manager, "walking/walking_1.png");
    walking[2] = game_manager_sprite_load(manager, "walking/walking_2.png");
    walking[3] = game_manager_sprite_load(manager, "walking/walking_3.png");
    walking[4] = game_manager_sprite_load(manager, "walking/walking_4.png");
    walking[5] = game_manager_sprite_load(manager, "walking/walking_5.png");
    walking[6] = game_manager_sprite_load(manager, "walking/walking_6.png");
    walking[7] = game_manager_sprite_load(manager, "walking/walking_7.png");
}

// the animation_play voids only transfer you to the next frames, any delay between frames needs to be done somewhere else.

void Idle_animation_play(GameManager* manager, void* context) {
    UNUSED(manager);
    int total_frames = sizeof(idle) / sizeof(idle[0]);
    if(idle_current_frame == total_frames) {
        idle_current_frame = 0;
    }

    PlayerContext* playerContext = (PlayerContext*)context;
    playerContext->sprite = idle[idle_current_frame];
    idle_current_frame++;
}
