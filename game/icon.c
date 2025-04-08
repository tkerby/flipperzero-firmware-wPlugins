#include "game/icon.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --- Define the global variable so other modules can link to it.
IconGroupContext *g_current_icon_group = NULL;

// ---------------------------------------------------------------------
// Icon Group Entity Implementation
// ---------------------------------------------------------------------

// Render callback: iterate over the group and draw each icon.
static void icon_group_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    IconGroupContext *igctx = (IconGroupContext *)context;
    if (!igctx)
    {
        FURI_LOG_E("Game", "Icon group context is NULL in render");
        return;
    }
    for (int i = 0; i < igctx->count; i++)
    {
        IconSpec *spec = &igctx->icons[i];
        int x_pos = spec->pos.x - camera_x - (spec->size.x / 2);
        int y_pos = spec->pos.y - camera_y - (spec->size.y / 2);
        if (x_pos + spec->size.x < 0 || x_pos > SCREEN_WIDTH ||
            y_pos + spec->size.y < 0 || y_pos > SCREEN_HEIGHT)
        {
            continue;
        }
        canvas_draw_icon(canvas, x_pos, y_pos, spec->icon);
    }
}

// Start callback: nothing special is needed here.
static void icon_group_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    UNUSED(context);
    // The context is assumed to be pre-filled by the world code.
}

// Free callback: free the icon specs array.
static void icon_group_free(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    IconGroupContext *igctx = (IconGroupContext *)context;
    if (igctx && igctx->icons)
    {
        free(igctx->icons);
        igctx->icons = NULL;
    }
}

// Optional helper to free an icon group context allocated dynamically.
void icon_group_context_free(IconGroupContext *ctx)
{
    if (ctx)
    {
        if (ctx->icons)
        {
            free(ctx->icons);
            ctx->icons = NULL;
        }
        free(ctx);
    }
}

// The entity description for the icon group.
// Note: we set context_size to sizeof(IconGroupContext).
const EntityDescription icon_desc = {
    .start = icon_group_start,
    .stop = icon_group_free,
    .update = NULL,
    .render = icon_group_render,
    // We leave collision as NULL because we now handle collisions in player_update.
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(IconGroupContext),
};
