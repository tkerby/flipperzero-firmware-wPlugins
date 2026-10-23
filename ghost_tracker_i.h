#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>

#include "views/view_menu.h"
#include "views/view_emf.h"
#include "views/view_spirit.h"
#include "views/view_rem.h"
#include "views/view_about.h"

typedef enum {
    GhostTrackerViewMenu,
    GhostTrackerViewEmf,
    GhostTrackerViewSpirit,
    GhostTrackerViewRem,
    GhostTrackerViewAbout,
} GhostTrackerViewId;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    MenuView* menu_view;

    EmfView* emf_view;
    SpiritView* spirit_view;
    RemView* rem_view;
    AboutView* about_view;

    GhostTrackerViewId current_view;
} GhostTrackerApp;
