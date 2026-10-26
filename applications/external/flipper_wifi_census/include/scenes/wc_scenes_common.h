#pragma once

// Helpers used by more than one scene file. Everything else stays private to the file that
// owns its screens - this header is the seam between them, not a dumping ground.

#include "include/app/wc_app.h"
#include "include/application/wc_files.h"
#include "include/scenes/wc_scene.h"

#include <furi.h>

// Selection in a list view -> a custom event carrying the item index.
void wc_list_cb(void *context, uint32_t index);
// Text-input confirmation -> WcCustomEventTextDone.
void wc_text_input_cb(void *context);

// Fill the list view with the stored captures (names land in app->list_names).
void wc_populate_files(WcApp *app, const char *header);

// Show a one-off scrollable message and move to the Msg scene.
void wc_show_message(WcApp *app, const char *text);

// Write why `filename` would not open. A capture holding more devices than this build's ceiling
// is refused by the loader exactly like a corrupt one, and "could not read" then sends you
// hunting for a broken file instead of telling you it is simply bigger than the Flipper.
void wc_explain_load_failure(WcApp *app, const char *filename, char *out, size_t cap);

// Of two captures, explain the one that actually refused. Asking "is the first one small?" gets
// it wrong, because a corrupt file reports no devices just as a small one does.
void wc_explain_which_failed(WcApp *app, const char *first, const char *second);
