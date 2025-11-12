#pragma once

#include "../icon.h"

// Returns icon as text string, binary files are in hex bytes (i.e. "013CFF...")
FuriString* c_file_generate(IEIcon* icon);
FuriString* bmx_file_generate(IEIcon* icon);
FuriString* png_file_generate(IEIcon* icon);

// File writers
bool c_file_save(IEIcon* icon);
bool xbm_file_save(IEIcon* icon);
bool png_file_save(IEIcon* icon);
bool bmx_file_save(IEIcon* icon);

// bool xbm_file_open(IEIcon* icon, filename);
IEIcon* png_file_open(const char* icon_path);
IEIcon* bmx_file_open(const char* icon_path);
