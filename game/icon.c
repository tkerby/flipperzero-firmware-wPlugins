#include "game/icon.h"

static void icon_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(self);
    IconContext *ictx = (IconContext *)context;
    if (ictx && entity_description_get(other) == &player_desc)
    {
        PlayerContext *player = (PlayerContext *)entity_context_get(other);
        if (player)
        {
            Vector player_pos = entity_pos_get(other);

            // Bounce player in the direction they came
            switch (player->direction)
            {
            case PLAYER_UP:
                player_pos.y += ictx->size.y;
                break;
            case PLAYER_DOWN:
                player_pos.y -= ictx->size.y;
                break;
            case PLAYER_LEFT:
                player_pos.x += ictx->size.x;
                break;
            case PLAYER_RIGHT:
                player_pos.x -= ictx->size.x;
                break;
            };

            entity_pos_set(other, player_pos);

            // Reset movement to prevent re-collision
            player->dx = 0;
            player->dy = 0;
        }
    }
}

static void icon_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(manager);
    IconContext *ictx = (IconContext *)context;
    if (ictx)
    {
        Vector pos = entity_pos_get(self);

        // Draw the icon, centered
        canvas_draw_icon(
            canvas,
            pos.x - camera_x - ictx->size.x / 2,
            pos.y - camera_y - ictx->size.y / 2,
            ictx->icon);
    }
}

static void icon_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(manager);

    IconContext *ictx_self = (IconContext *)context;
    if (!ictx_self)
    {
        FURI_LOG_E("Game", "Icon context self is NULL");
        return;
    }
    IconContext *ictx = entity_context_get(self);
    if (!ictx)
    {
        FURI_LOG_E("Game", "Icon context is NULL");
        return;
    }

    IconContext *loaded_data = get_icon_context(g_name);
    if (!loaded_data)
    {
        FURI_LOG_E("Game", "Failed to find icon data for %s", g_name);
        return;
    }

    ictx_self->icon = loaded_data->icon;
    ictx_self->size = (Vector){loaded_data->size.x, loaded_data->size.y};
    ictx->icon = loaded_data->icon;
    ictx->size = (Vector){loaded_data->size.x, loaded_data->size.y};

    Vector pos = entity_pos_get(self);
    pos.x += ictx_self->size.x / 2;
    pos.y += ictx_self->size.y / 2;
    entity_pos_set(self, pos);

    entity_collider_add_circle(
        self,
        (ictx_self->size.x + ictx_self->size.y) / 4);

    free(loaded_data);
}

// -------------- Stop callback --------------
static void icon_free(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    if (context)
    {
        free(context);
    }
}

// -------------- Entity description --------------
const EntityDescription icon_desc = {
    .start = icon_start,
    .stop = icon_free,
    .update = NULL,
    .render = icon_render,
    .collision = icon_collision,
    .event = NULL,
    .context_size = sizeof(IconContext),
};

static IconContext *icon_generic_alloc(const char *id, const Icon *icon, uint8_t width, uint8_t height)
{
    IconContext *ctx = malloc(sizeof(IconContext));
    if (!ctx)
    {
        FURI_LOG_E("Game", "Failed to allocate IconContext");
        return NULL;
    }
    snprintf(ctx->id, sizeof(ctx->id), "%s", id);
    ctx->icon = icon;
    ctx->size = (Vector){width, height};
    return ctx;
}

IconContext *get_icon_context(const char *name)
{
    // if (is_str(name, "earth") )
    // {
    //     return icon_generic_alloc("earth", &I_icon_earth_15x16, 15, 16);
    // }
    // else if (is_str(name, "home") )
    // {
    //     return icon_generic_alloc("home", &I_icon_home_15x16, 15, 16);
    // }
    if (is_str(name, "house"))
    {
        return icon_generic_alloc("house", &I_icon_house_48x32px, 48, 32);
    }
    // else if (is_str(name, "house_3d") )
    // {
    //     return icon_generic_alloc("house_3d", &I_icon_house_3d_34x45px, 34, 45);
    // }
    // else if (is_str(name, "info") )
    // {
    //     return icon_generic_alloc("info", &I_icon_info_15x16, 15, 16);
    // }
    else if (is_str(name, "man"))
    {
        return icon_generic_alloc("man", &I_icon_man_7x16, 7, 16);
    }
    else if (is_str(name, "plant"))
    {
        return icon_generic_alloc("plant", &I_icon_plant_16x16, 16, 16);
    }
    // else if (is_str(name, "plant_fern") )
    // {
    //     return icon_generic_alloc("plant_fern", &I_icon_plant_fern_18x16px, 18, 16);
    // }
    // else if (is_str(name, "plant_pointy") )
    // {
    //     return icon_generic_alloc("plant_pointy", &I_icon_plant_pointy_13x16px, 13, 16);
    // }
    else if (is_str(name, "tree"))
    {
        return icon_generic_alloc("tree", &I_icon_tree_16x16, 16, 16);
    }
    // else if (is_str(name, "tree_29x30") )
    // {
    //     return icon_generic_alloc("tree_29x30", &I_icon_tree_29x30px, 29, 30);
    // }
    // else if (is_str(name, "tree_48x48") )
    // {
    //     return icon_generic_alloc("tree_48x48", &I_icon_tree_48x48px, 48, 48);
    // }
    else if (is_str(name, "woman"))
    {
        return icon_generic_alloc("woman", &I_icon_woman_9x16, 9, 16);
    }
    // else if (is_str(name, "chest_closed"))
    // {
    //     return icon_generic_alloc("chest_closed", &I_icon_chest_closed_16x13px, 16, 13);
    // }
    // else if (is_str(name, "chest_open") )
    // {
    //     return icon_generic_alloc("chest_open", &I_icon_chest_open_16x16px, 16, 16);
    // }
    else if (is_str(name, "fence"))
    {
        return icon_generic_alloc("fence", &I_icon_fence_16x8px, 16, 8);
    }
    else if (is_str(name, "fence_end"))
    {
        return icon_generic_alloc("fence_end", &I_icon_fence_end_16x8px, 16, 8);
    }
    // else if (is_str(name, "fence_vertical_end") )
    // {
    //     return icon_generic_alloc("fence_vertical_end", &I_icon_fence_vertical_end_6x8px, 6, 8);
    // }
    // else if (is_str(name, "fence_vertical_start") )
    // {
    //     return icon_generic_alloc("fence_vertical_start", &I_icon_fence_vertical_start_6x15px, 6, 15);
    // }
    else if (is_str(name, "flower"))
    {
        return icon_generic_alloc("flower", &I_icon_flower_16x16, 16, 16);
    }
    else if (is_str(name, "lake_bottom"))
    {
        return icon_generic_alloc("lake_bottom", &I_icon_lake_bottom_31x12px, 31, 12);
    }
    else if (is_str(name, "lake_bottom_left"))
    {
        return icon_generic_alloc("lake_bottom_left", &I_icon_lake_bottom_left_24x22px, 24, 22);
    }
    else if (is_str(name, "lake_bottom_right"))
    {
        return icon_generic_alloc("lake_bottom_right", &I_icon_lake_bottom_right_24x22px, 24, 22);
    }
    else if (is_str(name, "lake_left"))
    {
        return icon_generic_alloc("lake_left", &I_icon_lake_left_11x31px, 11, 31);
    }
    else if (is_str(name, "lake_right"))
    {
        return icon_generic_alloc("lake_right", &I_icon_lake_right_11x31, 11, 31);
    }
    else if (is_str(name, "lake_top"))
    {
        return icon_generic_alloc("lake_top", &I_icon_lake_top_31x12px, 31, 12);
    }
    else if (is_str(name, "lake_top_left"))
    {
        return icon_generic_alloc("lake_top_left", &I_icon_lake_top_left_24x22px, 24, 22);
    }
    else if (is_str(name, "lake_top_right"))
    {
        return icon_generic_alloc("lake_top_right", &I_icon_lake_top_right_24x22px, 24, 22);
    }
    else if (is_str(name, "rock_large"))
    {
        return icon_generic_alloc("rock_large", &I_icon_rock_large_18x19px, 18, 19);
    }
    else if (is_str(name, "rock_medium"))
    {
        return icon_generic_alloc("rock_medium", &I_icon_rock_medium_16x14px, 16, 14);
    }
    else if (is_str(name, "rock_small"))
    {
        return icon_generic_alloc("rock_small", &I_icon_rock_small_10x8px, 10, 8);
    }

    // If no match is found
    FURI_LOG_E("Game", "Icon not found: %s", name);
    return NULL;
}

const char *icon_get_id(const Icon *icon)
{
    // if (icon == &I_icon_earth_15x16)
    // {
    //     return "earth";
    // }
    // else if (icon == &I_icon_home_15x16)
    // {
    //     return "home";
    // }
    if (icon == &I_icon_house_48x32px)
    {
        return "house";
    }
    // else if (icon == &I_icon_house_3d_34x45px)
    // {
    //     return "house_3d";
    // }
    // else if (icon == &I_icon_info_15x16)
    // {
    //     return "info";
    // }
    else if (icon == &I_icon_man_7x16)
    {
        return "man";
    }
    else if (icon == &I_icon_plant_16x16)
    {
        return "plant";
    }
    // else if (icon == &I_icon_plant_fern_18x16px)
    // {
    //     return "plant_fern";
    // }
    // else if (icon == &I_icon_plant_pointy_13x16px)
    // {
    //     return "plant_pointy";
    // }
    else if (icon == &I_icon_tree_16x16)
    {
        return "tree";
    }
    // else if (icon == &I_icon_tree_29x30px)
    // {
    //     return "tree_29x30";
    // }
    // else if (icon == &I_icon_tree_48x48px)
    // {
    //     return "tree_48x48";
    // }
    else if (icon == &I_icon_woman_9x16)
    {
        return "woman";
    }
    // else if (icon == &I_icon_chest_closed_16x13px)
    // {
    //     return "chest_closed";
    // }
    // else if (icon == &I_icon_chest_open_16x16px)
    // {
    //     return "chest_open";
    // }
    else if (icon == &I_icon_fence_16x8px)
    {
        return "fence";
    }
    else if (icon == &I_icon_fence_end_16x8px)
    {
        return "fence_end";
    }
    // else if (icon == &I_icon_fence_vertical_end_6x8px)
    // {
    //     return "fence_vertical_end";
    // }
    // else if (icon == &I_icon_fence_vertical_start_6x15px)
    // {
    //     return "fence_vertical_start";
    // }
    else if (icon == &I_icon_flower_16x16)
    {
        return "flower";
    }
    else if (icon == &I_icon_lake_bottom_31x12px)
    {
        return "lake_bottom";
    }
    else if (icon == &I_icon_lake_bottom_left_24x22px)
    {
        return "lake_bottom_left";
    }
    else if (icon == &I_icon_lake_bottom_right_24x22px)
    {
        return "lake_bottom_right";
    }
    else if (icon == &I_icon_lake_left_11x31px)
    {
        return "lake_left";
    }
    else if (icon == &I_icon_lake_right_11x31)
    {
        return "lake_right";
    }
    else if (icon == &I_icon_lake_top_31x12px)
    {
        return "lake_top";
    }
    else if (icon == &I_icon_lake_top_left_24x22px)
    {
        return "lake_top_left";
    }
    else if (icon == &I_icon_lake_top_right_24x22px)
    {
        return "lake_top_right";
    }
    else if (icon == &I_icon_rock_large_18x19px)
    {
        return "rock_large";
    }
    else if (icon == &I_icon_rock_medium_16x14px)
    {
        return "rock_medium";
    }
    else if (icon == &I_icon_rock_small_10x8px)
    {
        return "rock_small";
    }

    // If no match is found
    FURI_LOG_E("Game", "Icon ID not found for given icon pointer.");
    return NULL;
}
