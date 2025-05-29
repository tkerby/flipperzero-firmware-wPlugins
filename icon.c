#include "icon.h"

IEIcon* icon_alloc() {
    IEIcon* icon = (IEIcon*)malloc(sizeof(IEIcon));
    icon->width = 10;
    icon->height = 10;
    icon->data = malloc(icon->width * icon->height * sizeof(uint8_t));
    icon->name = furi_string_alloc_set_str("new_icon");
    return icon;
}

void icon_free(IEIcon* icon) {
    free(icon->data);
    furi_string_free(icon->name);
    free(icon);
}

void icon_reset(IEIcon* icon) {
    memset(icon->data, 0, icon->width * icon->height);
}
