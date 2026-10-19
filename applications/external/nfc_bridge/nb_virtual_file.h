/*
 * nb_virtual_file.h - read a card dump / .nfc file from the Flipper's storage into an NbVirtual.
 */
#pragma once
#include "core/nb_virtual.h"

/* Loads the file at `path` (e.g. "/ext/nfc/card.nfc"). On false, nbv_error(v) says why. */
bool nbv_load_path(NbVirtual* v, const char* path);
bool nbv_save_path(NbVirtual* v, const char* path);
