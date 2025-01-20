
#include "../nfc_comparator.h"

void nfc_comparator_comparator_scene_on_enter(void* context) {
    NfcComparator* nfc_comparator = context;
    furi_assert(nfc_comparator);

    popup_set_context(nfc_comparator->popup, nfc_comparator);
    view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Popup);

    NfcComparatorReaderWorker* worker = nfc_comparator_reader_worker_alloc();
    nfc_comparator_reader_worker_set_loaded_nfc_card(
        worker, furi_string_get_cstr(nfc_comparator->file_browser_output));
    nfc_comparator_reader_worker_start(worker);

    while(nfc_comparator_reader_worker_is_running(worker)) {
        switch(nfc_comparator_reader_worker_get_state(worker)) {
        case NfcComparatorReaderWorkerState_Scanning:
            popup_set_header(nfc_comparator->popup, "Scanning....", 64, 5, AlignCenter, AlignTop);
            break;
        case NfcComparatorReaderWorkerState_Polling:
            popup_set_header(nfc_comparator->popup, "Polling....", 64, 5, AlignCenter, AlignTop);
            break;
        case NfcComparatorReaderWorkerState_Comparing:
            popup_set_header(nfc_comparator->popup, "Comparing....", 64, 5, AlignCenter, AlignTop);
            break;
        default:
            break;
        }
        furi_delay_ms(100);
    }

    nfc_comparator_reader_worker_stop(worker);

    popup_set_header(nfc_comparator->popup, "Compare Results", 64, 5, AlignCenter, AlignTop);

    NfcComparatorReaderWorkerCompareChecks checks =
        nfc_comparator_reader_worker_get_compare_checks(worker);
    nfc_comparator_reader_worker_free(worker);

    FuriString* comparator = furi_string_alloc();
    furi_string_printf(
        comparator,
        "UID: %s\nUID length: %s\nProtocol: %s",
        checks.uid ? "Match" : "Mismatch",
        checks.uid_length ? "Match" : "Mismatch",
        checks.protocol ? "Match" : "Mismatch");

    char result_buffer[158];
    strncpy(result_buffer, furi_string_get_cstr(comparator), sizeof(result_buffer) - 1);

    furi_string_free(comparator);

    popup_set_text(nfc_comparator->popup, result_buffer, 64, 35, AlignCenter, AlignCenter);
}

bool nfc_comparator_comparator_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

void nfc_comparator_comparator_scene_on_exit(void* context) {
    NfcComparator* nfc_comparator = context;
    popup_reset(nfc_comparator->popup);
}
