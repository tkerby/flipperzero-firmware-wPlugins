#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification.h>

typedef enum {
    DropDetectViewSubmenu = 0,
    DropDetectViewDrop,
    DropDetectViewIdle,
    DropDetectViewWalking,
    DropDetectViewFidget,
} DropDetectView;

typedef enum {
    DropDetectMenuDrop = 0,
    DropDetectMenuIdle,
    DropDetectMenuWalking,
    DropDetectMenuFidget,
} DropDetectMenuIndex;

typedef struct DropDetectApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    NotificationApp* notif;

    View* drop_view;
    View* idle_view;
    View* walking_view;
    View* fidget_view;

    uint32_t exit_delay_frames;

    FuriTimer* anim_timer;

    uint32_t frame;
    DropDetectView active_view;

    // Capture state
    bool capturing;
    bool capture_started;  // Track if we've started capture for current view
    uint32_t samples_captured;
    uint32_t total_samples_to_capture;

} DropDetectApp;

void drop_detect_app_request_view(DropDetectApp* app, DropDetectView view);
