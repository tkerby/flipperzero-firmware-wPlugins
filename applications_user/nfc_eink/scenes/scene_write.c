#include "../nfc_eink_app.h"

typedef enum {
    NfcEinkAppSceneWriteStateWaitingForTarget,
    NfcEinkAppSceneWriteStateWritingDataBlocks,
} NfcEinkAppSceneWriteStates;

static void nfc_eink_write_callback(NfcEinkScreenEventType type, void* context) {
    furi_assert(context);
    NfcEinkApp* instance = context;
    NfcEinkAppCustomEvents event = NfcEinkAppCustomEventProcessFinish;
    switch(type) {
    case NfcEinkScreenEventTypeTargetDetected:
        event = NfcEinkAppCustomEventTargetDetected;
        FURI_LOG_D(TAG, "Target detected");
        break;
    case NfcEinkScreenEventTypeFinish:
        event = NfcEinkAppCustomEventProcessFinish;
        break;

    case NfcEinkScreenEventTypeBlockProcessed:
        NfcEinkScreenDevice* device = instance->screen->device;
        FURI_LOG_D(TAG, "%d, %d", device->block_current, device->block_total);
        event = NfcEinkAppCustomEventBlockProcessed;
        break;

    case NfcEinkScreenEventTypeFailure:
        event = NfcEinkAppCustomEventUnknownError;
        break;

    default:
        FURI_LOG_E(TAG, "Event: %02X nor implemented", type);
        furi_crash();
        break;
    }
    view_dispatcher_send_custom_event(instance->view_dispatcher, event);
}

static void nfc_eink_scene_write_show_waiting(const NfcEinkApp* instance) {
    popup_reset(instance->popup);
    popup_set_header(instance->popup, "Waiting", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        instance->popup, "Apply eink next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(instance->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewPopup);
}

///TODO: this must be replaced by progress bar view but it needs to be implemented
static void nfc_eink_scene_write_show_writing_data(const NfcEinkApp* instance) {
    //popup_set_header(instance->popup, "Writting", 97, 15, AlignCenter, AlignTop);
    //popup_set_text(
    //   instance->popup, "Hold eink next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    eink_progress_reset(instance->eink_progress);
    eink_progress_set_header(instance->eink_progress, instance->screen->data->base.name);
    eink_progress_set_value(
        instance->eink_progress,
        instance->screen->device->block_current,
        instance->screen->device->block_total);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewProgress);
}

void nfc_eink_scene_write_on_enter(void* context) {
    NfcEinkApp* instance = context;

    nfc_eink_scene_write_show_waiting(instance);
    scene_manager_set_scene_state(
        instance->scene_manager, NfcEinkAppSceneWrite, NfcEinkAppSceneWriteStateWaitingForTarget);

    NfcEinkScreen* screen = instance->screen;

    nfc_eink_screen_set_callback(instance->screen, nfc_eink_write_callback, instance);
    const NfcProtocol protocol = nfc_device_get_protocol(screen->device->nfc_device);
    instance->poller = nfc_poller_alloc(instance->nfc, protocol);

    nfc_poller_start(instance->poller, screen->handlers->poller_callback, screen);

    nfc_eink_blink_write_start(instance);
}

bool nfc_eink_scene_write_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcEinkAppCustomEventProcessFinish) {
            scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneWriteDone);
            notification_message(instance->notifications, &sequence_success);
            consumed = true;
        } else if(event.event == NfcEinkAppCustomEventTargetDetected) {
            scene_manager_set_scene_state(
                instance->scene_manager,
                NfcEinkAppSceneWrite,
                NfcEinkAppSceneWriteStateWritingDataBlocks);
            nfc_eink_scene_write_show_writing_data(instance);
            consumed = true;
        } else if(event.event == NfcEinkAppCustomEventUnknownError) {
            scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneError);
            notification_message(instance->notifications, &sequence_error);
            consumed = true;
        } else if(event.event == NfcEinkAppCustomEventBlockProcessed) {
            eink_progress_set_value(
                instance->eink_progress,
                instance->screen->device->block_current,
                instance->screen->device->block_total);
            consumed = true;
        }
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
