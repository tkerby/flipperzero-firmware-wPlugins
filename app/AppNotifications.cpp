#ifndef _APP_NOTIFICATIONS_
#define _APP_NOTIFICATIONS_

#include "lib/hardware/notification/Notification.cpp"

const NotificationSequence NOTIFICATION_PAGER_RECEIVE = {
    &message_vibro_on,
    &message_note_e6,

    &message_blue_255,
    &message_delay_50,

    &message_sound_off,
    &message_vibro_off,

    NULL,
};

#endif //_APP_NOTIFICATIONS_
