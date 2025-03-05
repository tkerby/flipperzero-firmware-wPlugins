#ifndef _NOTIFICATION_CLASS_
#define _NOTIFICATION_CLASS_

#include <furi.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

static NotificationApp* __notification_app_instance = NULL;

class Notification {
private:
    static NotificationApp* getApp() {
        if(__notification_app_instance == NULL) {
            __notification_app_instance = (NotificationApp*)furi_record_open(RECORD_NOTIFICATION);
        }
        return __notification_app_instance;
    }

public:
    static void Play(const NotificationSequence* nullTerminatedSequence) {
        notification_message(getApp(), nullTerminatedSequence);
    }

    static void Dispose() {
        if(__notification_app_instance != NULL) {
            furi_record_close(RECORD_NOTIFICATION);
            __notification_app_instance = NULL;
        }
    }
};

#endif //_NOTIFICATION_CLASS_
