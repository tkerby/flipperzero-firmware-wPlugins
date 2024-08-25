#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_common.h>
#include <nfc/nfc_device.h>

#include "../nfc_eink_tag.h"
#include "nfc_eink_types.h"

//const char* nfc_eink_screen_get_name(NfcEinkType type); ///TODO: move to
const char* nfc_eink_screen_get_manufacturer_name(NfcEinkManufacturer type);

NfcEinkScreen* nfc_eink_screen_alloc(NfcEinkManufacturer manufacturer);
void nfc_eink_screen_init(NfcEinkScreen* screen, NfcEinkType type);
void nfc_eink_screen_free(NfcEinkScreen* screen);

///TODO: maybe this function can be moved to nfc_eink_screen_alloc as a number of parameters
void nfc_eink_screen_set_callback(
    NfcEinkScreen* screen,
    NfcEinkScreenEventCallback event_callback,
    NfcEinkScreenEventContext event_context);

bool nfc_eink_screen_save(const NfcEinkScreen* screen, const char* file_path);
bool nfc_eink_screen_load(const char* file_path, NfcEinkScreen** screen);
bool nfc_eink_screen_delete(const char* file_path);
//bool nfc_eink_screen_load_descriptor(const char* file_path, NfcEinkScreenDescriptor** screen);