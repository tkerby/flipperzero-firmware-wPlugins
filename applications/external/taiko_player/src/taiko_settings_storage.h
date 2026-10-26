// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include "taiko_settings.h"
#include <storage/storage.h>

#define TAIKO_SETTINGS_PATH EXT_PATH("apps_data/taiko_player/settings.ff")

/* Always initializes settings. False means missing/invalid fields were defaulted. */
bool taiko_settings_load(Storage* storage, TaikoSettings* settings);
bool taiko_settings_save(Storage* storage, const TaikoSettings* settings);
