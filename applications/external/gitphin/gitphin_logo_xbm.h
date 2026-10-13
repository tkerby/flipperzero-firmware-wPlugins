#pragma once
/* Gitphin logo for the screen header: a dolphin jumping over a branch arc.
 * 1-bit, XBM order (LSB first per row byte). Drawn by hand, see docs/gitphin_logo.svg. */
#include <stdint.h>

#define GITPHIN_LOGO_W 12
#define GITPHIN_LOGO_H 11

static const uint8_t gitphin_logo_xbm[] = {
    0x20, 0x00, 0xc0, 0x00, 0xf0, 0x03, 0xf8, 0x0f, 0xfc, 0x01, 0x06,
    0x00, 0x02, 0x00, 0x00, 0x00, 0xfc, 0x03, 0x02, 0x04, 0x01, 0x08,
};
