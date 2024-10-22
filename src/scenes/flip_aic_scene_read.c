#include "../helpers/hex.h"
#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include <lib/nfc/protocols/mf_desfire/mf_desfire_poller.h>

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>

enum PollerResult {
    PollerResultSuccess,
    PollerResultFailed,
};

static NfcCommand flip_aic_scene_read_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipAIC* app = context;
    MfDesfirePollerEvent* poller_event = event.event_data;
    view_dispatcher_send_custom_event(app->view_dispatcher, poller_event->type);
    return NfcCommandStop;
}

void flip_aic_scene_read_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FURI_LOG_I(
        TAG,
        "Allocating poller with protocol: %s",
        nfc_device_get_protocol_name(app->nfc_protocol)
    );
    app->nfc_poller = nfc_poller_alloc(app->nfc, app->nfc_protocol);

    FURI_LOG_I(TAG, "Starting poller");
    nfc_poller_start(app->nfc_poller, flip_aic_scene_read_poller_callback, app);
}

bool flip_aic_scene_read_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if (event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
        case MfDesfirePollerEventTypeReadSuccess:;
            NfcDevice* device = nfc_device_alloc();
            nfc_device_set_data(device, app->nfc_protocol, nfc_poller_get_data(app->nfc_poller));

            size_t uid_len;
            const uint8_t* uid = nfc_device_get_uid(device, &uid_len);

            size_t uid_s_len = uid_len * 2 + 1;
            char* uid_s = malloc(uid_s_len);
            to_hex(uid_s, uid_s_len, uid, uid_len);

            FURI_LOG_I(TAG, "Name: %s", nfc_device_get_name(device, NfcDeviceNameTypeFull));
            FURI_LOG_I(TAG, "UID:  %s", uid_s);

            free(uid_s);
            nfc_device_free(device);

        case MfDesfirePollerEventTypeReadFailed:
            break;
        }
        scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
        return true;
    }

    return false;
}

void flip_aic_scene_read_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FURI_LOG_I(TAG, "Stopping poller");
    nfc_poller_stop(app->nfc_poller);
    nfc_poller_free(app->nfc_poller);
}
