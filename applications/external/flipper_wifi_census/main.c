#include "include/app/wc_app.h"

#include <furi.h>

int32_t wc_app(void *p) {
    UNUSED(p);
    return wc_app_run();
}
