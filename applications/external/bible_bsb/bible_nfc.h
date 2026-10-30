#pragma once

#include "bible_state.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * NFC emulation module
 *
 * Full implementation will be provided by the nfc-share-view feature.
 * This header defines the contract so ActionMenu can call
 * bible_nfc_start_url() today.
 * ============================================================================ */

/**
 * Start NFC URL emulation with the given route.bible URL.
 *
 * Sets up NTAG215 NDEF URI record and starts the NFC listener.
 *
 * @param state  app state (nfc_url must already be set)
 * @return true if emulation started successfully
 */
bool bible_nfc_start_url(BibleAppState* state);

/**
 * Start NFC text emulation with the given plain text payload.
 *
 * Sets up NTAG215 NDEF Text record and starts the NFC listener.
 * Used for bulk JSON export from the Collection view.
 *
 * @param state  app state
 * @param text   null-terminated text payload (must fit in NTAG215 capacity)
 * @return true if emulation started successfully
 */
bool bible_nfc_start_text(BibleAppState* state, const char* text);

/**
 * Stop NFC emulation and clean up resources.
 *
 * @param state  app state
 */
void bible_nfc_stop(BibleAppState* state);
