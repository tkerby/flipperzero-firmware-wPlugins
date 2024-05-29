#pragma once

#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>

#include "action_config.h"
#include "app_menu.h"
#include "flipboard.h"
#include "flipboard_file.h"
#include "flipboard_model.h"
#include "keyboard.h"
#include "leds.h"

/**
 * @brief    The flipboard struct.
 * @details  This struct contains all the data needed for the flipboard;
 *          the View Dispatcher, the UI components and the FlipboardModel.
 */
struct Flipboard {
    ViewDispatcher* view_dispatcher;

    AppMenu* app_menu;
    ActionConfig* action_config;
    View* view_primary;
    Widget* widget_about;
    Widget* widget_qr;

    FlipboardModel* model;
};

/**
 * @brief    The flipboard view id enum.
 * @details  This enum contains all the view ids for the flipboard.
 */
typedef enum {
    FlipboardViewAppMenuId, // The first entry must be the app menu
    FlipboardViewConfigureId,
    FlipboardViewConfigureSubviewId,
    FlipboardViewConfigureTextInputId,
    FlipboardViewConfigureKeystrokeSelectorId,

    FlipboardViewAboutId,
    FlipboardViewPrimaryId,
    FlipboardViewQRId,
} FlipboardViewId;
