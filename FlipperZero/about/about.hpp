#pragma once

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <furi.h>
#include <memory>
#include "easy_flipper/easy_flipper.h"

class FreeRoamApp;

class FreeRoamAbout
{
private:
    Widget *widget = nullptr;
    ViewDispatcher **viewDispatcherRef = nullptr;
    void *appContext = nullptr;

    // Static callback wrappers
    static uint32_t callbackToSubmenu(void *context);

public:
    FreeRoamAbout();
    ~FreeRoamAbout();

    bool init(ViewDispatcher **viewDispatcher, void *appContext);
    void free();
};
