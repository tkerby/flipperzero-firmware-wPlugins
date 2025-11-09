#pragma once

#include <strings.h> // for size_t
#include "../icon.h"
#include "../iconedit.h"

void send_usb_start(IEIcon* icon);
void send_usb_set_update_callback(IconEditUpdateCallback callback, void* context);
