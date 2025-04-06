#pragma once
#include "flip_world_icons.h"
#include "game.h"

typedef enum {
    ICON_ID_HOUSE, // House
    ICON_ID_MAN, // Man
    ICON_ID_PLANT, // Plant
    ICON_ID_TREE, // Tree
    ICON_ID_WOMAN, // Woman
    ICON_ID_FENCE, // Fence
    ICON_ID_FENCE_END, // Fence end
    ICON_ID_FENCE_VERTICAL_END, // Vertical fence end
    ICON_ID_FENCE_VERTICAL_START, // Vertical fence start
    ICON_ID_FLOWER, // Flower
    ICON_ID_LAKE_BOTTOM, // Lake bottom
    ICON_ID_LAKE_BOTTOM_LEFT, // Lake bottom left
    ICON_ID_LAKE_BOTTOM_RIGHT, // Lake bottom right
    ICON_ID_LAKE_LEFT, // Lake left
    ICON_ID_LAKE_RIGHT, // Lake right
    ICON_ID_LAKE_TOP, // Lake top
    ICON_ID_LAKE_TOP_LEFT, // Lake top left
    ICON_ID_LAKE_TOP_RIGHT, // Lake top right
    ICON_ID_ROCK_LARGE, // Large rock
    ICON_ID_ROCK_MEDIUM, // Medium rock
    ICON_ID_ROCK_SMALL, // Small rock
} IconID;

typedef struct {
    IconID id;
    const Icon* icon;
    Vector size;
} IconContext;

extern const EntityDescription icon_desc;
IconContext* get_icon_context(const char* name);
