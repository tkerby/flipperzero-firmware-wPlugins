#include "nfc_comparator.h"

static bool nfc_comparator_custom_callback(void* context, uint32_t custom_event) {
   furi_assert(context);
   NfcComparator* nfc_comparator = context;
   return scene_manager_handle_custom_event(nfc_comparator->scene_manager, custom_event);
}

static bool nfc_comparator_back_event_callback(void* context) {
   furi_assert(context);
   NfcComparator* nfc_comparator = context;
   return scene_manager_handle_back_event(nfc_comparator->scene_manager);
}

static NfcComparator* nfc_comparator_alloc() {
   NfcComparator* nfc_comparator = malloc(sizeof(NfcComparator));
   furi_assert(nfc_comparator);
   nfc_comparator->scene_manager =
      scene_manager_alloc(&nfc_comparator_scene_handlers, nfc_comparator);
   nfc_comparator->view_dispatcher = view_dispatcher_alloc();

   nfc_comparator->submenu = submenu_alloc();

   nfc_comparator->file_browser_output = furi_string_alloc();
   nfc_comparator->file_browser = file_browser_alloc(nfc_comparator->file_browser_output);

   nfc_comparator->popup = popup_alloc();

   nfc_comparator->notification_app = furi_record_open(RECORD_NOTIFICATION);

   view_dispatcher_set_event_callback_context(nfc_comparator->view_dispatcher, nfc_comparator);
   view_dispatcher_set_custom_event_callback(
      nfc_comparator->view_dispatcher, nfc_comparator_custom_callback);
   view_dispatcher_set_navigation_event_callback(
      nfc_comparator->view_dispatcher, nfc_comparator_back_event_callback);

   view_dispatcher_add_view(
      nfc_comparator->view_dispatcher,
      NfcComparatorView_Submenu,
      submenu_get_view(nfc_comparator->submenu));
   view_dispatcher_add_view(
      nfc_comparator->view_dispatcher,
      NfcComparatorView_FileBrowser,
      file_browser_get_view(nfc_comparator->file_browser));
   view_dispatcher_add_view(
      nfc_comparator->view_dispatcher,
      NfcComparatorView_Popup,
      popup_get_view(nfc_comparator->popup));

   return nfc_comparator;
}

static void nfc_comparator_free(NfcComparator* nfc_comparator) {
   furi_assert(nfc_comparator);

   view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_Submenu);
   view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_FileBrowser);
   view_dispatcher_remove_view(nfc_comparator->view_dispatcher, NfcComparatorView_Popup);

   scene_manager_free(nfc_comparator->scene_manager);
   view_dispatcher_free(nfc_comparator->view_dispatcher);

   submenu_free(nfc_comparator->submenu);
   file_browser_free(nfc_comparator->file_browser);
   furi_string_free(nfc_comparator->file_browser_output);
   popup_free(nfc_comparator->popup);
   furi_record_close(RECORD_NOTIFICATION);

   free(nfc_comparator);
}

void nfc_comparator_set_log_level() {
#ifdef FURI_DEBUG
   furi_log_set_level(FuriLogLevelTrace);
#else
   furi_log_set_level(FuriLogLevelInfo);
#endif
}

int32_t nfc_comparator_main(void* p) {
   UNUSED(p);

   NfcComparator* nfc_comparator = nfc_comparator_alloc();

   nfc_comparator_set_log_level();

   Gui* gui = furi_record_open(RECORD_GUI);
   view_dispatcher_attach_to_gui(
      nfc_comparator->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
   scene_manager_next_scene(nfc_comparator->scene_manager, NfcComparatorScene_MainMenu);
   view_dispatcher_run(nfc_comparator->view_dispatcher);

   furi_record_close(RECORD_GUI);
   nfc_comparator_free(nfc_comparator);

   return 0;
}

NotificationMessage blink_message_normal = {
   .type = NotificationMessageTypeLedBlinkStart,
   .data.led_blink.color = LightBlue | LightGreen,
   .data.led_blink.on_time = 10,
   .data.led_blink.period = 100};
const NotificationSequence blink_sequence_normal = {
   &blink_message_normal,
   &message_do_not_reset,
   NULL};

NotificationMessage blink_message_complete = {
   .type = NotificationMessageTypeLedBlinkStart,
   .data.led_blink.color = LightGreen};
const NotificationSequence blink_sequence_complete = {
   &blink_message_complete,
   &message_do_not_reset,
   NULL};

NotificationMessage blink_message_error = {
   .type = NotificationMessageTypeLedBlinkStart,
   .data.led_blink.color = LightRed};
const NotificationSequence blink_sequence_error = {
   &blink_message_error,
   &message_do_not_reset,
   NULL};

void start_led(NfcComparator* nfc_comparator, NfcComparatorLedState state) {
   switch(state) {
   case NfcComparatorLedState_Running:
      notification_message_block(nfc_comparator->notification_app, &blink_sequence_normal);
      break;
   case NfcComparatorLedState_complete:
      notification_message_block(nfc_comparator->notification_app, &blink_sequence_complete);
      break;
   case NfcComparatorLedState_error:
      notification_message_block(nfc_comparator->notification_app, &blink_sequence_error);
      break;
   default:
      break;
   }
}

void stop_led(NfcComparator* nfc_comparator) {
   notification_message_block(nfc_comparator->notification_app, &sequence_blink_stop);
}
