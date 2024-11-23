#pragma once

#include "pinball0.h"

// Read game settings from .pinball0.conf
void pinball_load_settings(PinballApp* pb);

// Save game settings to .pinball0.conf
void pinball_save_settings(PinballApp* pb);
