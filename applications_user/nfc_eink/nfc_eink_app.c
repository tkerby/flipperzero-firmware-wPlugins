#include "nfc_eink_app.h"
#include <flipper_format/flipper_format.h>

///TODO: make this function to point to my repo (maybe?)
static bool nfc_is_hal_ready(void) {
    if(furi_hal_nfc_is_hal_ready() != FuriHalNfcErrorNone) {
        // No connection to the chip, show an error screen
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_header(message, "Error: NFC Chip Failed", 64, 0, AlignCenter, AlignTop);
        dialog_message_set_text(
            message, "Send error photo via\nsupport.flipper.net", 0, 63, AlignLeft, AlignBottom);
        dialog_message_set_icon(message, &I_err_09, 128 - 25, 64 - 25);
        dialog_message_show(dialogs, message);
        dialog_message_free(message);
        furi_record_close(RECORD_DIALOGS);
        return false;
    } else {
        return true;
    }
}

static bool nfc_eink_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcEinkApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_eink_app_back_event_callback(void* context) {
    furi_assert(context);
    NfcEinkApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void nfc_eink_app_tick_event_callback(void* context) {
    furi_assert(context);
    NfcEinkApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void timer_callback(void* context) {
    furi_assert(context);
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, /* CustomEventEmulationDone */ 0);
}

static NfcEinkApp* nfc_eink_app_alloc() {
    NfcEinkApp* instance = malloc(sizeof(NfcEinkApp));

    // Open GUI record
    instance->gui = furi_record_open(RECORD_GUI);

    // Open Storage record
    instance->storage = furi_record_open(RECORD_STORAGE);

    // Open Dialogs record
    instance->dialogs = furi_record_open(RECORD_DIALOGS);

    // Open Notification record
    instance->notifications = furi_record_open(RECORD_NOTIFICATION);

    instance->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(instance->view_dispatcher, instance);
    view_dispatcher_set_custom_event_callback(
        instance->view_dispatcher, nfc_eink_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        instance->view_dispatcher, nfc_eink_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        instance->view_dispatcher, nfc_eink_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);
    //view_dispatcher_enable_queue(instance->view_dispatcher);

    // Submenu
    instance->submenu = submenu_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewMenu, submenu_get_view(instance->submenu));

    // Popup
    instance->popup = popup_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewPopup, popup_get_view(instance->popup));

    /*     // Dialog
    instance->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcViewDialogEx, dialog_ex_get_view(instance->dialog_ex));
    // Loading
    instance->loading = loading_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcViewLoading, loading_get_view(instance->loading));
    */

    // Text Input
    instance->text_input = text_input_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        NfcEinkViewTextInput,
        text_input_get_view(instance->text_input));

    /*
    // Byte Input
    instance->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcViewByteInput, byte_input_get_view(instance->byte_input));

    // TextBox
    instance->text_box = text_box_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcViewTextBox, text_box_get_view(instance->text_box));
    instance->text_box_store = furi_string_alloc();*/

    // Custom Widget
    instance->widget = widget_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewWidget, widget_get_view(instance->widget));

    // Empty Screen
    /*     instance->empty_screen = empty_screen_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewEmptyScreen, widget_get_view(instance->widget));
 */
    instance->view_image = view_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, NfcEinkViewEmptyScreen, instance->view_image);

    instance->scene_manager = scene_manager_alloc(&nfc_eink_scene_handlers, instance);
    instance->tx_buf = bit_buffer_alloc(50);
    instance->nfc = nfc_alloc();
    instance->file_path = furi_string_alloc();
    instance->file_name = furi_string_alloc();
    instance->timer = furi_timer_alloc(timer_callback, FuriTimerTypeOnce, instance);
    return instance;
}

static void nfc_eink_app_free(NfcEinkApp* instance) {
    furi_assert(instance);

    //free(instance->image_data);
    bit_buffer_free(instance->tx_buf);
    nfc_free(instance->nfc);
    scene_manager_free(instance->scene_manager);

    // Submenu
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewMenu);
    submenu_free(instance->submenu);

    /*     // DialogEx
    view_dispatcher_remove_view(instance->view_dispatcher, NfcViewDialogEx);
    dialog_ex_free(instance->dialog_ex);

    // Loading
    view_dispatcher_remove_view(instance->view_dispatcher, NfcViewLoading);
    loading_free(instance->loading);
    */

    // Popup
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewPopup);
    popup_free(instance->popup);

    // TextInput
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewTextInput);
    text_input_free(instance->text_input);

    /*
    // ByteInput
    view_dispatcher_remove_view(instance->view_dispatcher, NfcViewByteInput);
    byte_input_free(instance->byte_input);

    // TextBox
    view_dispatcher_remove_view(instance->view_dispatcher, NfcViewTextBox);
    text_box_free(instance->text_box);
    furi_string_free(instance->text_box_store);
    */

    // Custom Widget
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewWidget);
    widget_free(instance->widget);

    /*     // Empty Screen
    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewEmptyScreen);
    empty_screen_free(instance->empty_screen);
 */

    view_dispatcher_remove_view(instance->view_dispatcher, NfcEinkViewEmptyScreen);
    view_free(instance->view_image);

    view_dispatcher_free(instance->view_dispatcher);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    furi_string_free(instance->file_path);
    furi_string_free(instance->file_name);

    instance->dialogs = NULL;
    instance->gui = NULL;
    instance->storage = NULL;
    instance->notifications = NULL;
    furi_timer_free(instance->timer);
    free(instance);
}

static void nfc_eink_make_app_folders(const NfcEinkApp* instance) {
    furi_assert(instance);

    if(!storage_simply_mkdir(instance->storage, NFC_EINK_APP_FOLDER)) {
        dialog_message_show_storage_error(instance->dialogs, "Cannot create\napp folder");
    }
}
/* 
///TODO: Think of moving this to eink_screen code namespace
bool nfc_eink_screen_load(const char* file_path, NfcEinkScreen** screen) {
    furi_assert(screen);
    furi_assert(file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);

    bool loaded = false;
    do {
        if(!flipper_format_buffered_file_open_existing(ff, file_path)) break;

        uint32_t buf = 0;
        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_TYPE_KEY, &buf, 1)) break;
        NfcEinkScreen* scr = nfc_eink_screen_alloc(buf);

        if(!flipper_format_read_uint32(ff, NFC_EINK_SCREEN_DATA_READ_KEY, &buf, 1)) break;
        scr->data->received_data = buf;

        FuriString* temp_str = furi_string_alloc();
        bool block_loaded = false;
        for(uint16_t i = 0; i < scr->data->received_data; i += scr->data->base.data_block_size) {
            furi_string_printf(temp_str, "%s %d", NFC_EINK_SCREEN_BLOCK_DATA_KEY, i);
            block_loaded = flipper_format_read_hex(
                ff,
                furi_string_get_cstr(temp_str),
                &scr->data->image_data[i],
                scr->data->base.data_block_size);
            if(!block_loaded) break;
        }
        furi_string_free(temp_str);

        loaded = block_loaded;
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}
*/

bool nfc_eink_screen_delete(const char* file_path) {
    furi_assert(file_path);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool deleted = storage_simply_remove(storage, file_path);
    furi_record_close(RECORD_STORAGE);
    return deleted;
}

int32_t nfc_eink(/* void* p */) {
    if(!nfc_is_hal_ready()) return 0;

    NfcEinkApp* app = nfc_eink_app_alloc();

    nfc_eink_make_app_folders(app);
    scene_manager_next_scene(app->scene_manager, NfcEinkAppSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    nfc_eink_app_free(app);
    return 0;
}
