#include "player_animations.h"

Sprite* idle[2];
Sprite* sword_swing[7];
Sprite* walking[8];

Sprite* idle_right[2];
Sprite* sword_swing_right[7];
Sprite* walking_right[8];

int idle_current_frame = 0;
int swinging_sword_current_frame = 0;
int walking_current_frame = 0;

int idle_right_current_frame = 0;
int swinging_sword_right_current_frame = 0;
int walking_right_current_frame = 0;

// this tells how many frames to wait until going to the next animation frame
int idle_fps = 30;
int swinging_sword_fps = 0;
int walking_fps = 4;

int idle_i;
int sword_i;
int walking_i;

int idle_right_i;
int sword_right_i;
int walking_right_i;

void Idle_animation_load(GameManager* manager) {
    idle[0] = game_manager_sprite_load(manager, "other/player.fxbm");
    idle[1] = game_manager_sprite_load(manager, "other/idle.fxbm");
}

void Swinging_sword_animation_load(GameManager* manager) {
    sword_swing[0] = game_manager_sprite_load(manager, "other/player.fxbm");
    sword_swing[1] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_1.fxbm");
    sword_swing[2] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_2.fxbm");
    sword_swing[3] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_3.fxbm");
    sword_swing[4] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_4.fxbm");
    sword_swing[5] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_5.fxbm");
    sword_swing[6] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_6.fxbm");
}

void Walking_animation_load(GameManager* manager) {
    walking[0] = game_manager_sprite_load(manager, "walking/walking_0.fxbm");
    walking[1] = game_manager_sprite_load(manager, "walking/walking_1.fxbm");
    walking[2] = game_manager_sprite_load(manager, "walking/walking_2.fxbm");
    walking[3] = game_manager_sprite_load(manager, "walking/walking_3.fxbm");
    walking[4] = game_manager_sprite_load(manager, "walking/walking_4.fxbm");
    walking[5] = game_manager_sprite_load(manager, "walking/walking_5.fxbm");
    walking[6] = game_manager_sprite_load(manager, "walking/walking_6.fxbm");
    walking[7] = game_manager_sprite_load(manager, "walking/walking_7.fxbm");
}

// facing right animation load
void Idle_animation_right_load(GameManager* manager) {
    idle_right[0] = game_manager_sprite_load(manager, "other/player_right.fxbm");
    idle_right[1] = game_manager_sprite_load(manager, "other/idle_right.fxbm");
}

void Swinging_sword_right_animation_load(GameManager *manager){
    sword_swing_right[0] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_right_1.png");
    sword_swing_right[2] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_right_2.png");
    sword_swing_right[2] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_right_3.png");
    sword_swing_right[3] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_right_4.png");
    sword_swing_right[4] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_right_5.png");
    sword_swing_right[5] = game_manager_sprite_load(manager, "swinging_sword/swinging_sword_right_6.png");
}

void Walking_right_animation_load(GameManager* manager) {
    walking_right[0] = game_manager_sprite_load(manager, "walking/walking_right_0.png");
    walking_right[1] = game_manager_sprite_load(manager, "walking/walking_right_1.png");
    walking_right[2] = game_manager_sprite_load(manager, "walking/walking_right_2.png");
    walking_right[3] = game_manager_sprite_load(manager, "walking/walking_right_3.png");
    walking_right[4] = game_manager_sprite_load(manager, "walking/walking_right_4.png");
    walking_right[5] = game_manager_sprite_load(manager, "walking/walking_right_5.png");
    walking_right[6] = game_manager_sprite_load(manager, "walking/walking_right_6.png");
    walking_right[7] = game_manager_sprite_load(manager, "walking/walking_right_7.png");
}

void Idle_animation_play(GameManager* manager, void* context) {
    UNUSED(manager);
    int total_frames = sizeof(idle) / sizeof(idle[0]);
    if(idle_current_frame == total_frames) {
        idle_current_frame = 0;
    }

    PlayerContext* playerContext = (PlayerContext*)context;
    playerContext->sprite = idle[idle_current_frame];

    idle_i++;
    if(idle_i >= idle_fps) {
        idle_current_frame++;
        idle_i = 0;
    }
}

void Walking_animation_play(GameManager* manager, void* context) {
    UNUSED(manager);
    int total_frames = sizeof(walking) / sizeof(walking[0]);
    if(walking_current_frame == total_frames) {
        walking_current_frame = 0;
    }

    PlayerContext* playerContext = (PlayerContext*)context;
    playerContext->sprite = walking[walking_current_frame];

    walking_i++;
    if(walking_i >= walking_fps) {
        walking_current_frame++;
        walking_i = 0;
    }
}
