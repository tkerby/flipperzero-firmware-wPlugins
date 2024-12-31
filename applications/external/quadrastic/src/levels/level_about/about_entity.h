#pragma once

#include "../../engine/entity.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* logo_sprite;
} AboutContext;

extern const EntityDescription about_description;
