#pragma once

#include <gui/view.h>
#include <notification/notification.h>

View* drop_view_alloc(void);
void drop_view_free(View* view);
void drop_view_tick(View* view, uint32_t frame);
void drop_view_reset(View* view);

// hook for the audiovisual cue at the "DROP!" beat of the countdown
void drop_view_set_notification(NotificationApp* notif);
