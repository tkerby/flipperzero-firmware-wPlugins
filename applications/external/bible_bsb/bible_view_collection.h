#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include "bible_app.h"

/* ============================================================================
 * Collection view — saved passages list
 *
 * Draw header 'Collection >', scrollable list of saved passage display refs
 * with inverted highlight, empty state message, bottom hint 'L=export OK=read'.
 *
 * Handle inputs: Up/Down scroll, Left->NFC bulk export, Right->BookList,
 * OK Short->load saved passage and go to Reader, OK Long->delete selected
 * passage with 'Deleted' toast, Back->BookList.
 * ============================================================================ */

/**
 * Draw the Collection view: header, list of saved passages, empty state,
 * scroll indicator, bottom hint.
 */
void bible_bsb_view_collection_draw(Canvas* canvas, void* ctx);

/**
 * Handle Collection view input: Up/Down scroll, Left=NFC export, Right=BookList,
 * OK Short=read passage, OK Long=delete passage, Back=BookList.
 */
bool bible_bsb_view_collection_input(InputEvent* event, void* ctx);
