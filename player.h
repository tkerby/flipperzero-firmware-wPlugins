#ifndef PLAYER_H
#define PLAYER_H

#include "game.h"

typedef struct {
    Sprite* sprite;
    float Yvelocity;
    bool isGrounded;
} PlayerContext;

extern const EntityDescription player_desc;

void player_spawn(Level* level, GameManager* manager);

#endif
