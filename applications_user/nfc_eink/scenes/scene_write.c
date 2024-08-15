#include "../nfc_eink_app.h"

void nfc_eink_scene_write_on_enter(void* context) {
    NfcEinkApp* instance = context;

    popup_reset(instance->popup);
    popup_set_header(instance->popup, "Writing", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        instance->popup, "Hold eink next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewPopup);

    NfcEinkScreen* screen = instance->screen;

    const NfcProtocol protocol = nfc_device_get_protocol(screen->nfc_device);
    instance->poller = nfc_poller_alloc(instance->nfc, protocol);

    nfc_poller_start(instance->poller, screen->handlers->poller_callback, screen);

    nfc_eink_blink_write_start(instance);
}

bool nfc_eink_scene_write_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    UNUSED(instance);
    UNUSED(event);
    return false;
}

void nfc_eink_scene_write_on_exit(void* context) {
    NfcEinkApp* instance = context;
    nfc_poller_stop(instance->poller);
    nfc_eink_blink_stop(instance);
}
