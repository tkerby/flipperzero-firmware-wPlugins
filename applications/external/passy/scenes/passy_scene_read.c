#include "../passy_i.h"
#include "../passy_reader.h"
#include <dolphin/dolphin.h>

#define TAG "PassySceneRead"

void passy_scene_read_on_enter(void* context) {
    Passy* passy = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = passy->popup;
    popup_set_header(popup, "Reading", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    passy->poller = nfc_poller_alloc(passy->nfc, NfcProtocolIso14443_4b);
    nfc_poller_start(passy->poller, passy_reader_poller_callback, passy);

    passy_blink_start(passy);

    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewPopup);
}

bool passy_scene_read_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;
    Popup* popup = passy->popup;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PassyCustomEventReaderSuccess) {
            scene_manager_next_scene(passy->scene_manager, PassySceneReadSuccess);
            consumed = true;
        } else if(event.event == PassyCustomEventReaderError) {
            scene_manager_next_scene(passy->scene_manager, PassySceneReadError);
            consumed = true;
        } else if(event.event == PassyCustomEventReaderDetected) {
            popup_set_header(popup, "Detected", 68, 30, AlignLeft, AlignTop);
        } else if(event.event == PassyCustomEventReaderAuthenticated) {
            popup_set_header(popup, "Authenticated", 68, 30, AlignLeft, AlignTop);
        } else if(event.event == PassyCustomEventReaderReading) {
            popup_set_header(popup, "Reading", 68, 30, AlignLeft, AlignTop);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            passy->scene_manager, PassySceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void passy_scene_read_on_exit(void* context) {
    Passy* passy = context;

    if(passy->poller) {
        nfc_poller_stop(passy->poller);
        nfc_poller_free(passy->poller);
        passy->poller = NULL;
    }

    // Clear view
    popup_reset(passy->popup);

    passy_blink_stop(passy);
}
