#pragma once

#include "../xremote.h"

#define XREMOTE_SETTINGS_FILE_VERSION      2
#define CONFIG_FILE_DIRECTORY_PATH         EXT_PATH("apps_data/xremote")
#define XREMOTE_SETTINGS_SAVE_PATH         CONFIG_FILE_DIRECTORY_PATH "/xremote.conf"
#define XREMOTE_SETTINGS_SAVE_PATH_TMP     XREMOTE_SETTINGS_SAVE_PATH ".tmp"
#define XREMOTE_SETTINGS_HEADER            "Xremote Config File"
#define XREMOTE_SETTINGS_KEY_HAPTIC        "Haptic"
#define XREMOTE_SETTINGS_KEY_LED           "Led"
#define XREMOTE_SETTINGS_KEY_SPEAKER       "Speaker"
#define XREMOTE_SETTINGS_KEY_SAVE_SETTINGS "SaveSettings"
#define XREMOTE_SETTINGS_KEY_IR_TIMING     "IRTiming"
#define XREMOTE_SETTINGS_KEY_IR_TX_PIN     "IRTXPin"
#define XREMOTE_SETTINGS_KEY_IR_USE_OTP    "IRUSEOTP"
#define XREMOTE_SETTINGS_KEY_SG_TIMING     "SGTiming"
#define XREMOTE_SETTINGS_KEY_LOOP_TRANSMIT "LoopTransmit"

void xremote_save_settings(void* context);
void xremote_read_settings(void* context);
