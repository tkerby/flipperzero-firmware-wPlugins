#include "nfc_comparator_led_worker.h"

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

void start_led(NotificationApp* notification_app, NfcComparatorLedState state) {
   switch(state) {
   case NfcComparatorLedState_Running:
      notification_message_block(notification_app, &blink_sequence_normal);
      break;
   case NfcComparatorLedState_complete:
      notification_message_block(notification_app, &blink_sequence_complete);
      break;
   case NfcComparatorLedState_error:
      notification_message_block(notification_app, &blink_sequence_error);
      break;
   default:
      break;
   }
}

void stop_led(NotificationApp* notification_app) {
   notification_message_block(notification_app, &sequence_blink_stop);
}
