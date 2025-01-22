#pragma once
#include "flip_world_icons.h"
#include "game.h"

typedef struct {
    char id[32];
    const Icon* icon;
    uint8_t width;
    uint8_t height;
} IconContext;

extern const EntityDescription icon_desc;
IconContext* get_icon_context(const char* name);
const char* icon_get_id(const Icon* icon);
