#include "receiving_view.h"
#include <gui/elements.h>
#include "../utils/draw_utils.h"
#include "../infrared/infrared_transfer.h"
#include "../ir_main.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <input/input.h>
#include <storage/storage.h>
#include <string.h>

static bool rx_has_activity(const ReceivingModel* m) {
    if(!m || !m->controller) return false;
    const InfraredController* c = m->controller;
    return c->is_receiving || (c->bytes_received > 0) || (c->size_file_count > 0);
}

static void reset_reception_state(InfraredController* c) {
    // Arrêter proprement la session en cours
    c->is_receiving = false;
    c->processing_signal = false;

    // Compteurs / tailles
    c->size_file_count = 0;
    memset(c->size_file_buffer, 0, sizeof(c->size_file_buffer));
    c->file_size = 0;
    c->bytes_received = 0;
    c->progress = 0.0f;

    // Nom de fichier
    c->file_name_index = 0;
    memset(c->file_name_buffer, 0, sizeof(c->file_name_buffer));
}

static void Receiving_draw_callback(Canvas* canvas, void* model) {
    ReceivingModel* rec_model = (ReceivingModel*)model;
    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 12, "IR FILE RECEIVER");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 23, "path :");
    canvas_draw_str(canvas, 28, 23, furi_string_get_cstr(rec_model->receiving_path));
    canvas_draw_str(canvas, 0, 34, "status :");
    draw_truncated_text(canvas, 35, 34, furi_string_get_cstr(rec_model->status), 98);
    canvas_draw_str(canvas, 0, 44, "progress :");
    if(furi_string_cmp_str(rec_model->progression, "not started") == 0) {
        canvas_draw_str(canvas, 45, 44, furi_string_get_cstr(rec_model->progression));
    } else {
        elements_progress_bar(canvas, 46, 37, 75, rec_model->loading);
    }

    if(rx_has_activity(rec_model)) {
        elements_button_center(canvas, "cancel");
    } 
    elements_button_left(canvas, "back");

}

static uint32_t receiving_exit_callback(void* _context) {
    UNUSED(_context);
    return LandingView;
}

static bool Receiving_input_callback(InputEvent* event, void* context) {
    
	

	App* app = (App*)context;
    ReceivingModel* model = view_get_model(app->rec_view);
	bool in_activity = rx_has_activity(model);
	
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            

            // 1) Supprime le fichier temporaire s'il existe
            Storage* storage = furi_record_open(RECORD_STORAGE);
            if(storage_file_exists(storage, FILE_PATH_RECV)) {
                storage_common_remove(storage, FILE_PATH_RECV);
            }
            furi_record_close(RECORD_STORAGE);

            // 2) Reset complet de l'état RX (protégé par mutex)
            if(model->controller) {
                if(furi_mutex_acquire(model->controller->state_mutex, FuriWaitForever) == FuriStatusOk) {
                    reset_reception_state(model->controller);
                    furi_mutex_release(model->controller->state_mutex);
                } else {
                    reset_reception_state(model->controller);
                }
            }

            // 3) Met à jour l'UI (statut/progression/chemin)
            with_view_model(
                app->rec_view, ReceivingModel* _m, {
                    _m->loading = 0.0f;
                    furi_string_set(_m->status, in_activity ? "transfer canceled" : "awaiting signal");
                    furi_string_set(_m->progression, "not started");
                    furi_string_set(_m->receiving_path, FILE_PATH_FOLDER);
                }, true);

            return true;
        } else if(event->key == InputKeyLeft) {
            // (Facultatif) revenir au landing
            view_dispatcher_switch_to_view(app->view_dispatcher, LandingView);
            return true;
        }
    }

    // IMPORTANT : false si aucun bouton ne correspond
    return false;
}

static void rec_view_enter_callback(void* context) {
    App* app = (App*)context;
    ReceivingModel* model = view_get_model(app->rec_view);

    FURI_LOG_I("rec_view_enter", "about to allocate infrared controller");

    bool redraw = true;
    with_view_model(
        app->rec_view, ReceivingModel* _model, {
            _model->loading = 0;
            furi_string_set(_model->status, "awaiting signal");
            furi_string_set(_model->receiving_path, FILE_PATH_FOLDER);
            furi_string_set(_model->progression, "not started");
        }, redraw);

    model->controller = infrared_controller_alloc(app);
}

static void rec_view_exit_callback(void* context) {
    App* app = (App*)context;
    ReceivingModel* model = view_get_model(app->rec_view);
    if(model->controller) {
        infrared_controller_free(model->controller);
        model->controller = NULL; // <-- évite un dangling pointer
    }
}

View* receiving_view_alloc(App* app) {
    View* receiving_view = view_alloc();
    view_set_draw_callback(receiving_view, Receiving_draw_callback);
    view_set_input_callback(receiving_view, Receiving_input_callback);
    view_set_previous_callback(receiving_view, receiving_exit_callback);
    view_set_context(receiving_view, app);
    view_set_enter_callback(receiving_view, rec_view_enter_callback);
    view_set_exit_callback(receiving_view, rec_view_exit_callback);
    view_allocate_model(receiving_view, ViewModelTypeLockFree, sizeof(ReceivingModel));
    ReceivingModel* receving_model = view_get_model(receiving_view);
    receving_model->status = furi_string_alloc();
    receving_model->receiving_path = furi_string_alloc();
    receving_model->progression = furi_string_alloc();
    receving_model->controller = NULL;
    return receiving_view;
}

void receiving_view_free(View* receiving_view) {
    furi_assert(receiving_view);
    ReceivingModel* receving_model = view_get_model(receiving_view);
    furi_string_free(receving_model->status);
    furi_string_free(receving_model->receiving_path);
    furi_string_free(receving_model->progression);
    view_free(receiving_view);
}
