#pragma once

#include "key_types.h"
#include <furi.h>
#include <lib/ibutton/ibutton_protocols.h>
#include <lib/lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>
#include <lib/lfrfid/lfrfid_worker.h>
#include <lib/nfc/nfc.h>

typedef struct KeyScanner KeyScanner;

KeyScanner* key_scanner_alloc(void);
void key_scanner_free(KeyScanner* scanner);

void key_scanner_start(KeyScanner* scanner);
void key_scanner_stop(KeyScanner* scanner);

void key_scanner_set_mode(KeyScanner* scanner, ScanMode mode);
ScanMode key_scanner_get_mode(KeyScanner* scanner);
KeyCategory key_scanner_get_current_phase(KeyScanner* scanner);

bool key_scanner_poll_step(KeyScanner* scanner, DetectedKey* out_key);
