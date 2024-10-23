#include "../helpers/aic.h"
#include "../helpers/hex.h"
#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>

enum PollerResult {
    PollerResultSuccess,
    PollerResultFailed,
};

static NfcCommand flip_aic_scene_read_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FelicaPollerEvent* poller_event = event.event_data;
    FURI_LOG_I(TAG, "Poller event: %d", poller_event->type);

    switch (poller_event->type) {
    case FelicaPollerEventTypeReady:
    case FelicaPollerEventTypeIncomplete:
        view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultSuccess);
        return NfcCommandStop;

    case FelicaPollerEventTypeError:
        FURI_LOG_I(TAG, "Error: %d", poller_event->data->error);
        view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultFailed);
        return NfcCommandStop;

    case FelicaPollerEventTypeRequestAuthContext:
        poller_event->data->auth_context->skip_auth = true;
        return NfcCommandContinue;
    }

    return NfcCommandContinue; // why is this needed????
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
        case PollerResultSuccess:
            const FelicaData* data = nfc_poller_get_data(app->nfc_poller);

            char* idm = to_hex_alloc(data->idm.data, sizeof(data->idm.data));
            FURI_LOG_I(TAG, "IDm: %s", idm);
            free(idm);

            char* pmm = to_hex_alloc(data->pmm.data, sizeof(data->pmm.data));
            FURI_LOG_I(TAG, "PMm: %s", pmm);
            free(pmm);

            uint8_t spad0[16];
            aic_access_code_decrypt(data->data.fs.spad[0].data, spad0);

            char access_code[3 * 10];
            to_hex(access_code, sizeof(access_code), spad0 + 6, 10);
            FURI_LOG_I(TAG, "Access code: %s", access_code);

            scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
            return true;

        case PollerResultFailed:
            scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
            return true;
        }
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
