
#pragma once
#include "_base.h"

// MagicBand+ protocol integration for OFW-BLE-SPAM
// Provides a full library of EMCOT examples + E9-05 single-color palette with/without vibration.
// Disney Bluetooth SIG Company ID: 0x0183 (payload little endian 0x83 0x01).
// Advertising only (no services).

typedef enum {
    MB_Cat_CC = 0,
    MB_Cat_E905_Single,
    MB_Cat_E906_Dual,
    MB_Cat_E907_Unknown,
    MB_Cat_E908_6bit,
    MB_Cat_E909_5Palette,
    MB_Cat_E90B_Circle,
    MB_Cat_E90C_Animations,
    MB_Cat_E90E_Examples,
    MB_Cat_E90F_Examples,
    MB_Cat_E910_Alternating,
    MB_Cat_E911_Crossfade,
    MB_Cat_E912_CircleVibe,
    MB_Cat_E913_Examples,
    MB_Cat_E914_Examples,
    MB_Cat_COUNT
} MagicbandCategory;

typedef struct {
    uint8_t category; // MagicbandCategory
    uint8_t index; // example index (for categories with example lists)
    uint8_t color5; // 0..31 for E9-05
    bool vibe_on; // applies to E9-05
} MagicbandCfg;

extern const Protocol protocol_magicband;
