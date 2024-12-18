#include "game/icon.h"

// Icon entity description

static void icon_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(self);
    IconContext *icon = (IconContext *)context;
    UNUSED(icon);

    if (entity_description_get(other) == &player_desc)
    {
        PlayerContext *player = (PlayerContext *)entity_context_get(other);
        if (player)
        {
            Vector pos = entity_pos_get(other);

            // Bounce the player back by 2 units opposite their last movement direction
            pos.x -= player->dx * 2;
            pos.y -= player->dy * 2;
            entity_pos_set(other, pos);

            // Reset player's movement direction to prevent immediate re-collision
            player->dx = 0;
            player->dy = 0;
        }
    }
}

static void icon_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(manager);
    IconContext *icon_ctx = (IconContext *)context;
    Vector pos = entity_pos_get(self);
    canvas_draw_icon(canvas, pos.x - camera_x - icon_ctx->width / 2, pos.y - camera_y - icon_ctx->height / 2, icon_ctx->icon);
}

static void icon_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(manager);
    IconContext *icon_ctx = (IconContext *)context;
    // Just add the collision rectangle for 16x16 icon
    entity_collider_add_rect(self, icon_ctx->width + COLLISION_BOX_PADDING_HORIZONTAL, icon_ctx->height + COLLISION_BOX_PADDING_VERTICAL);
}

const EntityDescription icon_desc = {
    .start = icon_start,
    .stop = NULL,
    .update = NULL,
    .render = icon_render,
    .collision = icon_collision,
    .event = NULL,
    .context_size = sizeof(IconContext),
};

const Icon *get_icon(char *name)
{
    if (strcmp(name, "earth") == 0)
    {
        return &I_icon_earth_15x16;
    }
    if (strcmp(name, "home") == 0)
    {
        return &I_icon_home_15x16;
    }
    if (strcmp(name, "house") == 0)
    {
        return &I_icon_house_48x32px;
    }
    if (strcmp(name, "house_3d") == 0)
    {
        return &I_icon_house_3d_34x45px;
    }
    if (strcmp(name, "info") == 0)
    {
        return &I_icon_info_15x16;
    }
    if (strcmp(name, "man") == 0)
    {
        return &I_icon_man_7x16;
    }
    if (strcmp(name, "plant") == 0)
    {
        return &I_icon_plant_16x16;
    }
    if (strcmp(name, "plant_fern") == 0)
    {
        return &I_icon_plant_fern_18x16px;
    }
    if (strcmp(name, "plant_pointy") == 0)
    {
        return &I_icon_plant_pointy_13x16px;
    }
    if (strcmp(name, "tree") == 0)
    {
        return &I_icon_tree_16x16;
    }
    if (strcmp(name, "tree_29x30") == 0)
    {
        return &I_icon_tree_29x30px;
    }
    if (strcmp(name, "tree_48x48") == 0)
    {
        return &I_icon_tree_48x48px;
    }
    if (strcmp(name, "woman") == 0)
    {
        return &I_icon_woman_9x16;
    }
    if (strcmp(name, "chest_closed") == 0)
    {
        return &I_icon_chest_closed_16x13px;
    }
    if (strcmp(name, "chest_open") == 0)
    {
        return &I_icon_chest_open_16x16px;
    }
    if (strcmp(name, "fence") == 0)
    {
        return &I_icon_fence_16x8px;
    }
    if (strcmp(name, "fence_end") == 0)
    {
        return &I_icon_fence_end_16x8px;
    }
    if (strcmp(name, "fence_vertical_end") == 0)
    {
        return &I_icon_fence_vertical_end_6x8px;
    }
    if (strcmp(name, "fence_vertical_start") == 0)
    {
        return &I_icon_fence_vertical_start_6x15px;
    }
    if (strcmp(name, "flower") == 0)
    {
        return &I_icon_flower_16x16;
    }
    if (strcmp(name, "lake_bottom") == 0)
    {
        return &I_icon_lake_bottom_31x12px;
    }
    if (strcmp(name, "lake_bottom_left") == 0)
    {
        return &I_icon_lake_bottom_left_24x22px;
    }
    if (strcmp(name, "lake_bottom_right") == 0)
    {
        return &I_icon_lake_bottom_right_24x22px;
    }
    if (strcmp(name, "lake_left") == 0)
    {
        return &I_icon_lake_left_11x31;
    }
    if (strcmp(name, "lake_right") == 0)
    {
        return &I_icon_lake_right_11x31px;
    }
    if (strcmp(name, "lake_top") == 0)
    {
        return &I_icon_lake_top_31x22px;
    }
    if (strcmp(name, "lake_top_left") == 0)
    {
        return &I_icon_lake_top_left_24x22px;
    }
    if (strcmp(name, "lake_top_right") == 0)
    {
        return &I_icon_lake_top_right_24x22px;
    }
    if (strcmp(name, "rock_large") == 0)
    {
        return &I_icon_rock_large_18x19px;
    }
    if (strcmp(name, "rock_medium") == 0)
    {
        return &I_icon_rock_medium_16x14px;
    }
    if (strcmp(name, "rock_small") == 0)
    {
        return &I_icon_rock_small_10x8px;
    }

    // If no match is found
    return NULL;
}

const Icon *get_icon_furi(FuriString *name)
{
    if (furi_string_cmp(name, "earth") == 0)
    {
        return &I_icon_earth_15x16;
    }
    if (furi_string_cmp(name, "home") == 0)
    {
        return &I_icon_home_15x16;
    }
    if (furi_string_cmp(name, "house") == 0)
    {
        return &I_icon_house_48x32px;
    }
    if (furi_string_cmp(name, "house_3d") == 0)
    {
        return &I_icon_house_3d_34x45px;
    }
    if (furi_string_cmp(name, "info") == 0)
    {
        return &I_icon_info_15x16;
    }
    if (furi_string_cmp(name, "man") == 0)
    {
        return &I_icon_man_7x16;
    }
    if (furi_string_cmp(name, "plant") == 0)
    {
        return &I_icon_plant_16x16;
    }
    if (furi_string_cmp(name, "plant_fern") == 0)
    {
        return &I_icon_plant_fern_18x16px;
    }
    if (furi_string_cmp(name, "plant_pointy") == 0)
    {
        return &I_icon_plant_pointy_13x16px;
    }
    if (furi_string_cmp(name, "tree") == 0)
    {
        return &I_icon_tree_16x16;
    }
    if (furi_string_cmp(name, "tree_29x30") == 0)
    {
        return &I_icon_tree_29x30px;
    }
    if (furi_string_cmp(name, "tree_48x48") == 0)
    {
        return &I_icon_tree_48x48px;
    }
    if (furi_string_cmp(name, "woman") == 0)
    {
        return &I_icon_woman_9x16;
    }
    if (furi_string_cmp(name, "chest_closed") == 0)
    {
        return &I_icon_chest_closed_16x13px;
    }
    if (furi_string_cmp(name, "chest_open") == 0)
    {
        return &I_icon_chest_open_16x16px;
    }
    if (furi_string_cmp(name, "fence") == 0)
    {
        return &I_icon_fence_16x8px;
    }
    if (furi_string_cmp(name, "fence_end") == 0)
    {
        return &I_icon_fence_end_16x8px;
    }
    if (furi_string_cmp(name, "fence_vertical_end") == 0)
    {
        return &I_icon_fence_vertical_end_6x8px;
    }
    if (furi_string_cmp(name, "fence_vertical_start") == 0)
    {
        return &I_icon_fence_vertical_start_6x15px;
    }
    if (furi_string_cmp(name, "flower") == 0)
    {
        return &I_icon_flower_16x16;
    }
    if (furi_string_cmp(name, "lake_bottom") == 0)
    {
        return &I_icon_lake_bottom_31x12px;
    }
    if (furi_string_cmp(name, "lake_bottom_left") == 0)
    {
        return &I_icon_lake_bottom_left_24x22px;
    }
    if (furi_string_cmp(name, "lake_bottom_right") == 0)
    {
        return &I_icon_lake_bottom_right_24x22px;
    }
    if (furi_string_cmp(name, "lake_left") == 0)
    {
        return &I_icon_lake_left_11x31;
    }
    if (furi_string_cmp(name, "lake_right") == 0)
    {
        return &I_icon_lake_right_11x31px;
    }
    if (furi_string_cmp(name, "lake_top") == 0)
    {
        return &I_icon_lake_top_31x22px;
    }
    if (furi_string_cmp(name, "lake_top_left") == 0)
    {
        return &I_icon_lake_top_left_24x22px;
    }
    if (furi_string_cmp(name, "lake_top_right") == 0)
    {
        return &I_icon_lake_top_right_24x22px;
    }
    if (furi_string_cmp(name, "rock_large") == 0)
    {
        return &I_icon_rock_large_18x19px;
    }
    if (furi_string_cmp(name, "rock_medium") == 0)
    {
        return &I_icon_rock_medium_16x14px;
    }
    if (furi_string_cmp(name, "rock_small") == 0)
    {
        return &I_icon_rock_small_10x8px;
    }

    // If no match is found
    return NULL;
}
