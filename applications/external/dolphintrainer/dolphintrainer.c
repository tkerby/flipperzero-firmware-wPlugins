#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include "dolphin_trainer_icons.h"
#include <dolphin/helpers/dolphin_state.h>
#include <toolbox/saved_struct.h>
#include <power/power_service/power.h>
#include "tinyfont.h"

const char* funnyText[] = {
    "\"Stop poking my brain\"",
    "\"You're a terrible owner\"",
    "\"I'll remember this!\"",
    "\"This really isnt ok\"",
    "\"Just feed me RFID cards!\"",
    "\"Forget to charge me too?\"",
    "\"Asshole...\""};

int funnyTextIndex = 0;

DolphinState* stateLocal = 0;
char strButthurt[16];
char strXp[16];
char strLevel[10];
int btnIndex = 0;
uint32_t curLevel = 0;

const uint32_t DOLPHIN_LEVELS[] = {500,    1250,   2250,   3500,   5000,  6750,  8750,  11000,
                                   13500,  16250,  19250,  22500,  26000, 29750, 33750, 38000,
                                   42500,  47250,  52250,  58250,  65250, 73250, 82250, 92250,
                                   103250, 115250, 128250, 142250, 157250};
const size_t DOLPHIN_LEVEL_COUNT = COUNT_OF(DOLPHIN_LEVELS);

#define NUM(a) (sizeof(a) / sizeof(*a))

uint8_t dolphin_get_mylevel(uint32_t icounter) {
    for(size_t i = 0; i < DOLPHIN_LEVEL_COUNT; ++i) {
        if(icounter <= DOLPHIN_LEVELS[i]) {
            return i + 1;
        }
    }
    return DOLPHIN_LEVEL_COUNT + 1;
}

int yOffset = 9;

static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    //graphics
    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_icon(canvas, 3, 3 + yOffset, &I_passport_bad1_46x49);
    if(btnIndex == 0) {
        canvas_draw_icon(canvas, 120, 9 + yOffset, &I_ButtonLeftSmall_3x5);
    } else if(btnIndex == 1) {
        canvas_draw_icon(canvas, 120, 19 + yOffset, &I_ButtonLeftSmall_3x5);
    } else if(btnIndex == 2) {
        canvas_draw_icon(canvas, 120, 29 + yOffset, &I_ButtonLeftSmall_3x5);
    }

    //strings
    curLevel = dolphin_get_mylevel(stateLocal->data.icounter);
    canvas_set_custom_u8g2_font(canvas, u8g2_font_5x7_tf);
    canvas_draw_str(canvas, 3, 9, funnyText[funnyTextIndex]);

    canvas_set_font(canvas, FontSecondary);
    snprintf(strLevel, 10, "Level: %lu", curLevel);
    snprintf(strButthurt, 16, "Butthurt: %lu", stateLocal->data.butthurt);
    snprintf(strXp, 16, "XP: %lu", stateLocal->data.icounter);
    canvas_draw_str(canvas, 51, 15 + yOffset, strLevel);
    canvas_draw_str(canvas, 51, 25 + yOffset, strButthurt);
    canvas_draw_str(canvas, 51, 35 + yOffset, strXp);

    if(btnIndex == 3) {
        //save button
        canvas_draw_rbox(canvas, 51, 37 + yOffset, 74, 15, 3);
        canvas_invert_color(canvas);
        canvas_draw_str_aligned(
            canvas, 88, 45 + yOffset, AlignCenter, AlignCenter, "Save & Reboot");
        canvas_invert_color(canvas);
        canvas_draw_rframe(canvas, 51, 37 + yOffset, 74, 15, 3);
    } else {
        canvas_invert_color(canvas);
        canvas_draw_rbox(canvas, 51, 37 + yOffset, 74, 15, 3);
        canvas_invert_color(canvas);
        canvas_draw_str_aligned(
            canvas, 88, 45 + yOffset, AlignCenter, AlignCenter, "Save & Reboot");
        canvas_draw_rframe(canvas, 51, 37 + yOffset, 74, 15, 3);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t dolphin_trainer_app(void* p) {
    UNUSED(p);

    InputEvent event;
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    funnyTextIndex = rand() % 7;

    stateLocal = malloc(sizeof(DolphinState));

    bool running = true;
    bool success = saved_struct_load(
        DOLPHIN_STATE_PATH, &stateLocal->data, sizeof(DolphinStoreData), 0xD0, 0x01);
    if(!success) {
        running = false;
    }

    view_port_update(view_port);

    while(running) {
        furi_check(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk);
        view_port_update(view_port);
        if(event.type == InputTypePress) {
            if(event.key == InputKeyOk && btnIndex == 3) {
                bool result = saved_struct_save(
                    DOLPHIN_STATE_PATH, &stateLocal->data, sizeof(DolphinStoreData), 0xD0, 0x01);
                if(result) {
                    power_reboot(PowerBootModeNormal);
                    running = false;
                    return 0;
                }
            }
            if(event.key == InputKeyUp) {
                if(btnIndex == 0) {
                    btnIndex = 3;
                } else {
                    btnIndex--;
                }
            }
            if(event.key == InputKeyDown) {
                if(btnIndex == 3) {
                    btnIndex = 0;
                } else {
                    btnIndex++;
                }
            }
            if(event.key == InputKeyRight) {
                if(btnIndex == 0) {
                    curLevel = dolphin_get_mylevel(stateLocal->data.icounter) - 1;
                    if(curLevel <= 28) {
                        stateLocal->data.icounter = DOLPHIN_LEVELS[curLevel] + 1;
                    }
                } else if(btnIndex == 1 && stateLocal->data.butthurt < 14) {
                    stateLocal->data.butthurt++;
                } else if(btnIndex == 2) {
                    stateLocal->data.icounter += 10;
                }
            }
            if(event.key == InputKeyLeft) {
                if(btnIndex == 0) {
                    curLevel = dolphin_get_mylevel(stateLocal->data.icounter) - 3;
                    if(curLevel >= 1) {
                        stateLocal->data.icounter = DOLPHIN_LEVELS[curLevel] + 1;
                    } else if(curLevel == 0) {
                        stateLocal->data.icounter = 0;
                    }
                } else if(btnIndex == 1 && stateLocal->data.butthurt > 0) {
                    stateLocal->data.butthurt--;
                } else if(btnIndex == 2) {
                    if(stateLocal->data.icounter < 10) {
                        stateLocal->data.icounter = 0;
                    } else {
                        stateLocal->data.icounter -= 10;
                    }
                }
            }
            if(event.key == InputKeyBack) {
                running = false;
            }
        }
    }

    furi_message_queue_free(event_queue);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    return 0;
}
