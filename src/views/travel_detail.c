#include "travel_detail.h"
#include "../view_modules/elements.h"

struct TravelDetailView {
    View* view;
    TravelDetailViewCallback cb;
    void* ctx;
};

typedef struct {
    TUnionMessage* message;
    TUnionMessageExt* message_ext;

    size_t index;
} RecordListViewModel;

void travel_detail_process_up(TravelDetailView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->travel_cnt;

            if(model->index > 0)
                model->index--;
            else
                model->index = items_size - 1;
        },
        true);
}

void travel_detail_process_down(TravelDetailView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->travel_cnt;

            if(model->index < items_size - 1)
                model->index++;
            else
                model->index = 0;
        },
        true);
}

void travel_detail_process_ok(TravelDetailView* instance) {
    UNUSED(instance);
}

void travel_detail_process_left(TravelDetailView* instance) {
    UNUSED(instance);
}

void travel_detail_process_right(TravelDetailView* instance) {
    UNUSED(instance);
}

uint8_t travel_detail_get_index(TravelDetailView* instance) {
    uint8_t index = 0;
    with_view_model(instance->view, RecordListViewModel * model, { index = model->index; }, false);
    return index;
}

void travel_detail_set_index(TravelDetailView* instance, size_t index) {
    with_view_model(instance->view, RecordListViewModel * model, { model->index = index; }, true);
}

void travel_detail_reset(TravelDetailView* instance) {
    with_view_model(instance->view, RecordListViewModel * model, { model->index = 0; }, false);
}

static void travel_detail_view_draw_cb(Canvas* canvas, void* _model) {
    RecordListViewModel* model = _model;

    FuriString* temp_str = furi_string_alloc_printf("travel detail idx=%d", model->index);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, furi_string_get_cstr(temp_str));
    furi_string_free(temp_str);
}

static bool travel_detail_view_input_cb(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    TravelDetailView* instance = ctx;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            travel_detail_process_up(instance);
            break;
        case InputKeyDown:
            consumed = true;
            travel_detail_process_down(instance);
            break;
        case InputKeyOk:
            consumed = true;
            travel_detail_process_ok(instance);
            break;
        case InputKeyRight:
            consumed = true;
            travel_detail_process_right(instance);
            break;
        case InputKeyLeft:
            consumed = true;
            travel_detail_process_left(instance);
        default:
            break;
        }
    }
    return consumed;
}

TravelDetailView* travel_detail_alloc(TUnionMessage* message, TUnionMessageExt* message_ext) {
    furi_assert(message);
    furi_assert(message_ext);
    TravelDetailView* instance = malloc(sizeof(TravelDetailView));

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(RecordListViewModel));
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            model->message = message;
            model->message_ext = message_ext;
            model->index = 0;
        },
        false);

    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, travel_detail_view_draw_cb);
    view_set_input_callback(instance->view, travel_detail_view_input_cb);

    return instance;
}

void travel_detail_free(TravelDetailView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* travel_detail_get_view(TravelDetailView* instance) {
    furi_assert(instance);
    return instance->view;
}

void travel_detail_set_callback(TravelDetailView* instance, TravelDetailViewCallback cb, void* ctx) {
    furi_assert(instance);
    furi_assert(cb);
    instance->cb = cb;
    instance->ctx = ctx;
}
