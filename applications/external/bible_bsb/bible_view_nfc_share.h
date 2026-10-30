#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include "bible_app.h"

/* ============================================================================
 * NfcShare view — NFC emission waiting screen
 *
 * Draw header "NFC Share" or "Export NFC", "Hold near reader..." with
 * animated pulsing dots, URL preview (truncated 28+...), "Back to cancel".
 *
 * Handle Back (Short): stop NFC emulation, return to Reader or Collection.
 * ============================================================================ */

/**
 * Draw the NfcShare view.
 */
void bible_bsb_view_nfc_share_draw(Canvas* canvas, void* ctx);

/**
 * Handle NfcShare input: Back (Short) stops emulation and returns.
 */
bool bible_bsb_view_nfc_share_input(InputEvent* event, void* ctx);
