#pragma once

#include "key_types.h"
#include <storage/storage.h>

bool key_saver_save_to_native_asset(DetectedKey* key, char* out_path, size_t out_path_size);
