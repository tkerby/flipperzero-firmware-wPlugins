#include "seos_i.h"

#define TAG "Seos"

#define SEOS_KEYS_FILENAME "keys"

bool seos_load_keys(Seos* seos) {
    const char* file_header = "Seos keys";
    const uint32_t file_version = 1;
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(seos->storage);
    FuriString* path = furi_string_alloc();
    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;

    do {
        furi_string_printf(
            path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, SEOS_KEYS_FILENAME, ".txt");
        // Open file
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(!furi_string_equal_str(temp_str, file_header) || (version != file_version)) {
            break;
        }

        if(!flipper_format_read_uint32(file, "SEOS_ADF_OID_LEN", (uint32_t*)&SEOS_ADF_OID_LEN, 1))
            break;
        if(!flipper_format_read_hex(file, "SEOS_ADF_OID", SEOS_ADF_OID, SEOS_ADF_OID_LEN)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_PRIV_ENC", SEOS_ADF1_PRIV_ENC, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_PRIV_MAC", SEOS_ADF1_PRIV_MAC, 16)) break;
        if(!flipper_format_read_hex(file, "SEOS_ADF1_READ", SEOS_ADF1_READ, 16)) break;

        parsed = true;
    } while(false);

    if(parsed) {
        FURI_LOG_I(TAG, "Keys loaded");
        BitBuffer* tmp = bit_buffer_alloc(SEOS_ADF_OID_LEN);
        bit_buffer_append_bytes(tmp, SEOS_ADF_OID, SEOS_ADF_OID_LEN);
        seos_log_bitbuffer(TAG, "Keys for ADF OID loaded", tmp);
        bit_buffer_free(tmp);
    } else {
        FURI_LOG_I(TAG, "Using default keys");
    }

    furi_string_free(path);
    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

bool seos_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Seos* seos = context;
    return scene_manager_handle_custom_event(seos->scene_manager, event);
}

bool seos_back_event_callback(void* context) {
    furi_assert(context);
    Seos* seos = context;
    return scene_manager_handle_back_event(seos->scene_manager);
}

void seos_tick_event_callback(void* context) {
    furi_assert(context);
    Seos* seos = context;
    scene_manager_handle_tick_event(seos->scene_manager);
}

Seos* seos_alloc() {
    Seos* seos = malloc(sizeof(Seos));

    seos->has_external_ble = false;
    furi_hal_power_enable_otg();

    seos->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(seos->view_dispatcher);
    seos->scene_manager = scene_manager_alloc(&seos_scene_handlers, seos);
    view_dispatcher_set_event_callback_context(seos->view_dispatcher, seos);
    view_dispatcher_set_custom_event_callback(seos->view_dispatcher, seos_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(seos->view_dispatcher, seos_back_event_callback);
    view_dispatcher_set_tick_event_callback(seos->view_dispatcher, seos_tick_event_callback, 100);

    seos->nfc = nfc_alloc();

    // Nfc device
    seos->nfc_device = nfc_device_alloc();
    nfc_device_set_loading_callback(seos->nfc_device, seos_show_loading_popup, seos);

    // Open GUI record
    seos->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(seos->view_dispatcher, seos->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    seos->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Submenu
    seos->submenu = submenu_alloc();
    view_dispatcher_add_view(seos->view_dispatcher, SeosViewMenu, submenu_get_view(seos->submenu));

    // Popup
    seos->popup = popup_alloc();
    view_dispatcher_add_view(seos->view_dispatcher, SeosViewPopup, popup_get_view(seos->popup));

    // Loading
    seos->loading = loading_alloc();
    view_dispatcher_add_view(
        seos->view_dispatcher, SeosViewLoading, loading_get_view(seos->loading));

    // Text Input
    seos->text_input = text_input_alloc();
    view_dispatcher_add_view(
        seos->view_dispatcher, SeosViewTextInput, text_input_get_view(seos->text_input));

    // TextBox
    seos->text_box = text_box_alloc();
    view_dispatcher_add_view(
        seos->view_dispatcher, SeosViewTextBox, text_box_get_view(seos->text_box));
    seos->text_box_store = furi_string_alloc();

    // Custom Widget
    seos->widget = widget_alloc();
    view_dispatcher_add_view(seos->view_dispatcher, SeosViewWidget, widget_get_view(seos->widget));

    seos->storage = furi_record_open(RECORD_STORAGE);
    seos->dialogs = furi_record_open(RECORD_DIALOGS);
    seos->load_path = furi_string_alloc();

    seos->seos_emulator = seos_emulator_alloc(&seos->credential);

    seos->keys_loaded = seos_load_keys(seos);

    return seos;
}

void seos_free(Seos* seos) {
    furi_assert(seos);

    furi_hal_power_disable_otg();

    nfc_free(seos->nfc);

    // Nfc device
    nfc_device_free(seos->nfc_device);

    // Submenu
    view_dispatcher_remove_view(seos->view_dispatcher, SeosViewMenu);
    submenu_free(seos->submenu);

    // Popup
    view_dispatcher_remove_view(seos->view_dispatcher, SeosViewPopup);
    popup_free(seos->popup);

    // Loading
    view_dispatcher_remove_view(seos->view_dispatcher, SeosViewLoading);
    loading_free(seos->loading);

    // TextInput
    view_dispatcher_remove_view(seos->view_dispatcher, SeosViewTextInput);
    text_input_free(seos->text_input);

    // TextBox
    view_dispatcher_remove_view(seos->view_dispatcher, SeosViewTextBox);
    text_box_free(seos->text_box);
    furi_string_free(seos->text_box_store);

    // Custom Widget
    view_dispatcher_remove_view(seos->view_dispatcher, SeosViewWidget);
    widget_free(seos->widget);

    // View Dispatcher
    view_dispatcher_free(seos->view_dispatcher);

    // Scene Manager
    scene_manager_free(seos->scene_manager);

    // GUI
    furi_record_close(RECORD_GUI);
    seos->gui = NULL;

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    seos->notifications = NULL;

    furi_string_free(seos->load_path);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    if(seos->seos_emulator) {
        seos_emulator_free(seos->seos_emulator);
        seos->seos_emulator = NULL;
    }

    free(seos);
}

void seos_text_store_set(Seos* seos, const char* text, ...) {
    va_list args;
    va_start(args, text);

    vsnprintf(seos->text_store, sizeof(seos->text_store), text, args);

    va_end(args);
}

void seos_text_store_clear(Seos* seos) {
    memset(seos->text_store, 0, sizeof(seos->text_store));
}

bool seos_credential_save(Seos* seos, const char* dev_name) {
    bool saved = false;
    FlipperFormat* file = flipper_format_file_alloc(seos->storage);
    FuriString* temp_str = furi_string_alloc();
    bool use_load_path = true;

    do {
        if(use_load_path && !furi_string_empty(seos->load_path)) {
            // Get directory name
            path_extract_dirname(furi_string_get_cstr(seos->load_path), temp_str);
            // Make path to file to save
            furi_string_cat_printf(temp_str, "/%s%s", dev_name, SEOS_APP_EXTENSION);
        } else {
            // First remove file if it was saved
            furi_string_printf(
                temp_str, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, dev_name, SEOS_APP_EXTENSION);
        }

        // Open file
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(temp_str))) break;

        // Write header
        if(!flipper_format_write_header_cstr(file, seos_file_header, seos_file_version)) break;

        if(!flipper_format_write_uint32(
               file, "Diversifier Length", (uint32_t*)&(seos->credential.diversifier_len), 1))
            break;
        if(!flipper_format_write_hex(
               file, "Diversifier", seos->credential.diversifier, seos->credential.diversifier_len))
            break;
        if(!flipper_format_write_uint32(
               file, "SIO Length", (uint32_t*)&(seos->credential.sio_len), 1))
            break;
        if(!flipper_format_write_hex(file, "SIO", seos->credential.sio, seos->credential.sio_len))
            break;

        saved = true;
    } while(false);

    if(!saved) {
        dialog_message_show_storage_error(seos->dialogs, "Can not save\nfile");
    }
    furi_string_free(temp_str);
    flipper_format_free(file);
    return saved;
}

static const NotificationSequence seos_sequence_blink_start_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence seos_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void seos_blink_start(Seos* seos) {
    notification_message(seos->notifications, &seos_sequence_blink_start_blue);
}

void seos_blink_stop(Seos* seos) {
    notification_message(seos->notifications, &seos_sequence_blink_stop);
}

void seos_show_loading_popup(void* context, bool show) {
    Seos* seos = context;

    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

int32_t seos_app(void* p) {
    UNUSED(p);
    Seos* seos = seos_alloc();

    if(seos->keys_loaded) {
        scene_manager_next_scene(seos->scene_manager, SeosSceneMainMenu);
    } else {
        scene_manager_next_scene(seos->scene_manager, SeosSceneZeroKeys);
    }

    view_dispatcher_run(seos->view_dispatcher);

    seos_free(seos);

    return 0;
}
