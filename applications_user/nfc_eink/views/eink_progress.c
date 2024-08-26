#include "eink_progress.h"

#include <gui/elements.h>

struct EinkProgress {
    View* view;
    void* context;
};

typedef struct {
    FuriString* header;
    uint16_t blocks_total;
    uint16_t blocks_current;
} EinkProgressViewModel;

static void eink_progress_draw_callback(Canvas* canvas, void* model) {
    EinkProgressViewModel* m = model;

    FuriString* str = furi_string_alloc_printf("%d / %d", m->blocks_current, m->blocks_total);
    elements_text_box(
        canvas, 60, 30, 60, 20, AlignCenter, AlignCenter, furi_string_get_cstr(m->header), false);
    elements_progress_bar_with_text(canvas, 0, 48, 127, 0.5, furi_string_get_cstr(str));
    furi_string_free(str);
}

View* eink_progress_get_view(EinkProgress* instance) {
    furi_assert(instance);
    return instance->view;
}

EinkProgress* eink_progress_alloc(void) {
    EinkProgress* instance = malloc(sizeof(EinkProgress));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(EinkProgressViewModel));
    view_set_draw_callback(instance->view, eink_progress_draw_callback);
    //view_set_input_callback(instance->view, dict_attack_input_callback);
    view_set_context(instance->view, instance);
    with_view_model(
        instance->view,
        EinkProgressViewModel * model,
        {
            model->header = furi_string_alloc();
            furi_string_printf(model->header, "Dummy EInk");
            model->blocks_current = 20;
            model->blocks_total = 200;
        },
        false);

    return instance;
}

void eink_progress_free(EinkProgress* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view, EinkProgressViewModel * model, { furi_string_free(model->header); }, false);

    view_free(instance->view);
    free(instance);
}
