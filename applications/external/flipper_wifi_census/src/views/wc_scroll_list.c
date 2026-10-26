#include "include/views/wc_scroll_list.h"

#include <furi.h>
#include <gui/elements.h>

// How often the selected item's text advances one step. Matches the firmware's own
// VariableItemList so the two read as the same motion.
#define WC_SCROLL_LIST_TICK_MS 333

typedef struct {
    const char *label; // NULL -> the list's label_fn writes it while drawing
    uint32_t index;
    WcScrollListCb callback;
    void *callback_context;
} WcScrollListItem;

typedef struct {
    WcScrollListItem items[WC_SCROLL_LIST_MAX];
    size_t count;
    size_t position;
    size_t window_position;
    size_t scroll_counter;
    uint16_t hidden;     // entries not shown, refused here or by the caller
    int32_t value_index; // item carrying a right-hand value, or -1
    const char *value;   // borrowed, like a label
    WcScrollListNudgeFn nudge;
    void *nudge_ctx;
    WcScrollListLabelFn label_fn;
    void *label_ctx;
    bool selected_overflows; // set while drawing: whether the selected label needs to move
    FuriString *header;
    FuriString *scratch;   // the firmware element takes a FuriString, the labels are char[]
    FuriString *value_str; // ditto, for the right-hand value
} WcScrollListModel;

struct WcScrollList {
    View *view;
    FuriTimer *scroll_timer;
};

// The header row also carries the hidden count, so it exists whenever either does.
static bool has_header_row(const WcScrollListModel *model) {
    return !furi_string_empty(model->header) || model->hidden > 0;
}

static size_t items_on_screen(const WcScrollListModel *model) {
    return has_header_row(model) ? 3 : 4;
}

static void wc_scroll_list_draw_callback(Canvas *canvas, void *_model) {
    WcScrollListModel *model = _model;
    const uint8_t item_height = 16;
    const uint8_t item_width = canvas_width(canvas) - 5;

    canvas_clear(canvas);

    if (has_header_row(model)) {
        canvas_set_font(canvas, FontPrimary);
        if (model->hidden > 0) {
            furi_string_printf(model->scratch, "%s +%u", furi_string_get_cstr(model->header),
                               model->hidden);
        } else {
            furi_string_set(model->scratch, model->header);
        }
        elements_scrollable_text_line(canvas, 4, 11, canvas_width(canvas) - 8, model->scratch, 0,
                                      true);
    }

    canvas_set_font(canvas, FontSecondary);

    const size_t on_screen = items_on_screen(model);
    const uint8_t y_offset = has_header_row(model) ? item_height : 0;

    for (size_t position = 0; position < model->count; position++) {
        const size_t item_position = position - model->window_position;
        if (item_position >= on_screen) {
            continue;
        }
        const uint8_t item_y = y_offset + (uint8_t)(item_position * item_height);
        const bool selected = (position == model->position);
        size_t scroll = 0;

        if (selected) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 0, item_y + 1, item_width, item_height - 2);
            canvas_set_color(canvas, ColorWhite);
            // Hold on the start of the text for one extra tick before it starts moving.
            scroll = (model->scroll_counter < 1) ? 0 : model->scroll_counter - 1;
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        const WcScrollListItem *item = &model->items[position];
        if (item->label) {
            furi_string_set_str(model->scratch, item->label);
        } else {
            char generated[WC_SCROLL_LIST_LABEL_MAX] = {0};
            if (model->label_fn) {
                model->label_fn(model->label_ctx, item->index, generated, sizeof(generated));
            }
            furi_string_set_str(model->scratch, generated);
        }
        const size_t text_width = item_width - 11;
        if (selected) {
            model->selected_overflows =
                canvas_string_width(canvas, furi_string_get_cstr(model->scratch)) > text_width;
        }
        const uint8_t text_y = item_y + item_height - 4;
        if (model->value && model->value_index >= 0 &&
            item->index == (uint32_t)model->value_index) {
            // Label left, value right: the option is decided on the row that uses it.
            furi_string_printf(model->value_str, "%s%s%s", selected ? "<" : " ", model->value,
                               selected ? ">" : " ");
            const uint16_t vw = canvas_string_width(canvas, furi_string_get_cstr(model->value_str));
            canvas_draw_str(canvas, (int32_t)(item_width - 3 - vw), text_y,
                            furi_string_get_cstr(model->value_str));
            elements_scrollable_text_line(canvas, 6, text_y, text_width - vw - 4, model->scratch,
                                          scroll, !selected);
        } else {
            elements_scrollable_text_line(canvas, 6, text_y, text_width, model->scratch, scroll,
                                          !selected);
        }
    }

    elements_scrollbar(canvas, model->position, model->count);
}

static void wc_scroll_list_process_up(WcScrollList *list) {
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            const size_t on_screen = items_on_screen(model);
            if (model->position > 0) {
                model->position--;
                if ((model->position == model->window_position) && (model->window_position > 0)) {
                    model->window_position--;
                }
            } else {
                model->position = model->count - 1;
                if (model->position > on_screen - 1) {
                    model->window_position = model->position - (on_screen - 1);
                }
            }
            model->scroll_counter = 0;
        },
        true);
}

static void wc_scroll_list_process_down(WcScrollList *list) {
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            const size_t on_screen = items_on_screen(model);
            if (model->position < model->count - 1) {
                model->position++;
                if ((model->position - model->window_position > on_screen - 2) &&
                    (model->window_position < model->count - on_screen)) {
                    model->window_position++;
                }
            } else {
                model->position = 0;
                model->window_position = 0;
            }
            model->scroll_counter = 0;
        },
        true);
}

// Left/right adjust the selected row's value, when it has one. The scene redraws by updating
// whatever the value points at, so the view only has to ask.
static bool wc_scroll_list_process_nudge(WcScrollList *list, int8_t delta) {
    WcScrollListNudgeFn nudge = NULL;
    void *ctx = NULL;
    uint32_t index = 0;
    with_view_model(
        list->view, const WcScrollListModel *model,
        {
            if (model->nudge && model->value_index >= 0 && model->position < model->count &&
                model->items[model->position].index == (uint32_t)model->value_index) {
                nudge = model->nudge;
                ctx = model->nudge_ctx;
                index = model->items[model->position].index;
            }
        },
        false);
    if (!nudge) {
        return false;
    }
    nudge(ctx, index, delta);
    with_view_model(list->view, WcScrollListModel * model, { model->scroll_counter = 0; }, true);
    return true;
}

static void wc_scroll_list_process_ok(WcScrollList *list) {
    WcScrollListCb callback = NULL;
    void *context = NULL;
    uint32_t index = 0;

    with_view_model(
        list->view, WcScrollListModel * model,
        {
            if (model->position < model->count) {
                callback = model->items[model->position].callback;
                context = model->items[model->position].callback_context;
                index = model->items[model->position].index;
            }
        },
        false);

    if (callback) {
        callback(context, index);
    }
}

// cppcheck-suppress constParameterCallback // the firmware's ViewInputCallback is not const
static bool wc_scroll_list_input_callback(InputEvent *event, void *context) {
    WcScrollList *list = context;
    furi_assert(list);

    size_t count = 0;
    with_view_model(list->view, const WcScrollListModel *model, { count = model->count; }, false);
    if (count == 0) {
        return false;
    }

    bool consumed = false;
    if (event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if (event->key == InputKeyUp) {
            wc_scroll_list_process_up(list);
            consumed = true;
        } else if (event->key == InputKeyDown) {
            wc_scroll_list_process_down(list);
            consumed = true;
        } else if (event->key == InputKeyLeft) {
            consumed = wc_scroll_list_process_nudge(list, -1);
        } else if (event->key == InputKeyRight) {
            consumed = wc_scroll_list_process_nudge(list, +1);
        }
    }
    if (!consumed && event->key == InputKeyOk && event->type == InputTypeShort) {
        wc_scroll_list_process_ok(list);
        consumed = true;
    }
    return consumed;
}

static void wc_scroll_list_timer_callback(void *context) {
    WcScrollList *list = context;
    bool overflows = false;
    with_view_model(
        list->view, const WcScrollListModel *model, { overflows = model->selected_overflows; },
        false);
    if (!overflows) {
        return; // a menu where every row fits must not repaint three times a second
    }
    with_view_model(list->view, WcScrollListModel * model, { model->scroll_counter++; }, true);
}

// The timer only runs while the list is the visible view: a list off screen has nothing to
// animate, and a periodic timer left running keeps waking the CPU.
static void wc_scroll_list_enter_callback(void *context) {
    WcScrollList *list = context;
    with_view_model(list->view, WcScrollListModel * model, { model->scroll_counter = 0; }, true);
    furi_timer_start(list->scroll_timer, furi_ms_to_ticks(WC_SCROLL_LIST_TICK_MS));
}

static void wc_scroll_list_exit_callback(void *context) {
    WcScrollList *list = context;
    furi_timer_stop(list->scroll_timer);
}

WcScrollList *wc_scroll_list_alloc(void) {
    WcScrollList *list = malloc(sizeof(WcScrollList));
    list->view = view_alloc();
    view_set_context(list->view, list);
    view_allocate_model(list->view, ViewModelTypeLocking, sizeof(WcScrollListModel));
    view_set_draw_callback(list->view, wc_scroll_list_draw_callback);
    view_set_input_callback(list->view, wc_scroll_list_input_callback);
    view_set_enter_callback(list->view, wc_scroll_list_enter_callback);
    view_set_exit_callback(list->view, wc_scroll_list_exit_callback);

    list->scroll_timer =
        furi_timer_alloc(wc_scroll_list_timer_callback, FuriTimerTypePeriodic, list);

    with_view_model(
        list->view, WcScrollListModel * model,
        {
            model->count = 0;
            model->position = 0;
            model->window_position = 0;
            model->scroll_counter = 0;
            model->selected_overflows = false;
            model->hidden = 0;
            model->label_fn = NULL;
            model->label_ctx = NULL;
            model->header = furi_string_alloc();
            model->scratch = furi_string_alloc();
            model->value_str = furi_string_alloc();
            model->value_index = -1;
        },
        true);
    return list;
}

void wc_scroll_list_free(WcScrollList *list) {
    furi_check(list);
    furi_timer_stop(list->scroll_timer);
    furi_timer_free(list->scroll_timer);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            furi_string_free(model->header);
            furi_string_free(model->scratch);
            furi_string_free(model->value_str);
        },
        false);
    view_free(list->view);
    free(list);
}

View *wc_scroll_list_get_view(WcScrollList *list) {
    furi_check(list);
    return list->view;
}

void wc_scroll_list_reset(WcScrollList *list) {
    furi_check(list);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            model->count = 0;
            model->position = 0;
            model->window_position = 0;
            model->scroll_counter = 0;
            model->hidden = 0;
            model->label_fn = NULL;
            model->label_ctx = NULL;
            model->value_index = -1;
            model->value = NULL;
            model->nudge = NULL;
            furi_string_reset(model->header);
        },
        true);
}

void wc_scroll_list_set_header(WcScrollList *list, const char *header) {
    furi_check(list);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            if (header) {
                furi_string_set_str(model->header, header);
            } else {
                furi_string_reset(model->header);
            }
        },
        true);
}

void wc_scroll_list_add_item(WcScrollList *list, const char *label, uint32_t index,
                             WcScrollListCb callback, void *callback_context) {
    furi_check(list);
    furi_check(label);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            if (model->count < WC_SCROLL_LIST_MAX) {
                WcScrollListItem *item = &model->items[model->count];
                item->label = label;
                item->index = index;
                item->callback = callback;
                item->callback_context = callback_context;
                model->count++;
            } else {
                model->hidden++;
            }
        },
        true);
}

void wc_scroll_list_add_generated(WcScrollList *list, uint16_t count, WcScrollListLabelFn label_fn,
                                  WcScrollListCb callback, void *callback_context) {
    furi_check(list);
    furi_check(label_fn);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            model->label_fn = label_fn;
            model->label_ctx = callback_context;
            for (uint16_t i = 0; i < count; i++) {
                if (model->count >= WC_SCROLL_LIST_MAX) {
                    model->hidden += (uint16_t)(count - i);
                    break;
                }
                WcScrollListItem *item = &model->items[model->count];
                item->label = NULL; // generated on demand, never stored
                item->index = i;
                item->callback = callback;
                item->callback_context = callback_context;
                model->count++;
            }
        },
        true);
}

void wc_scroll_list_set_item_value(WcScrollList *list, uint32_t index, const char *value,
                                   WcScrollListNudgeFn on_nudge, void *context) {
    furi_check(list);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            model->value_index = (int32_t)index;
            model->value = value;
            model->nudge = on_nudge;
            model->nudge_ctx = context;
        },
        true);
}

void wc_scroll_list_note_hidden(WcScrollList *list, uint16_t n) {
    furi_check(list);
    with_view_model(list->view, WcScrollListModel * model, { model->hidden += n; }, true);
}
