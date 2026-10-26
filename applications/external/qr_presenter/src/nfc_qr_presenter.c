#include "nfc_qr_nfc.h"
#include "nfc_qr_presenter.h"
#include "qrcodegen.h"

#include "../assets/nfc_qr_icon_xbm.h"

#include <furi.h>
#include <dialogs/dialogs.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFC_QR_QR_MAX_VERSION 9

/* Module callbacks enqueue events; view changes happen after their model unlocks. */
#define NFC_QR_EVENT_MAIN    100
#define NFC_QR_EVENT_ADD     200
#define NFC_QR_EVENT_PAYLOAD 300
#define NFC_QR_EVENT_TEXT    400
#define NFC_QR_EVENT_NAME    401
#define NFC_QR_EVENT_DIALOG  500

typedef enum {
    NfcQrViewMain,
    NfcQrViewPayloads,
    NfcQrViewTextInput,
    NfcQrViewDialog,
    NfcQrViewShare,
    NfcQrViewAdd,
} NfcQrView;

typedef enum {
    NfcQrMainShare,
    NfcQrMainAdd,
    NfcQrMainEdit,
    NfcQrMainDelete,
} NfcQrMainIndex;

typedef enum {
    NfcQrDialogStatus,
    NfcQrDialogConfirmDelete,
} NfcQrDialogMode;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    ViewDispatcher* dispatcher;
    Submenu* main_menu;
    Submenu* payload_menu;
    Submenu* add_menu;
    TextInput* text_input;
    DialogEx* dialog;
    View* share_view;

    NfcQrView current_view;
    NfcQrPayloadAction pending_action;
    NfcQrDialogMode dialog_mode;
    NfcQrView dialog_return;
    bool naming_card;
    bool importing_card;

    NfcQrPayload payloads[NFC_QR_MAX_PAYLOADS];
    size_t payload_count;
    uint32_t selected_index;

    char text_buffer[NFC_QR_TEXT_MAX + 1];
    char name_buffer[NFC_QR_NAME_LEN];
    char status_buffer[96];

    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(NFC_QR_QR_MAX_VERSION)];
    uint8_t qr_temp[qrcodegen_BUFFER_LEN_FOR_VERSION(NFC_QR_QR_MAX_VERSION)];
    bool qr_ready;

    uint8_t ndef[NFC_QR_NDEF_MAX];
    size_t ndef_len;
    NfcQrNfcWorker nfc_worker;

    bool storage_opened;
    bool gui_opened;
    bool dialogs_opened;
    bool views_registered;
    bool share_model_allocated;
} NfcQrApp;

typedef struct {
    char payload_name[NFC_QR_NAME_LEN];
    char nfc_label[16];
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(NFC_QR_QR_MAX_VERSION)];
    bool qr_ready;
    char type_label[8];
} NfcQrShareModel;

static void nfc_qr_main_menu_callback(void* context, uint32_t index);
static void nfc_qr_payload_selected_callback(void* context, uint32_t index);
static void nfc_qr_text_input_done(void* context);
static void nfc_qr_name_input_done(void* context);
static void nfc_qr_add_menu_callback(void* context, uint32_t index);
static void nfc_qr_dialog_callback(DialogExResult result, void* context);
static void nfc_qr_app_free(NfcQrApp* app);
static void nfc_qr_tick_callback(void* context);
static void nfc_qr_switch_view(NfcQrApp* app, NfcQrView view);

static void nfc_qr_switch_view(NfcQrApp* app, NfcQrView view) {
    if(!app || !app->dispatcher) return;
    app->current_view = view;
    view_dispatcher_switch_to_view(app->dispatcher, view);
}

static void nfc_qr_refresh_payloads(NfcQrApp* app) {
    app->payload_count = nfc_qr_storage_scan(app->storage, app->payloads, COUNT_OF(app->payloads));
    if(app->selected_index >= app->payload_count) app->selected_index = 0;
}

static void nfc_qr_show_status(NfcQrApp* app, const char* header, const char* text) {
    dialog_ex_reset(app->dialog);
    /* Reset also clears the callback and its context. */
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, nfc_qr_dialog_callback);
    dialog_ex_set_header(app->dialog, header, 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(app->dialog, text, 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_center_button_text(app->dialog, "OK");
    app->dialog_mode = NfcQrDialogStatus;
    app->dialog_return = NfcQrViewMain;
    nfc_qr_switch_view(app, NfcQrViewDialog);
}

static void nfc_qr_refresh_main_menu(NfcQrApp* app) {
    submenu_reset(app->main_menu);
    snprintf(
        app->status_buffer,
        sizeof(app->status_buffer),
        "NFC QR | %u cards",
        (unsigned)app->payload_count);
    submenu_set_header(app->main_menu, app->status_buffer);
    submenu_add_item(app->main_menu, "Share", NfcQrMainShare, nfc_qr_main_menu_callback, app);
    submenu_add_item(app->main_menu, "Add card", NfcQrMainAdd, nfc_qr_main_menu_callback, app);
    submenu_add_item(app->main_menu, "Edit card", NfcQrMainEdit, nfc_qr_main_menu_callback, app);
    submenu_add_item(
        app->main_menu, "Delete card", NfcQrMainDelete, nfc_qr_main_menu_callback, app);
}

static const char* nfc_qr_action_header(NfcQrPayloadAction action) {
    switch(action) {
    case NfcQrPayloadActionShare:
        return "Share card";
    case NfcQrPayloadActionAdd:
        return "Add card";
    case NfcQrPayloadActionEdit:
        return "Edit card";
    case NfcQrPayloadActionDelete:
        return "Delete card";
    default:
        return "Card";
    }
}

static const char* nfc_qr_share_nfc_label(NfcQrNfcStatus status) {
    switch(status) {
    case NfcQrNfcStatusRunning:
        return "NFC active";
    case NfcQrNfcStatusStarting:
        return "Starting...";
    case NfcQrNfcStatusDisabled:
        return "NDEF ready";
    case NfcQrNfcStatusIdle:
        return "NFC idle";
    default:
        return "NFC error";
    }
}

static void nfc_qr_update_share_model(NfcQrApp* app, bool redraw) {
    if(!app || !app->share_view) return;

    NfcQrShareModel* model = view_get_model(app->share_view);
    if(!model) return;

    memset(model, 0, sizeof(NfcQrShareModel));
    if(app->selected_index < app->payload_count) {
        strlcpy(
            model->payload_name,
            app->payloads[app->selected_index].name,
            sizeof(model->payload_name));
    }
    strlcpy(
        model->nfc_label,
        nfc_qr_share_nfc_label(app->nfc_worker.status),
        sizeof(model->nfc_label));
    model->qr_ready = app->qr_ready;
    strlcpy(
        model->type_label,
        strncasecmp(app->text_buffer, "BEGIN:VCARD", 11) == 0 ? "Contact" :
        strncasecmp(app->text_buffer, "http", 4) == 0         ? "URL" :
                                                                "Text",
        sizeof(model->type_label));
    for(size_t i = 0; model->payload_name[i]; i++) {
        if(model->payload_name[i] == '_') model->payload_name[i] = ' ';
    }
    if(app->qr_ready) memcpy(model->qr, app->qr, sizeof(model->qr));

    view_commit_model(app->share_view, redraw);
}

static void nfc_qr_refresh_payload_menu(NfcQrApp* app) {
    submenu_reset(app->payload_menu);
    submenu_set_header(app->payload_menu, nfc_qr_action_header(app->pending_action));
    for(size_t i = 0; i < app->payload_count; i++) {
        submenu_add_item(
            app->payload_menu, app->payloads[i].name, i, nfc_qr_payload_selected_callback, app);
    }
    submenu_set_selected_item(app->payload_menu, app->selected_index);
}

static bool nfc_qr_prepare_share(NfcQrApp* app) {
    if(!app || (app->selected_index >= app->payload_count)) {
        if(app) nfc_qr_show_status(app, "Error", "Invalid card");
        return false;
    }

    NfcQrPayload* payload = &app->payloads[app->selected_index];

    if(!nfc_qr_storage_read_text(
           app->storage, payload, app->text_buffer, sizeof(app->text_buffer))) {
        nfc_qr_show_status(app, "Cannot read card", "Use UTF-8 text\nUp to 256 bytes");
        return false;
    }

    nfc_qr_normalize_text(app->text_buffer, app->text_buffer, sizeof(app->text_buffer));
    app->ndef_len = nfc_qr_build_ndef_message(app->text_buffer, app->ndef, sizeof(app->ndef));
    if(app->ndef_len == 0) {
        nfc_qr_show_status(app, "Error", "Invalid NDEF data");
        return false;
    }
    if(!nfc_qr_nfc_ndef_fits(app->ndef_len)) {
        nfc_qr_show_status(app, "Card too long", "Use a shorter card\nNDEF limit: 141 bytes");
        return false;
    }

    app->qr_ready = qrcodegen_encodeText(
        app->text_buffer,
        app->qr_temp,
        app->qr,
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        NFC_QR_QR_MAX_VERSION,
        qrcodegen_Mask_AUTO,
        true);
    if(!app->qr_ready) {
        nfc_qr_show_status(app, "Error", "QR data too long");
        return false;
    }

    if(!nfc_qr_nfc_worker_start(&app->nfc_worker, app->ndef, app->ndef_len)) {
        nfc_qr_show_status(
            app, "NFC error", nfc_qr_nfc_worker_status_text(app->nfc_worker.status));
        return false;
    }

    nfc_qr_update_share_model(app, false);
    nfc_qr_switch_view(app, NfcQrViewShare);
    return true;
}

static void nfc_qr_handle_payload(void* context, uint32_t index) {
    NfcQrApp* app = context;
    if(!app || (index >= app->payload_count)) return;

    app->selected_index = index;

    switch(app->pending_action) {
    case NfcQrPayloadActionShare:
        nfc_qr_prepare_share(app);
        break;
    case NfcQrPayloadActionAdd:
        nfc_qr_switch_view(app, NfcQrViewMain);
        break;
    case NfcQrPayloadActionEdit:
        if(nfc_qr_storage_read_text(
               app->storage,
               &app->payloads[app->selected_index],
               app->text_buffer,
               sizeof(app->text_buffer))) {
            text_input_reset(app->text_input);
            app->naming_card = false;
            text_input_set_header_text(app->text_input, "Edit URL / text");
            text_input_set_minimum_length(app->text_input, 1);
            text_input_set_result_callback(
                app->text_input,
                nfc_qr_text_input_done,
                app,
                app->text_buffer,
                sizeof(app->text_buffer),
                false);
            nfc_qr_switch_view(app, NfcQrViewTextInput);
        } else {
            nfc_qr_show_status(app, "Cannot read card", "Use UTF-8 text\nUp to 256 bytes");
        }
        break;
    case NfcQrPayloadActionDelete:
        dialog_ex_reset(app->dialog);
        dialog_ex_set_context(app->dialog, app);
        dialog_ex_set_result_callback(app->dialog, nfc_qr_dialog_callback);
        snprintf(
            app->status_buffer,
            sizeof(app->status_buffer),
            "Delete\n%.24s?",
            app->payloads[app->selected_index].name);
        dialog_ex_set_header(app->dialog, "Delete card?", 64, 10, AlignCenter, AlignCenter);
        dialog_ex_set_text(app->dialog, app->status_buffer, 64, 32, AlignCenter, AlignCenter);
        dialog_ex_set_left_button_text(app->dialog, "Cancel");
        dialog_ex_set_right_button_text(app->dialog, "Delete");
        app->dialog_mode = NfcQrDialogConfirmDelete;
        nfc_qr_switch_view(app, NfcQrViewDialog);
        break;
    }
}

static void nfc_qr_handle_save(void* context) {
    NfcQrApp* app = context;
    NfcQrPayload saved;
    bool ok = false;

    if(!app || !app->storage) return;

    nfc_qr_normalize_text(app->text_buffer, app->text_buffer, sizeof(app->text_buffer));
    app->ndef_len = nfc_qr_build_ndef_message(app->text_buffer, app->ndef, sizeof(app->ndef));
    if(!nfc_qr_nfc_ndef_fits(app->ndef_len)) {
        nfc_qr_show_status(app, "Card too long", "Use a shorter card\nNDEF limit: 141 bytes");
        app->dialog_return = NfcQrViewTextInput;
        return;
    }

    if(app->pending_action == NfcQrPayloadActionEdit) {
        if(app->selected_index >= app->payload_count) return;
        saved = app->payloads[app->selected_index];
        ok = nfc_qr_storage_update_payload(app->storage, &saved, app->text_buffer);
    } else {
        ok = nfc_qr_storage_add_payload(app->storage, app->name_buffer, app->text_buffer, &saved);
    }

    nfc_qr_refresh_payloads(app);
    nfc_qr_refresh_main_menu(app);

    if(ok) {
        nfc_qr_show_status(app, "Card saved", saved.name);
    } else {
        nfc_qr_show_status(app, "Save failed", "Check SD card\nYour input is kept");
        app->dialog_return = NfcQrViewTextInput;
    }
}

static void nfc_qr_handle_main(void* context, uint32_t index) {
    NfcQrApp* app = context;
    if(!app) return;

    if(index == NfcQrMainAdd) {
        app->pending_action = NfcQrPayloadActionAdd;
        nfc_qr_switch_view(app, NfcQrViewAdd);
        return;
    }

    nfc_qr_refresh_payloads(app);
    nfc_qr_refresh_main_menu(app);

    if(app->payload_count == 0) {
        nfc_qr_show_status(app, "No cards yet", "Choose Add card\nto create or import");
        return;
    }

    if(index == NfcQrMainShare) {
        app->pending_action = NfcQrPayloadActionShare;
    } else if(index == NfcQrMainEdit) {
        app->pending_action = NfcQrPayloadActionEdit;
    } else if(index == NfcQrMainDelete) {
        app->pending_action = NfcQrPayloadActionDelete;
    } else {
        nfc_qr_show_status(app, "Error", "Invalid option");
        return;
    }

    nfc_qr_refresh_payload_menu(app);
    nfc_qr_switch_view(app, NfcQrViewPayloads);
}

static void nfc_qr_handle_dialog(DialogExResult result, void* context) {
    NfcQrApp* app = context;
    if(!app) return;

    if(app->dialog_mode == NfcQrDialogConfirmDelete) {
        if((result == DialogExResultRight) && (app->selected_index < app->payload_count)) {
            const bool ok =
                nfc_qr_storage_delete_payload(app->storage, &app->payloads[app->selected_index]);
            nfc_qr_refresh_payloads(app);
            nfc_qr_refresh_main_menu(app);
            nfc_qr_show_status(
                app, ok ? "Card deleted" : "Error", ok ? "Files removed" : "Could not delete");
        } else if(result == DialogExResultLeft) {
            nfc_qr_switch_view(app, NfcQrViewPayloads);
        }
    } else if(app->dialog_mode == NfcQrDialogStatus) {
        if(result == DialogExResultCenter) {
            nfc_qr_switch_view(app, app->dialog_return);
        }
    }
}

static void nfc_qr_main_menu_callback(void* context, uint32_t index) {
    NfcQrApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, NFC_QR_EVENT_MAIN + index);
}

static void nfc_qr_payload_selected_callback(void* context, uint32_t index) {
    NfcQrApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, NFC_QR_EVENT_PAYLOAD + index);
}

static void nfc_qr_text_input_done(void* context) {
    NfcQrApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, NFC_QR_EVENT_TEXT);
}

static void nfc_qr_name_input_done(void* context) {
    NfcQrApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, NFC_QR_EVENT_NAME);
}

static void nfc_qr_add_menu_callback(void* context, uint32_t index) {
    NfcQrApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, NFC_QR_EVENT_ADD + index);
}

static void nfc_qr_dialog_callback(DialogExResult result, void* context) {
    NfcQrApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, NFC_QR_EVENT_DIALOG + result);
}

static void nfc_qr_begin_name(NfcQrApp* app) {
    app->naming_card = true;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Card name");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        nfc_qr_name_input_done,
        app,
        app->name_buffer,
        sizeof(app->name_buffer),
        false);
    nfc_qr_switch_view(app, NfcQrViewTextInput);
}

static void nfc_qr_begin_content(NfcQrApp* app) {
    app->naming_card = false;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "URL / text");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        nfc_qr_text_input_done,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        false);
    nfc_qr_switch_view(app, NfcQrViewTextInput);
}

static void nfc_qr_handle_add(NfcQrApp* app, uint32_t index) {
    nfc_qr_refresh_payloads(app);
    if(index == 2) {
        nfc_qr_refresh_main_menu(app);
        nfc_qr_switch_view(app, NfcQrViewMain);
        return;
    }
    if(app->payload_count >= NFC_QR_MAX_PAYLOADS) {
        nfc_qr_show_status(app, "Card list full", "Maximum: 16 cards\nDelete a card first");
        return;
    }
    app->pending_action = NfcQrPayloadActionAdd;
    app->importing_card = index == 1;
    app->name_buffer[0] = '\0';
    app->text_buffer[0] = '\0';
    if(app->importing_card) {
        FuriString* path = furi_string_alloc_set(NFC_QR_DATA_DIR);
        DialogsFileBrowserOptions options;
        dialog_file_browser_set_basic_options(&options, ".txt|.vcf", NULL);
        options.base_path = "/ext";
        options.hide_ext = false;
        const bool selected = dialog_file_browser_show(app->dialogs, path, path, &options);
        nfc_qr_switch_view(app, NfcQrViewAdd);
        if(!selected) {
            furi_string_free(path);
            return;
        }
        const char* source = furi_string_get_cstr(path);
        const char* ext = strrchr(source, '.');
        NfcQrPayload file = {0};
        strlcpy(file.txt_path, source, sizeof(file.txt_path));
        bool ok = strlen(source) < sizeof(file.txt_path) && ext &&
                  (!strcasecmp(ext, ".txt") || !strcasecmp(ext, ".vcf"));
        if(ok)
            ok = nfc_qr_storage_read_text(
                app->storage, &file, app->text_buffer, sizeof(app->text_buffer));
        if(ok) {
            const char* base = strrchr(source, '/');
            base = base ? base + 1 : source;
            const size_t length = MIN((size_t)(ext - base), sizeof(app->name_buffer) - 1);
            memcpy(app->name_buffer, base, length);
            app->name_buffer[length] = '\0';
        }
        furi_string_free(path);
        if(!ok) {
            nfc_qr_show_status(app, "Import failed", "Use .txt or .vcf\nUTF-8, max 256 bytes");
            app->dialog_return = NfcQrViewAdd;
            return;
        }
    }
    nfc_qr_begin_name(app);
}

static bool nfc_qr_custom_event(void* context, uint32_t event) {
    NfcQrApp* app = context;
    if(event >= NFC_QR_EVENT_DIALOG && event <= NFC_QR_EVENT_DIALOG + DialogExResultRight) {
        nfc_qr_handle_dialog(event - NFC_QR_EVENT_DIALOG, app);
    } else if(event == NFC_QR_EVENT_TEXT) {
        nfc_qr_handle_save(app);
    } else if(event == NFC_QR_EVENT_NAME) {
        nfc_qr_begin_content(app);
        if(app->importing_card) nfc_qr_handle_save(app);
    } else if(event >= NFC_QR_EVENT_PAYLOAD && event < NFC_QR_EVENT_PAYLOAD + NFC_QR_MAX_PAYLOADS) {
        nfc_qr_handle_payload(app, event - NFC_QR_EVENT_PAYLOAD);
    } else if(event >= NFC_QR_EVENT_ADD && event <= NFC_QR_EVENT_ADD + 2) {
        nfc_qr_handle_add(app, event - NFC_QR_EVENT_ADD);
    } else if(event >= NFC_QR_EVENT_MAIN && event <= NFC_QR_EVENT_MAIN + NfcQrMainDelete) {
        nfc_qr_handle_main(app, event - NFC_QR_EVENT_MAIN);
    } else {
        return false;
    }
    return true;
}

static void nfc_qr_share_draw(Canvas* canvas, void* model) {
    NfcQrShareModel* share_model = model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(!share_model) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 18, 30, "View unavailable");
        elements_button_left(canvas, "Back");
        return;
    }

    if(!share_model->qr_ready) {
        canvas_draw_xbm(
            canvas, 8, 14, NFC_QR_ICON_XBM_WIDTH, NFC_QR_ICON_XBM_HEIGHT, nfc_qr_icon_xbm);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 48, 28, "QR not ready");
        elements_button_left(canvas, "Back");
        return;
    }

    const int qr_size = qrcodegen_getSize(share_model->qr);
    const int quiet = 4;
    int scale = 64 / (qr_size + (quiet * 2));
    if(scale < 1) scale = 1;
    if(scale > 2) scale = 2;
    const int box = (qr_size + (quiet * 2)) * scale;
    const int x0 = (64 - box) / 2;
    const int y0 = (64 - box) / 2;

    for(int y = 0; y < qr_size; y++) {
        for(int x = 0; x < qr_size; x++) {
            if(qrcodegen_getModule(share_model->qr, x, y)) {
                canvas_draw_box(
                    canvas, x0 + ((x + quiet) * scale), y0 + ((y + quiet) * scale), scale, scale);
            }
        }
    }

    canvas_draw_line(canvas, 66, 2, 66, 61);
    canvas_set_font(canvas, FontSecondary);
    char first_line[NFC_QR_NAME_LEN];
    strlcpy(first_line, share_model->payload_name, sizeof(first_line));
    size_t split = strlen(first_line);
    while(split > 0 && canvas_string_width(canvas, first_line) > 57)
        first_line[--split] = '\0';
    if(share_model->payload_name[split]) {
        char* space = strrchr(first_line, ' ');
        if(space && space != first_line) {
            split = space - first_line;
            *space = '\0';
        }
    }
    canvas_draw_str(canvas, 70, 10, first_line);
    const char* remainder = share_model->payload_name + split;
    while(*remainder == ' ')
        remainder++;
    FuriString* label = furi_string_alloc_set(remainder);
    elements_string_fit_width(canvas, label, 57);
    canvas_draw_str(canvas, 70, 21, furi_string_get_cstr(label));
    furi_string_set(label, share_model->nfc_label);
    elements_string_fit_width(canvas, label, 57);
    canvas_draw_str(canvas, 70, 36, furi_string_get_cstr(label));
    furi_string_free(label);
    canvas_draw_str(canvas, 70, 47, share_model->type_label);
    canvas_draw_str(canvas, 70, 61, "Back");
}

static bool nfc_qr_share_input(InputEvent* event, void* context) {
    NfcQrApp* app = context;
    if((event->type == InputTypeShort) && (event->key == InputKeyBack)) {
        nfc_qr_nfc_worker_stop(&app->nfc_worker);
        nfc_qr_switch_view(app, NfcQrViewPayloads);
        return true;
    }
    return false;
}

static bool nfc_qr_navigation_callback(void* context) {
    NfcQrApp* app = context;

    if(app->current_view == NfcQrViewMain) {
        view_dispatcher_stop(app->dispatcher);
    } else {
        if(app->current_view == NfcQrViewShare) {
            nfc_qr_nfc_worker_stop(&app->nfc_worker);
        }
        if(app->current_view == NfcQrViewDialog && app->dialog_mode == NfcQrDialogStatus) {
            nfc_qr_switch_view(app, app->dialog_return);
        } else if(
            app->current_view == NfcQrViewTextInput &&
            app->pending_action == NfcQrPayloadActionAdd) {
            if(app->naming_card)
                nfc_qr_switch_view(app, NfcQrViewAdd);
            else
                nfc_qr_begin_name(app);
        } else {
            nfc_qr_switch_view(app, NfcQrViewMain);
        }
    }

    return true;
}

static NfcQrApp* nfc_qr_app_alloc(void) {
    NfcQrApp* app = malloc(sizeof(NfcQrApp));
    if(!app) return NULL;

    memset(app, 0, sizeof(NfcQrApp));

    app->storage = furi_record_open(RECORD_STORAGE);
    app->storage_opened = app->storage != NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->gui_opened = app->gui != NULL;
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->dialogs_opened = app->dialogs != NULL;
    app->dispatcher = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->payload_menu = submenu_alloc();
    app->add_menu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->dialog = dialog_ex_alloc();
    app->share_view = view_alloc();

    if(!app->storage || !app->gui || !app->dialogs || !app->dispatcher || !app->main_menu ||
       !app->payload_menu || !app->add_menu || !app->text_input || !app->dialog ||
       !app->share_view) {
        nfc_qr_app_free(app);
        return NULL;
    }

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, nfc_qr_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->dispatcher, nfc_qr_custom_event);
    view_dispatcher_set_tick_event_callback(app->dispatcher, nfc_qr_tick_callback, 250);

    submenu_set_header(app->add_menu, "Add card");
    submenu_add_item(app->add_menu, "Type URL / text", 0, nfc_qr_add_menu_callback, app);
    submenu_add_item(app->add_menu, "Import .txt / .vcf", 1, nfc_qr_add_menu_callback, app);
    submenu_add_item(app->add_menu, "Refresh SD cards", 2, nfc_qr_add_menu_callback, app);

    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, nfc_qr_dialog_callback);

    view_set_context(app->share_view, app);
    view_allocate_model(app->share_view, ViewModelTypeLocking, sizeof(NfcQrShareModel));
    NfcQrShareModel* share_model = view_get_model(app->share_view);
    memset(share_model, 0, sizeof(NfcQrShareModel));
    view_commit_model(app->share_view, false);
    app->share_model_allocated = true;
    view_set_draw_callback(app->share_view, nfc_qr_share_draw);
    view_set_input_callback(app->share_view, nfc_qr_share_input);

    view_dispatcher_add_view(app->dispatcher, NfcQrViewMain, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->dispatcher, NfcQrViewPayloads, submenu_get_view(app->payload_menu));
    view_dispatcher_add_view(
        app->dispatcher, NfcQrViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->dispatcher, NfcQrViewDialog, dialog_ex_get_view(app->dialog));
    view_dispatcher_add_view(app->dispatcher, NfcQrViewShare, app->share_view);
    view_dispatcher_add_view(app->dispatcher, NfcQrViewAdd, submenu_get_view(app->add_menu));
    app->views_registered = true;
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    nfc_qr_nfc_worker_init(&app->nfc_worker);
    return app;
}

static void nfc_qr_app_free(NfcQrApp* app) {
    if(!app) return;

    nfc_qr_nfc_worker_stop(&app->nfc_worker);

    if(app->dispatcher && app->views_registered) {
        view_dispatcher_remove_view(app->dispatcher, NfcQrViewMain);
        view_dispatcher_remove_view(app->dispatcher, NfcQrViewPayloads);
        view_dispatcher_remove_view(app->dispatcher, NfcQrViewTextInput);
        view_dispatcher_remove_view(app->dispatcher, NfcQrViewDialog);
        view_dispatcher_remove_view(app->dispatcher, NfcQrViewShare);
        view_dispatcher_remove_view(app->dispatcher, NfcQrViewAdd);
    }

    if(app->share_view) {
        view_set_draw_callback(app->share_view, NULL);
        view_set_input_callback(app->share_view, NULL);
        view_set_context(app->share_view, NULL);
        if(app->share_model_allocated) view_free_model(app->share_view);
        view_free(app->share_view);
    }
    if(app->dialog) dialog_ex_free(app->dialog);
    if(app->text_input) text_input_free(app->text_input);
    if(app->payload_menu) submenu_free(app->payload_menu);
    if(app->add_menu) submenu_free(app->add_menu);
    if(app->main_menu) submenu_free(app->main_menu);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);

    if(app->gui_opened) furi_record_close(RECORD_GUI);
    if(app->dialogs_opened) furi_record_close(RECORD_DIALOGS);
    if(app->storage_opened) furi_record_close(RECORD_STORAGE);
    free(app);
}

static void nfc_qr_tick_callback(void* context) {
    NfcQrApp* app = context;
    if(!app || (app->current_view != NfcQrViewShare) || !app->share_view) return;

    if(app->nfc_worker.status >= NfcQrNfcStatusErrorPayload) {
        strlcpy(
            app->status_buffer,
            nfc_qr_nfc_worker_status_text(app->nfc_worker.status),
            sizeof(app->status_buffer));
        nfc_qr_nfc_worker_stop(&app->nfc_worker);
        nfc_qr_show_status(app, "NFC error", app->status_buffer);
    } else {
        nfc_qr_update_share_model(app, true);
    }
}

int32_t nfc_qr_presenter_app(void* p) {
    UNUSED(p);

    NfcQrApp* app = nfc_qr_app_alloc();
    if(!app) return -1;

    const bool storage_ready = nfc_qr_storage_init(app->storage);

    nfc_qr_refresh_payloads(app);
    nfc_qr_refresh_main_menu(app);
    if(storage_ready) {
        nfc_qr_switch_view(app, NfcQrViewMain);
    } else {
        nfc_qr_show_status(app, "SD error", "Check SD card");
    }
    view_dispatcher_run(app->dispatcher);

    nfc_qr_app_free(app);
    return 0;
}
