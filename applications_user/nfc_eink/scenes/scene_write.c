#include "../nfc_eink_app.h"

static void nfc_eink_write_done_callback(void* context) {
    furi_assert(context);
    NfcEinkApp* instance = context;
    furi_timer_start(instance->timer, furi_ms_to_ticks(500));
}

void nfc_eink_scene_write_on_enter(void* context) {
    NfcEinkApp* instance = context;

    popup_reset(instance->popup);
    popup_set_header(instance->popup, "Writing", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        instance->popup, "Hold eink next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewPopup);

    NfcEinkScreen* screen = instance->screen;

    nfc_eink_screen_set_done_callback(instance->screen, nfc_eink_write_done_callback, instance);
    const NfcProtocol protocol = nfc_device_get_protocol(screen->nfc_device);
    instance->poller = nfc_poller_alloc(instance->nfc, protocol);

    nfc_poller_start(instance->poller, screen->handlers->poller_callback, screen);

    nfc_eink_blink_write_start(instance);
}

bool nfc_eink_scene_write_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneWriteDone);
        notification_message(instance->notifications, &sequence_success);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_write_on_exit(void* context) {
    NfcEinkApp* instance = context;
    nfc_poller_stop(instance->poller);
    nfc_eink_blink_stop(instance);
    popup_reset(instance->popup);
}
