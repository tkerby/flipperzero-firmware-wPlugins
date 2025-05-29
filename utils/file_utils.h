#pragma once

#include "../icon.h"

uint8_t* convert_bytes_to_xbm_bits(IEIcon* icon, size_t* size);

void log_xbm_data(uint8_t* data, size_t size);

bool c_file_save(IEIcon* icon);
bool xbm_file_save(IEIcon* icon);
bool png_file_save(IEIcon* icon);

// bool xbm_file_open(IEIcon* icon, filename);
IEIcon* png_file_open(const char* icon_path);
