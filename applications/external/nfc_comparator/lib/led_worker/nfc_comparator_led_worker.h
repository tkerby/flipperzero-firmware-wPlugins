#pragma once

#include <notification/notification_messages.h>

typedef enum {
   NfcComparatorLedState_Running,
   NfcComparatorLedState_complete,
   NfcComparatorLedState_error
} NfcComparatorLedState;

void start_led(NotificationApp* notification_app, NfcComparatorLedState state);
void stop_led(NotificationApp* notification_app);
