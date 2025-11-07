#pragma once

#include "../icon.h"

// Returns icon as C code text string
FuriString* c_file_generate(IEIcon* icon);

// File writers
bool c_file_save(IEIcon* icon);
bool xbm_file_save(IEIcon* icon);
bool png_file_save(IEIcon* icon);

// bool xbm_file_open(IEIcon* icon, filename);
IEIcon* png_file_open(const char* icon_path);
