#include "context_menu.h"

#include <stddef.h>

#include <furi.h>
#include <gui/modules/variable_item_list.h>

#include "../../engine/game_manager.h"
#include "../../gui_bridge/input_converter.h"

// #include "../../game.h"
// #include "../../game_settings.h"

typedef struct {
    InputConverter* input_converter;

    ContextMenuBackCallback back_callback;
    void* back_callback_context;

    size_t current_index;

} ContextMenuContext;

void context_menu_back_callback_set(
    Entity* self,
    ContextMenuBackCallback back_callback,
    void* callback_context) {
    ContextMenuContext* entity_context = entity_context_get(self);
    entity_context->back_callback = back_callback;
    entity_context->back_callback_context = callback_context;
}

static void context_menu_start(Entity* self, GameManager* manager, void* _entity_context) {
    UNUSED(self);
    UNUSED(manager);

    ContextMenuContext* entity_context = _entity_context;

    // Alloc converter
    entity_context->input_converter = input_converter_alloc();

    // Callback
    entity_context->back_callback = NULL;
    entity_context->back_callback_context = NULL;
}

static void context_menu_stop(Entity* self, GameManager* manager, void* _entity_context) {
    UNUSED(self);
    UNUSED(manager);

    ContextMenuContext* entity_context = _entity_context;
    input_converter_free(entity_context->input_converter);
}

static void context_menu_update(Entity* self, GameManager* manager, void* _entity_context) {
    UNUSED(self);
    ContextMenuContext* entity_context = _entity_context;

    InputState input = game_manager_input_get(manager);
    input_converter_process_state(entity_context->input_converter, &input);

    InputEvent event;
    while(input_converter_get_event(entity_context->input_converter, &event) == FuriStatusOk) {
        if(event.key == InputKeyBack && event.type == InputTypeShort &&
           entity_context->back_callback != NULL) {
            entity_context->back_callback(entity_context->back_callback_context);
            return;
        }
    }
}

static void gray_canvas(Canvas* const canvas) {
    // canvas_set_color(canvas, ColorWhite);
    canvas_clear(canvas);
    for(size_t x = 0; x < 128; x += 2) {
        for(size_t y = 0; y < 64; y++) {
            canvas_draw_dot(canvas, x + (y % 2 == 1 ? 0 : 1), y);
        }
    }
    // canvas_set_color(canvas, ColorBlack);
}

static void
    context_menu_render(Entity* self, GameManager* manager, Canvas* canvas, void* _entity_context) {
    UNUSED(self);
    UNUSED(manager);
    UNUSED(_entity_context);

    gray_canvas(canvas);
}

const EntityDescription context_menu_description = {
    .start = context_menu_start,
    .stop = context_menu_stop,
    .update = context_menu_update,
    .render = context_menu_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(ContextMenuContext),
};
