#pragma once

#include "bambu_rfid.h"

#include <stddef.h>
#include <stdint.h>

// Derive all Bambu Key A/Key B values for a completed batch of UIDs, then
// update Flipper's native MIFARE Classic user dictionary with one dictionary
// open/scan/close cycle. The caller should collect/deduplicate UIDs first.
bool br_add_uid_batch_to_user_dictionary(const uint8_t (*uids)[BR_UID_SIZE], size_t uid_count);
