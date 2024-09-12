#include "../nfc_eink_app.h"

static void reverse_copy_block(const uint8_t* array, uint8_t* reverse_array, bool invert_image) {
    furi_assert(array);
    furi_assert(reverse_array);

    for(int i = 0; i < 16; i++) {
        reverse_array[i] = invert_image ? ~array[15 - i] : array[15 - i];
    }
}

void empty_view_draw_callback(Canvas* canvas, void* model) {
    UNUSED(canvas);
    UNUSED(model);
    canvas_draw_raw_bitmap(canvas, 0, 0, 128, 64, model);
}

void nfc_eink_scene_show_image_on_enter(void* context) {
    NfcEinkApp* instance = context;

    view_set_draw_callback(instance->view_image, empty_view_draw_callback);

    uint16_t received_size = 0;
    if(!instance->screen_loaded) {
        const NfcEinkScreenInfo* info = instance->info_temp;
        instance->screen = nfc_eink_screen_alloc(info->screen_manufacturer);
        nfc_eink_screen_init(instance->screen, info->screen_type);

        if(!nfc_eink_screen_load_data(
               furi_string_get_cstr(instance->file_path), instance->screen, info)) {
            nfc_eink_screen_free(instance->screen);
            instance->screen_loaded = false;
        } else {
            instance->screen_loaded = true;
            received_size = nfc_eink_screen_get_image_size(instance->screen);
        }
    } else
        received_size = nfc_eink_screen_get_received_size(instance->screen);

    const uint8_t* data = nfc_eink_screen_get_image_data(instance->screen);

    ///TODO: this allocation is wrong because it is more efficient to allocate memory equal to screen size
    view_allocate_model(instance->view_image, ViewModelTypeLockFree, received_size);

    uint8_t* model_ptr = view_get_model(instance->view_image);
    for(uint16_t i = 0; i < received_size; i += /* screen->base.data_block_size */ 16)
        reverse_copy_block(data + i, model_ptr + i, instance->settings.invert_image);

    view_commit_model(instance->view_image, true);
    view_set_orientation(instance->view_image, ViewOrientationHorizontalFlip);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewEmptyScreen);
}

bool nfc_eink_scene_show_image_on_event(void* context, SceneManagerEvent event) {
    //NfcEinkApp* instance = context;
    //SceneManager* scene_manager = instance->scene_manager;
    UNUSED(context);
    UNUSED(event);

    bool consumed = false;
    /*    if(event.type == SceneManagerEventTypeTick) {
        View* view = empty_screen_get_view(instance->empty_screen);
        view_commit_model(view, true);
    } */
    /* if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t submenu_index = event.event;
        if(submenu_index == SubmenuIndexShow) {
            scene_manager_next_scene(scene_manager, NfcEinkAppSceneChooseType);
            consumed = true;
        } else if(submenu_index == SubmenuIndexSave) {
            //scene_manager_next_scene(scene_manager, );
            consumed = true;
        }
    } */

    return consumed;
}

void nfc_eink_scene_show_image_on_exit(void* context) {
    NfcEinkApp* instance = context;
    UNUSED(instance);

    if(!scene_manager_has_previous_scene(instance->scene_manager, NfcEinkAppSceneEmulate) &&
       instance->screen_loaded) {
        nfc_eink_screen_free(instance->screen);
        instance->screen_loaded = false;
    }
    //View* view = empty_screen_get_view(instance->empty_screen);
    view_free_model(instance->view_image);
}
