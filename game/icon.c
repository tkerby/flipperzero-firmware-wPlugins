#include "game/icon.h"
const Icon *get_icon(char *name)
{
    if (strcmp(name, "earth") == 0)
    {
        return &I_icon_earth;
    }
    if (strcmp(name, "home") == 0)
    {
        return &I_icon_home;
    }
    if (strcmp(name, "info") == 0)
    {
        return &I_icon_info;
    }
    if (strcmp(name, "man") == 0)
    {
        return &I_icon_man;
    }
    if (strcmp(name, "plant") == 0)
    {
        return &I_icon_plant;
    }
    if (strcmp(name, "tree") == 0)
    {
        return &I_icon_tree;
    }
    if (strcmp(name, "woman") == 0)
    {
        return &I_icon_woman;
    }
    return NULL;
}
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