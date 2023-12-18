#include <stdint.h>
#include "usb_can_app_i.hpp"
int32_t usb_can_app_start(void* p) {
    usb_can_app(p);
    return 0;
}
