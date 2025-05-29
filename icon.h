#pragma once

#include <furi.h>

typedef struct IEIcon {
    size_t width;
    size_t height;
    uint8_t* data;
    FuriString* name;
} IEIcon;

IEIcon* icon_alloc();
void icon_free(IEIcon* icon);

// Clear all icon data, resetting to blank icon
void icon_reset(IEIcon* icon);
