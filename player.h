#ifndef PLAYER_H
#define PLAYER_H

#include "game.h"

typedef struct {
    Sprite* sprite;
    float Yvelocity;
    bool isGrounded;
} PlayerContext;

extern float ground_hight;

extern const EntityDescription player_desc;

void player_spawn(Level* level, GameManager* manager);

extern void Animations(GameManager* manager, void* context);

#endif
