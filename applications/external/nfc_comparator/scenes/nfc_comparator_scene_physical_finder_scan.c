#include "../nfc_comparator.h"

static volatile bool force_quit = false;

void nfc_comparator_physical_finder_scan_scene_on_enter(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    force_quit = false;

    popup_set_context(nfc_comparator->views.popup, nfc_comparator);
    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Popup);

    nfc_comparator->finder.compare_checks.nfc_card_path = furi_string_alloc();

    nfc_comparator->finder.worker = nfc_comparator_finder_worker_alloc(
        &nfc_comparator->finder.compare_checks, &nfc_comparator->finder.settings);

    nfc_comparator_finder_worker_start(nfc_comparator->finder.worker);
    nfc_comparator_led_worker_start(
        nfc_comparator->notification_app, NfcComparatorLedState_Running);
}

bool nfc_comparator_physical_finder_scan_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        force_quit = true;
        nfc_comparator_finder_worker_stop(nfc_comparator->finder.worker);
        scene_manager_search_and_switch_to_previous_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_FinderMenu);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeTick) {
        switch(nfc_comparator_finder_worker_get_state(nfc_comparator->finder.worker)) {
        case NfcComparatorFinderWorkerState_Scanning:
            popup_set_header(
                nfc_comparator->views.popup, "Scanning....", 64, 5, AlignCenter, AlignTop);
            popup_set_text(
                nfc_comparator->views.popup,
                "Hold card next\nto Flipper's back",
                64,
                40,
                AlignCenter,
                AlignTop);
            break;
        case NfcComparatorFinderWorkerState_Polling:
            popup_set_header(
                nfc_comparator->views.popup, "Polling....", 64, 5, AlignCenter, AlignTop);
            break;
        case NfcComparatorFinderWorkerState_Finding:
            popup_set_header(
                nfc_comparator->views.popup, "Finding....", 64, 5, AlignCenter, AlignTop);
            break;
        case NfcComparatorFinderWorkerState_Stopped:
            FURI_LOG_I("NFC Comparator", "Stopped");
            if(!force_quit) {
                nfc_comparator_finder_worker_stop(nfc_comparator->finder.worker);

                FURI_LOG_I(
                    "NFC Comparator",
                    "UID: %s\nUID length: %s\nProtocol: %s\nPath: %s",
                    nfc_comparator->finder.compare_checks.uid ? "Match" : "Mismatch",
                    nfc_comparator->finder.compare_checks.uid_length ? "Match" : "Mismatch",
                    nfc_comparator->finder.compare_checks.protocol ? "Match" : "Mismatch",
                    furi_string_get_cstr(nfc_comparator->finder.compare_checks.nfc_card_path));

                scene_manager_next_scene(
                    nfc_comparator->scene_manager, NfcComparatorScene_PhysicalFinderResults);
            }
            consumed = true;
            break;
        default: {
            break;
        }
        }
    }
    return consumed;
}

void nfc_comparator_physical_finder_scan_scene_on_exit(void* context) {
    furi_assert(context);
    NfcComparator* nfc_comparator = context;
    popup_reset(nfc_comparator->views.popup);
    nfc_comparator_led_worker_stop(nfc_comparator->notification_app);
    nfc_comparator_finder_worker_free(nfc_comparator->finder.worker);
}
