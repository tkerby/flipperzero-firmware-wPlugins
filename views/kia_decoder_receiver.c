// views/kia_decoder_receiver.c
#include "kia_decoder_receiver.h"
#include "../kia_decoder_app_i.h"
#include <input/input.h>
#include <gui/elements.h>
#include <furi.h>

#define FRAME_HEIGHT 12
#define MAX_LEN_PX 112
#define MENU_ITEMS 4u
#define UNLOCK_CNT 3

typedef struct {
    FuriString* item_str;
    uint8_t type;
} KiaReceiverMenuItem;

ARRAY_DEF(KiaReceiverMenuItemArray, KiaReceiverMenuItem, M_POD_OPLIST)

struct KiaReceiver {
    View* view;
    KiaReceiverCallback callback;
    void* context;
};

typedef struct {
    KiaReceiverMenuItemArray_t history_item_arr;
    uint8_t list_offset;
    uint8_t history_item;
    float rssi;
    FuriString* frequency_str;
    FuriString* preset_str;
    FuriString* history_stat_str;
    bool external_radio;
    KiaLock lock;
    uint8_t lock_count;
} KiaReceiverModel;

void kia_view_receiver_set_rssi(KiaReceiver* receiver, float rssi) {
    furi_assert(receiver);
    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        { model->rssi = rssi; },
        true);
}

void kia_view_receiver_set_lock(KiaReceiver* receiver, KiaLock lock) {
    furi_assert(receiver);
    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        { model->lock = lock; },
        true);
}

void kia_view_receiver_set_callback(
    KiaReceiver* receiver,
    KiaReceiverCallback callback,
    void* context) {
    furi_assert(receiver);
    receiver->callback = callback;
    receiver->context = context;
}

static void kia_view_receiver_update_offset(KiaReceiver* receiver) {
    furi_assert(receiver);
    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        {
            size_t history_item = model->history_item;
            size_t list_offset = model->list_offset;
            size_t item_count = KiaReceiverMenuItemArray_size(model->history_item_arr);

            if(history_item < list_offset) {
                model->list_offset = history_item;
            } else if(history_item >= (list_offset + MENU_ITEMS)) {
                model->list_offset = history_item - (MENU_ITEMS - 1);
            }

            if(item_count < MENU_ITEMS) {
                model->list_offset = 0;
            } else if(model->list_offset > (item_count - MENU_ITEMS)) {
                model->list_offset = item_count - MENU_ITEMS;
            }
        },
        true);
}

void kia_view_receiver_add_item_to_menu(KiaReceiver* receiver, const char* name, uint8_t type) {
    furi_assert(receiver);
    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        {
            KiaReceiverMenuItem* item_menu =
                KiaReceiverMenuItemArray_push_raw(model->history_item_arr);
            item_menu->item_str = furi_string_alloc_set(name);
            item_menu->type = type;
        },
        true);
    kia_view_receiver_update_offset(receiver);
}

void kia_view_receiver_add_data_statusbar(
    KiaReceiver* receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external_radio) {
    furi_assert(receiver);
    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        {
            furi_string_set_str(model->frequency_str, frequency_str);
            furi_string_set_str(model->preset_str, preset_str);
            furi_string_set_str(model->history_stat_str, history_stat_str);
            model->external_radio = external_radio;
        },
        true);
}

static void kia_view_receiver_draw_frame(Canvas* canvas, uint16_t idx, bool scrollbar) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(
        canvas, 0, 0 + idx * FRAME_HEIGHT, scrollbar ? 122 : 127, FRAME_HEIGHT);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, 0, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 1, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 1);

    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 11);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, (0 + idx * FRAME_HEIGHT) + 11);
}

static void kia_view_receiver_draw_rssi(Canvas* canvas, KiaReceiverModel* model) {
    uint8_t x = 108;
    uint8_t y = 53;
    float rssi = model->rssi;

    if(rssi >= -60.0f) {
        canvas_draw_box(canvas, x + 8, y + 2, 4, 5);
    }
    if(rssi >= -70.0f) {
        canvas_draw_box(canvas, x + 4, y + 4, 4, 3);
    }
    if(rssi >= -80.0f) {
        canvas_draw_box(canvas, x, y + 6, 4, 1);
    }
}

void kia_view_receiver_draw(Canvas* canvas, KiaReceiverModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    size_t item_count = KiaReceiverMenuItemArray_size(model->history_item_arr);
    bool scrollbar = item_count > MENU_ITEMS;

    FuriString* str_buff;
    str_buff = furi_string_alloc();

    if(item_count > 0) {
        size_t shift_position = model->list_offset;

        for(size_t i = 0; i < MIN(item_count, MENU_ITEMS); i++) {
            size_t idx = shift_position + i;
            KiaReceiverMenuItem* item = KiaReceiverMenuItemArray_get(model->history_item_arr, idx);

            furi_string_set(str_buff, item->item_str);
            elements_string_fit_width(canvas, str_buff, scrollbar ? MAX_LEN_PX - 6 : MAX_LEN_PX);

            if(model->history_item == idx) {
                kia_view_receiver_draw_frame(canvas, i, scrollbar);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            canvas_draw_str(canvas, 4, 9 + (i * FRAME_HEIGHT), furi_string_get_cstr(str_buff));
        }

        if(scrollbar) {
            elements_scrollbar_pos(canvas, 128, 0, 49, shift_position, item_count);
        }
    } else {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "ProtoPirate");
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Waiting for signal...");
    }

    furi_string_free(str_buff);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 48, 127, 48);

    // Draw status bar
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 58, furi_string_get_cstr(model->frequency_str));
    canvas_draw_str(canvas, 40, 58, furi_string_get_cstr(model->preset_str));
    canvas_draw_str_aligned(
        canvas, 90, 58, AlignCenter, AlignBottom, furi_string_get_cstr(model->history_stat_str));

    // Draw RSSI
    kia_view_receiver_draw_rssi(canvas, model);

    // Draw external radio indicator if needed
    if(model->external_radio) {
        canvas_draw_str(canvas, 100, 58, "Ext");
    }

    // Draw lock indicator
    if(model->lock == KiaLockOn) {
        canvas_draw_str(canvas, 119, 58, "L");
    }
}

bool kia_view_receiver_input(InputEvent* event, void* context) {
    furi_assert(context);
    KiaReceiver* receiver = context;

    bool consumed = false;

    KiaLock lock;
    with_view_model(
        receiver->view, KiaReceiverModel * model, { lock = model->lock; }, false);

    if(lock == KiaLockOn) {
        with_view_model(
            receiver->view,
            KiaReceiverModel * model,
            {
                if(event->type == InputTypeShort && event->key == InputKeyBack) {
                    model->lock_count++;
                    if(model->lock_count >= UNLOCK_CNT) {
                        model->lock = KiaLockOff;
                        model->lock_count = 0;
                        if(receiver->callback) {
                            receiver->callback(KiaCustomEventViewReceiverUnlock, receiver->context);
                        }
                    }
                } else {
                    model->lock_count = 0;
                }
            },
            true);
        consumed = true;
    } else if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                receiver->view,
                KiaReceiverModel * model,
                {
                    if(model->history_item > 0) {
                        model->history_item--;
                    }
                },
                true);
            kia_view_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyDown:
            with_view_model(
                receiver->view,
                KiaReceiverModel * model,
                {
                    size_t item_count = KiaReceiverMenuItemArray_size(model->history_item_arr);
                    if(item_count > 0 && model->history_item < item_count - 1) {
                        model->history_item++;
                    }
                },
                true);
            kia_view_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyLeft:
            if(receiver->callback) {
                receiver->callback(KiaCustomEventViewReceiverConfig, receiver->context);
            }
            consumed = true;
            break;
        case InputKeyRight:
            consumed = true;
            break;
        case InputKeyOk:
            with_view_model(
                receiver->view,
                KiaReceiverModel * model,
                {
                    size_t item_count = KiaReceiverMenuItemArray_size(model->history_item_arr);
                    if(item_count > 0) {
                        if(receiver->callback) {
                            receiver->callback(KiaCustomEventViewReceiverOK, receiver->context);
                        }
                    }
                },
                false);
            consumed = true;
            break;
        case InputKeyBack:
            if(receiver->callback) {
                receiver->callback(KiaCustomEventViewReceiverBack, receiver->context);
            }
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void kia_view_receiver_enter(void* context) {
    furi_assert(context);
    UNUSED(context);
}

void kia_view_receiver_exit(void* context) {
    furi_assert(context);
    KiaReceiver* receiver = context;
    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        {
            for(size_t i = 0; i < KiaReceiverMenuItemArray_size(model->history_item_arr); i++) {
                KiaReceiverMenuItem* item = KiaReceiverMenuItemArray_get(model->history_item_arr, i);
                furi_string_free(item->item_str);
            }
            KiaReceiverMenuItemArray_reset(model->history_item_arr);
            model->history_item = 0;
            model->list_offset = 0;
        },
        false);
}

KiaReceiver* kia_view_receiver_alloc() {
    KiaReceiver* receiver = malloc(sizeof(KiaReceiver));

    receiver->view = view_alloc();
    view_allocate_model(receiver->view, ViewModelTypeLocking, sizeof(KiaReceiverModel));
    view_set_context(receiver->view, receiver);
    view_set_draw_callback(receiver->view, (ViewDrawCallback)kia_view_receiver_draw);
    view_set_input_callback(receiver->view, kia_view_receiver_input);
    view_set_enter_callback(receiver->view, kia_view_receiver_enter);
    view_set_exit_callback(receiver->view, kia_view_receiver_exit);

    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        {
            KiaReceiverMenuItemArray_init(model->history_item_arr);
            model->frequency_str = furi_string_alloc();
            model->preset_str = furi_string_alloc();
            model->history_stat_str = furi_string_alloc();
            model->list_offset = 0;
            model->history_item = 0;
            model->rssi = -127.0f;
            model->external_radio = false;
            model->lock = KiaLockOff;
            model->lock_count = 0;
        },
        true);

    return receiver;
}

void kia_view_receiver_free(KiaReceiver* receiver) {
    furi_assert(receiver);

    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        {
            for(size_t i = 0; i < KiaReceiverMenuItemArray_size(model->history_item_arr); i++) {
                KiaReceiverMenuItem* item = KiaReceiverMenuItemArray_get(model->history_item_arr, i);
                furi_string_free(item->item_str);
            }
            KiaReceiverMenuItemArray_clear(model->history_item_arr);
            furi_string_free(model->frequency_str);
            furi_string_free(model->preset_str);
            furi_string_free(model->history_stat_str);
        },
        false);

    view_free(receiver->view);
    free(receiver);
}

View* kia_view_receiver_get_view(KiaReceiver* receiver) {
    furi_assert(receiver);
    return receiver->view;
}

uint16_t kia_view_receiver_get_idx_menu(KiaReceiver* receiver) {
    furi_assert(receiver);
    uint16_t idx = 0;
    with_view_model(
        receiver->view, KiaReceiverModel * model, { idx = model->history_item; }, false);
    return idx;
}

void kia_view_receiver_set_idx_menu(KiaReceiver* receiver, uint16_t idx) {
    furi_assert(receiver);
    with_view_model(
        receiver->view,
        KiaReceiverModel * model,
        {
            model->history_item = idx;
            size_t item_count = KiaReceiverMenuItemArray_size(model->history_item_arr);
            if(model->history_item >= item_count) {
                model->history_item = item_count > 0 ? item_count - 1 : 0;
            }
        },
        true);
    kia_view_receiver_update_offset(receiver);
}