#include "../nfc_eink_app.h"

static void nfc_eink_scene_choose_screen_submenu_callback(void* context, uint32_t index) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

void nfc_eink_scene_saved_menu_on_enter(void* context) {
    NfcEinkApp* instance = context;
    Submenu* submenu = instance->submenu;

    EinkScreenInfoArray_init(instance->arr);
    FURI_LOG_W(TAG, "Array size: %d", EinkScreenInfoArray_size(instance->arr));

    /* uint8_t cnt =
        nfc_eink_descriptor_filter_by_screen_type(instance->arr, instance->info_temp->screen_type); */
    uint8_t cnt =
        nfc_eink_descriptor_filter_by_screen_size(instance->arr, instance->info_temp->screen_size);
    /*     uint8_t cnt = nfc_eink_descriptor_filter_by_manufacturer(
        instance->arr, instance->info_temp->screen_manufacturer); */
    FURI_LOG_W(TAG, "Array size: %d", EinkScreenInfoArray_size(instance->arr));

    submenu_set_header(submenu, "Choose Screen Type");

    for(uint8_t i = 0; i < cnt; i++) {
        const NfcEinkScreenInfo* item = *EinkScreenInfoArray_get(instance->arr, i);
        submenu_add_item(
            submenu, item->name, i, nfc_eink_scene_choose_screen_submenu_callback, instance);
        FURI_LOG_W(TAG, "Item: %s, width: %d, height: %d", item->name, item->width, item->height);
    }

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewMenu);
}

bool nfc_eink_scene_saved_menu_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t index = event.event;
        const NfcEinkScreenInfo* item = *EinkScreenInfoArray_get(instance->arr, index);

        instance->screen = nfc_eink_screen_alloc(item->screen_manufacturer);
        nfc_eink_screen_init(instance->screen, item->screen_type);
        if(nfc_eink_screen_load_data(
               furi_string_get_cstr(instance->file_path), instance->screen, instance->info_temp))
            scene_manager_next_scene(scene_manager, NfcEinkAppSceneWrite);
        else {
            FURI_LOG_E(TAG, "Unable to load image data");
        }

        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_saved_menu_on_exit(void* context) {
    NfcEinkApp* instance = context;
    submenu_reset(instance->submenu);
    EinkScreenInfoArray_clear(instance->arr);
}
