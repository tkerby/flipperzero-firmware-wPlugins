#pragma once
#include <game/game.h>
#include "flip_world.h"

// NPCContext definition
typedef struct
{
    char id[64];               // Unique ID for the NPC type
    int index;                 // Index for the specific NPC instance
    Vector size;               // Size of the NPC
    Sprite *sprite_right;      // NPC sprite when looking right
    Sprite *sprite_left;       // NPC sprite when looking left
    EntityDirection direction; // Direction the NPC is facing
    EntityState state;         // Current state of the NPC
    Vector start_position;     // Start position of the NPC
    Vector end_position;       // End position of the NPC
    float move_timer;          // Timer for the NPC movement
    float elapsed_move_timer;  // Elapsed time for the NPC movement
    float radius;              // Collision radius for the NPC
    float speed;               // Speed of the NPC
} NPCContext;

void spawn_npc(Level *level, GameManager *manager, FuriString *json);