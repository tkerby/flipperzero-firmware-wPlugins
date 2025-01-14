
#include "../nfc_comparator.h"

void nfc_comparator_results_menu_callback(void* context, uint32_t index) {
   NfcComparator* nfc_comparator = context;
   scene_manager_handle_custom_event(nfc_comparator->scene_manager, index);
}

void nfc_comparator_results_scene_on_enter(void* context) {
   NfcComparator* nfc_comparator = context;

   popup_set_context(nfc_comparator->popup, nfc_comparator);
   popup_set_header(nfc_comparator->popup, "Compare Results", 64, 5, AlignCenter, AlignTop);

   FuriString* results = furi_string_alloc_printf(
      "UID: %s\nProtocol:%s",
      nfc_comparator->compare_results->uid ? "true" : "false",
      nfc_comparator->compare_results->protocol ? "true" : "false");

   popup_set_text(
      nfc_comparator->popup, furi_string_get_cstr(results), 64, 50, AlignCenter, AlignCenter);
      
   view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_Popup);
   furi_string_free(results);
}

bool nfc_comparator_results_scene_on_event(void* context, SceneManagerEvent event) {
   UNUSED(context);
   UNUSED(event);
   bool consumed = false;
   return consumed;
}

void nfc_comparator_results_scene_on_exit(void* context) {
   NfcComparator* nfc_comparator = context;
   popup_reset(nfc_comparator->popup);
}
