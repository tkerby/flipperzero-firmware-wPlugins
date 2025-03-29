#include "../nfc_comparator.h"

int32_t nfc_comparator_comparator_task(void* context) {
    NfcComparator* nfc_comparator = context;
    furi_assert(nfc_comparator);

    popup_set_context(nfc_comparator->popup, nfc_comparator);
    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Popup);

    if(!nfc_comparator_reader_worker_set_loaded_nfc_card(
           nfc_comparator->nfc_comparator_reader_worker,
           furi_string_get_cstr(nfc_comparator->file_browser_output))) {
        popup_set_header(
            nfc_comparator->popup, "Failed to load\nselected card", 64, 5, AlignCenter, AlignTop);
    } else {
        nfc_comparator_reader_worker_start(nfc_comparator->nfc_comparator_reader_worker);

        start_led(nfc_comparator->notification_app, NfcComparatorLedState_Running);

        while(
            nfc_comparator_reader_worker_get_state(nfc_comparator->nfc_comparator_reader_worker) !=
            NfcComparatorReaderWorkerState_Stopped) {
            switch(nfc_comparator_reader_worker_get_state(
                nfc_comparator->nfc_comparator_reader_worker)) {
            case NfcComparatorReaderWorkerState_Scanning:
                popup_set_header(
                    nfc_comparator->popup, "Scanning....", 64, 5, AlignCenter, AlignTop);
                break;
            case NfcComparatorReaderWorkerState_Polling:
                popup_set_header(
                    nfc_comparator->popup, "Polling....", 64, 5, AlignCenter, AlignTop);
                break;
            case NfcComparatorReaderWorkerState_Comparing:
                popup_set_header(
                    nfc_comparator->popup, "Comparing....", 64, 5, AlignCenter, AlignTop);
                break;
            default:
                break;
            }
            furi_delay_ms(100);
        }

        nfc_comparator_reader_worker_stop(nfc_comparator->nfc_comparator_reader_worker);

        start_led(nfc_comparator->notification_app, NfcComparatorLedState_complete);

        popup_set_header(nfc_comparator->popup, "Compare Results", 64, 5, AlignCenter, AlignTop);

        NfcComparatorReaderWorkerCompareChecks checks =
            nfc_comparator_reader_worker_get_compare_checks(
                nfc_comparator->nfc_comparator_reader_worker);

        FuriString* comparator = furi_string_alloc();
        furi_string_printf(
            comparator,
            "UID: %s\nUID length: %s\nProtocol: %s",
            checks.uid ? "Match" : "Mismatch",
            checks.uid_length ? "Match" : "Mismatch",
            checks.protocol ? "Match" : "Mismatch");

        popup_set_text(
            nfc_comparator->popup,
            furi_string_get_cstr(comparator),
            64,
            35,
            AlignCenter,
            AlignCenter);
        furi_delay_ms(5);
        furi_string_free(comparator);
    }
    return 0;
}

void nfc_comparator_comparator_setup(void* context) {
    NfcComparator* nfc_comparator = context;
    nfc_comparator->thread = furi_thread_alloc_ex(
        "NfcComparatorComparingWorker", 4096, nfc_comparator_comparator_task, nfc_comparator);
    nfc_comparator->nfc_comparator_reader_worker = nfc_comparator_reader_worker_alloc();
}

void nfc_comparator_comparator_free(void* context) {
    NfcComparator* nfc_comparator = context;
    furi_thread_free(nfc_comparator->thread);
    nfc_comparator_reader_worker_free(nfc_comparator->nfc_comparator_reader_worker);
}

void nfc_comparator_comparator_start(void* context) {
    NfcComparator* nfc_comparator = context;
    furi_thread_start(nfc_comparator->thread);
}

void nfc_comparator_comparator_stop(void* context) {
    NfcComparator* nfc_comparator = context;
    furi_thread_join(nfc_comparator->thread);
}

void nfc_comparator_comparator_scene_on_enter(void* context) {
    NfcComparator* nfc_comparator = context;
    nfc_comparator_comparator_setup(nfc_comparator);
    nfc_comparator_comparator_start(nfc_comparator);
}

bool nfc_comparator_comparator_scene_on_event(void* context, SceneManagerEvent event) {
    NfcComparator* nfc_comparator = context;
    bool consumed = false;
    if(event.event == 0) {
        nfc_comparator_reader_worker_stop(nfc_comparator->nfc_comparator_reader_worker);
        scene_manager_search_and_switch_to_previous_scene(
            nfc_comparator->scene_manager, NfcComparatorScene_MainMenu);
        consumed = true;
    }
    return consumed;
}

void nfc_comparator_comparator_scene_on_exit(void* context) {
    NfcComparator* nfc_comparator = context;
    popup_reset(nfc_comparator->popup);
    stop_led(nfc_comparator->notification_app);
    nfc_comparator_comparator_stop(nfc_comparator);
    nfc_comparator_comparator_free(nfc_comparator);
}
