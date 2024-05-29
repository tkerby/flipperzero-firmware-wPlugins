#pragma once

#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <m-array.h>

#include "custom_event.h"
#include "app_menu.h"
#include "../app_icons.h"

ARRAY_DEF(ViewIdsArray, uint32_t, M_PTR_OPLIST);

struct AppMenu {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Popup* popup;
    ViewIdsArray_t view_ids; // List of view ids for each menu item
    AppMenuCallback callback;
    void* callback_context;
};