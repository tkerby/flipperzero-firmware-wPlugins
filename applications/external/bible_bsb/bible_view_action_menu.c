#include "bible_view_action_menu.h"
#include "bible_renderer.h"
#include "bible_storage.h"
#include "bible_url.h"
#include "bible_nfc.h"

#include <string.h>

/* ============================================================================
 * ActionMenu layout constants
 * ============================================================================ */
#define ACTION_MENU_HEADER_Y  10
#define ACTION_MENU_DIVIDER_Y 12
#define ACTION_MENU_ITEM_H    14
#define ACTION_MENU_START_Y   24

static const char* ACTION_MENU_ITEMS[] = {
    "Save to collection",
    "Share via NFC",
    "Back to reading",
};

#define ACTION_MENU_ITEM_COUNT 3

/* ============================================================================
 * ActionMenu view — draw callback
 * ============================================================================ */
void bible_bsb_view_action_menu_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    BibleAppState* state = app->state;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, BIBLE_MARGIN_X, ACTION_MENU_HEADER_Y, "Menu");
    canvas_draw_line(canvas, 0, ACTION_MENU_DIVIDER_Y, BIBLE_SCREEN_WIDTH, ACTION_MENU_DIVIDER_Y);

    for(uint8_t i = 0; i < ACTION_MENU_ITEM_COUNT; i++) {
        int16_t y = ACTION_MENU_START_Y + ((int16_t)i * ACTION_MENU_ITEM_H);
        bool is_selected = (i == state->action_menu_selection);

        if(is_selected) {
            canvas_draw_box(canvas, 0, y - 11, BIBLE_SCREEN_WIDTH, ACTION_MENU_ITEM_H);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, 4, y, ACTION_MENU_ITEMS[i]);

        if(is_selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

/* ============================================================================
 * ActionMenu view — input callback
 * ============================================================================ */
bool bible_bsb_view_action_menu_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;
    BibleAppState* state = app->state;

    /* Only process Press, Repeat, Short, Long — ignore Release */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Clamp: do not wrap past the first item. */
        if(state->action_menu_selection > 0) {
            state->action_menu_selection -= 1;
        }
        consumed = true;

    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        if(state->action_menu_selection < (ACTION_MENU_ITEM_COUNT - 1)) {
            state->action_menu_selection += 1;
        }
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        switch(state->action_menu_selection) {
        case 0: /* Save to collection */
            bible_save_passage(state);
            scene_manager_previous_scene(app->scene_manager);
            break;

        case 1: /* Share via NFC */
            if(state->passage.verse_count > 0) {
                if(bible_build_route_url(&state->passage, state->nfc_url, sizeof(state->nfc_url))) {
                    if(bible_nfc_start_url(state)) {
                        scene_manager_next_scene(app->scene_manager, BibleSceneNfcShare);
                    } else {
                        bible_toast_set(&state->toast, "NFC failed");
                        scene_manager_previous_scene(app->scene_manager);
                    }
                } else {
                    bible_toast_set(&state->toast, "URL failed");
                    scene_manager_previous_scene(app->scene_manager);
                }
            } else {
                bible_toast_set(&state->toast, "No passage");
                scene_manager_previous_scene(app->scene_manager);
            }
            break;

        case 2: /* Back to reading */
        default:
            scene_manager_previous_scene(app->scene_manager);
            break;
        }
        consumed = true;

    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    bible_app_request_redraw(app);
    return consumed;
}
