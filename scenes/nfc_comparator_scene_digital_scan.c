#include "../nfc_comparator.h"

static void nfc_comparator_digital_scan_menu_callback(void* context) {
   furi_assert(context);
   NfcComparator* nfc_comparator = context;

   furi_string_swap(
      nfc_comparator->views.file_browser.output, nfc_comparator->views.file_browser.tmp_output);

   NfcDevice* nfc_card_1 = nfc_device_alloc();
   if(!nfc_device_load(
         nfc_card_1, furi_string_get_cstr(nfc_comparator->views.file_browser.output))) {
      nfc_device_free(nfc_card_1);
      scene_manager_next_scene(
         nfc_comparator->scene_manager, NfcComparatorScene_FailedToLoadNfcCard);
      return;
   }

   NfcDevice* nfc_card_2 = nfc_device_alloc();
   if(!nfc_device_load(
         nfc_card_2, furi_string_get_cstr(nfc_comparator->views.file_browser.tmp_output))) {
      nfc_device_free(nfc_card_2);
      scene_manager_next_scene(
         nfc_comparator->scene_manager, NfcComparatorScene_FailedToLoadNfcCard);
      return;
   }

   nfc_comparator->worker.compare_checks.protocol = nfc_device_get_protocol(nfc_card_1) == nfc_device_get_protocol(nfc_card_2);

   // Get UIDs and lengths
   size_t poller_uid_len = 0;
   const uint8_t* poller_uid = nfc_device_get_uid(nfc_card_1, &poller_uid_len);

   size_t loaded_uid_len = 0;
   const uint8_t* loaded_uid = nfc_device_get_uid(nfc_card_2, &loaded_uid_len);

   // Compare UID length
   nfc_comparator->worker.compare_checks.uid_length = (poller_uid_len == loaded_uid_len);

   // Compare UID bytes if lengths match
   if(nfc_comparator->worker.compare_checks.uid_length && poller_uid && loaded_uid) {
      nfc_comparator->worker.compare_checks.uid = (memcmp(poller_uid, loaded_uid, poller_uid_len) == 0);
   } else {
      nfc_comparator->worker.compare_checks.uid = false;
   }

   // compare NFC data
   nfc_comparator->worker.compare_checks.nfc_data = nfc_device_is_equal(nfc_card_1, nfc_card_2);

   nfc_device_free(nfc_card_1);
   nfc_device_free(nfc_card_2);

   furi_string_reset(nfc_comparator->views.file_browser.tmp_output);

   scene_manager_next_scene(nfc_comparator->scene_manager, NfcComparatorScene_DigitalResults);
}

void nfc_comparator_digital_scan_scene_on_enter(void* context) {
   furi_assert(context);
   NfcComparator* nfc_comparator = context;

   nfc_comparator_led_worker_start(nfc_comparator->notification_app, NfcComparatorLedState_Running);

   furi_string_swap(
      nfc_comparator->views.file_browser.output, nfc_comparator->views.file_browser.tmp_output);

   file_browser_configure(
      nfc_comparator->views.file_browser.view,
      ".nfc",
      NFC_ITEM_LOCATION,
      true,
      true,
      &I_Nfc_10px,
      true);
   file_browser_set_callback(
      nfc_comparator->views.file_browser.view,
      nfc_comparator_digital_scan_menu_callback,
      nfc_comparator);
   FuriString* tmp_str = furi_string_alloc_set_str(NFC_ITEM_LOCATION);
   file_browser_start(nfc_comparator->views.file_browser.view, tmp_str);
   furi_string_free(tmp_str);

   view_dispatcher_switch_to_view(nfc_comparator->view_dispatcher, NfcComparatorView_FileBrowser);
}

bool nfc_comparator_digital_scan_scene_on_event(void* context, SceneManagerEvent event) {
   UNUSED(event);
   UNUSED(context);
   return false;
}

void nfc_comparator_digital_scan_scene_on_exit(void* context) {
   furi_assert(context);
   NfcComparator* nfc_comparator = context;
   file_browser_stop(nfc_comparator->views.file_browser.view);
   nfc_comparator_led_worker_stop(nfc_comparator->notification_app);
}
