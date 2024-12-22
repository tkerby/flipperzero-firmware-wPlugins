#include "game/icon.h"

// Icon entity description

static void icon_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(manager);
    UNUSED(self);
    IconContext* icon = (IconContext*)context;
    UNUSED(icon);

    if(entity_description_get(other) == &player_desc) {
        PlayerContext* player = (PlayerContext*)entity_context_get(other);
        if(player) {
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

static void icon_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    IconContext* icon_ctx = (IconContext*)context;
    Vector pos = entity_pos_get(self);
    canvas_draw_icon(
        canvas,
        pos.x - camera_x - icon_ctx->width / 2,
        pos.y - camera_y - icon_ctx->height / 2,
        icon_ctx->icon);
}

static void icon_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(manager);
    IconContext* icon_ctx = (IconContext*)context;
    // Just add the collision rectangle for 16x16 icon
    entity_collider_add_rect(
        self,
        icon_ctx->width + COLLISION_BOX_PADDING_HORIZONTAL,
        icon_ctx->height + COLLISION_BOX_PADDING_VERTICAL);
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

static IconContext _generic_icon = {
    .icon = &I_icon_earth_15x16,
    .width = 15,
    .height = 16,
};

IconContext* get_icon_context(char* name) {
    if(strcmp(name, "earth") == 0) {
        _generic_icon.icon = &I_icon_earth_15x16;
        _generic_icon.width = 15;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "home") == 0) {
        _generic_icon.icon = &I_icon_home_15x16;
        _generic_icon.width = 15;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "house") == 0) {
        _generic_icon.icon = &I_icon_house_48x32px;
        _generic_icon.width = 48;
        _generic_icon.height = 32;
        return &_generic_icon;
    }
    if(strcmp(name, "house_3d") == 0) {
        _generic_icon.icon = &I_icon_house_3d_34x45px;
        _generic_icon.width = 34;
        _generic_icon.height = 45;
        return &_generic_icon;
    }
    if(strcmp(name, "info") == 0) {
        _generic_icon.icon = &I_icon_info_15x16;
        _generic_icon.width = 15;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "man") == 0) {
        _generic_icon.icon = &I_icon_man_7x16;
        _generic_icon.width = 7;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "plant") == 0) {
        _generic_icon.icon = &I_icon_plant_16x16;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "plant_fern") == 0) {
        _generic_icon.icon = &I_icon_plant_fern_18x16px;
        _generic_icon.width = 18;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "plant_pointy") == 0) {
        _generic_icon.icon = &I_icon_plant_pointy_13x16px;
        _generic_icon.width = 13;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "tree") == 0) {
        _generic_icon.icon = &I_icon_tree_16x16;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "tree_29x30") == 0) {
        _generic_icon.icon = &I_icon_tree_29x30px;
        _generic_icon.width = 29;
        _generic_icon.height = 30;
        return &_generic_icon;
    }
    if(strcmp(name, "tree_48x48") == 0) {
        _generic_icon.icon = &I_icon_tree_48x48px;
        _generic_icon.width = 48;
        _generic_icon.height = 48;
        return &_generic_icon;
    }
    if(strcmp(name, "woman") == 0) {
        _generic_icon.icon = &I_icon_woman_9x16;
        _generic_icon.width = 9;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "chest_closed") == 0) {
        _generic_icon.icon = &I_icon_chest_closed_16x13px;
        _generic_icon.width = 16;
        _generic_icon.height = 13;
        return &_generic_icon;
    }
    if(strcmp(name, "chest_open") == 0) {
        _generic_icon.icon = &I_icon_chest_open_16x16px;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "fence") == 0) {
        _generic_icon.icon = &I_icon_fence_16x8px;
        _generic_icon.width = 16;
        _generic_icon.height = 8;
        return &_generic_icon;
    }
    if(strcmp(name, "fence_end") == 0) {
        _generic_icon.icon = &I_icon_fence_end_16x8px;
        _generic_icon.width = 16;
        _generic_icon.height = 8;
        return &_generic_icon;
    }
    if(strcmp(name, "fence_vertical_end") == 0) {
        _generic_icon.icon = &I_icon_fence_vertical_end_6x8px;
        _generic_icon.width = 6;
        _generic_icon.height = 8;
        return &_generic_icon;
    }
    if(strcmp(name, "fence_vertical_start") == 0) {
        _generic_icon.icon = &I_icon_fence_vertical_start_6x15px;
        _generic_icon.width = 6;
        _generic_icon.height = 15;
        return &_generic_icon;
    }
    if(strcmp(name, "flower") == 0) {
        _generic_icon.icon = &I_icon_flower_16x16;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_bottom") == 0) {
        _generic_icon.icon = &I_icon_lake_bottom_31x12px;
        _generic_icon.width = 31;
        _generic_icon.height = 12;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_bottom_left") == 0) {
        _generic_icon.icon = &I_icon_lake_bottom_left_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_bottom_right") == 0) {
        _generic_icon.icon = &I_icon_lake_bottom_right_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_left") == 0) {
        _generic_icon.icon = &I_icon_lake_left_11x31px;
        _generic_icon.width = 11;
        _generic_icon.height = 31;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_right") == 0) {
        _generic_icon.icon = &I_icon_lake_right_11x31; // Assuming it's 11x31
        _generic_icon.width = 11;
        _generic_icon.height = 31;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_top") == 0) {
        _generic_icon.icon = &I_icon_lake_top_31x12px;
        _generic_icon.width = 31;
        _generic_icon.height = 12;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_top_left") == 0) {
        _generic_icon.icon = &I_icon_lake_top_left_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(strcmp(name, "lake_top_right") == 0) {
        _generic_icon.icon = &I_icon_lake_top_right_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(strcmp(name, "rock_large") == 0) {
        _generic_icon.icon = &I_icon_rock_large_18x19px;
        _generic_icon.width = 18;
        _generic_icon.height = 19;
        return &_generic_icon;
    }
    if(strcmp(name, "rock_medium") == 0) {
        _generic_icon.icon = &I_icon_rock_medium_16x14px;
        _generic_icon.width = 16;
        _generic_icon.height = 14;
        return &_generic_icon;
    }
    if(strcmp(name, "rock_small") == 0) {
        _generic_icon.icon = &I_icon_rock_small_10x8px;
        _generic_icon.width = 10;
        _generic_icon.height = 8;
        return &_generic_icon;
    }

    // If no match is found
    return NULL;
}

IconContext* get_icon_context_furi(FuriString* name) {
    if(furi_string_cmp(name, "earth") == 0) {
        _generic_icon.icon = &I_icon_earth_15x16;
        _generic_icon.width = 15;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "home") == 0) {
        _generic_icon.icon = &I_icon_home_15x16;
        _generic_icon.width = 15;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "house") == 0) {
        _generic_icon.icon = &I_icon_house_48x32px;
        _generic_icon.width = 48;
        _generic_icon.height = 32;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "house_3d") == 0) {
        _generic_icon.icon = &I_icon_house_3d_34x45px;
        _generic_icon.width = 34;
        _generic_icon.height = 45;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "info") == 0) {
        _generic_icon.icon = &I_icon_info_15x16;
        _generic_icon.width = 15;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "man") == 0) {
        _generic_icon.icon = &I_icon_man_7x16;
        _generic_icon.width = 7;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "plant") == 0) {
        _generic_icon.icon = &I_icon_plant_16x16;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "plant_fern") == 0) {
        _generic_icon.icon = &I_icon_plant_fern_18x16px;
        _generic_icon.width = 18;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "plant_pointy") == 0) {
        _generic_icon.icon = &I_icon_plant_pointy_13x16px;
        _generic_icon.width = 13;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "tree") == 0) {
        _generic_icon.icon = &I_icon_tree_16x16;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "tree_29x30") == 0) {
        _generic_icon.icon = &I_icon_tree_29x30px;
        _generic_icon.width = 29;
        _generic_icon.height = 30;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "tree_48x48") == 0) {
        _generic_icon.icon = &I_icon_tree_48x48px;
        _generic_icon.width = 48;
        _generic_icon.height = 48;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "woman") == 0) {
        _generic_icon.icon = &I_icon_woman_9x16;
        _generic_icon.width = 9;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "chest_closed") == 0) {
        _generic_icon.icon = &I_icon_chest_closed_16x13px;
        _generic_icon.width = 16;
        _generic_icon.height = 13;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "chest_open") == 0) {
        _generic_icon.icon = &I_icon_chest_open_16x16px;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "fence") == 0) {
        _generic_icon.icon = &I_icon_fence_16x8px;
        _generic_icon.width = 16;
        _generic_icon.height = 8;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "fence_end") == 0) {
        _generic_icon.icon = &I_icon_fence_end_16x8px;
        _generic_icon.width = 16;
        _generic_icon.height = 8;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "fence_vertical_end") == 0) {
        _generic_icon.icon = &I_icon_fence_vertical_end_6x8px;
        _generic_icon.width = 6;
        _generic_icon.height = 8;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "fence_vertical_start") == 0) {
        _generic_icon.icon = &I_icon_fence_vertical_start_6x15px;
        _generic_icon.width = 6;
        _generic_icon.height = 15;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "flower") == 0) {
        _generic_icon.icon = &I_icon_flower_16x16;
        _generic_icon.width = 16;
        _generic_icon.height = 16;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_bottom") == 0) {
        _generic_icon.icon = &I_icon_lake_bottom_31x12px;
        _generic_icon.width = 31;
        _generic_icon.height = 12;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_bottom_left") == 0) {
        _generic_icon.icon = &I_icon_lake_bottom_left_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_bottom_right") == 0) {
        _generic_icon.icon = &I_icon_lake_bottom_right_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_left") == 0) {
        _generic_icon.icon = &I_icon_lake_left_11x31px;
        _generic_icon.width = 11;
        _generic_icon.height = 31;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_right") == 0) {
        _generic_icon.icon = &I_icon_lake_right_11x31; // Assuming dimensions
        _generic_icon.width = 11;
        _generic_icon.height = 31;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_top") == 0) {
        _generic_icon.icon = &I_icon_lake_top_31x12px;
        _generic_icon.width = 31;
        _generic_icon.height = 12;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_top_left") == 0) {
        _generic_icon.icon = &I_icon_lake_top_left_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "lake_top_right") == 0) {
        _generic_icon.icon = &I_icon_lake_top_right_24x22px;
        _generic_icon.width = 24;
        _generic_icon.height = 22;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "rock_large") == 0) {
        _generic_icon.icon = &I_icon_rock_large_18x19px;
        _generic_icon.width = 18;
        _generic_icon.height = 19;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "rock_medium") == 0) {
        _generic_icon.icon = &I_icon_rock_medium_16x14px;
        _generic_icon.width = 16;
        _generic_icon.height = 14;
        return &_generic_icon;
    }
    if(furi_string_cmp(name, "rock_small") == 0) {
        _generic_icon.icon = &I_icon_rock_small_10x8px;
        _generic_icon.width = 10;
        _generic_icon.height = 8;
        return &_generic_icon;
    }

    // If no match is found
    return NULL;
}
