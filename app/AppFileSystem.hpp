#pragma once

#include <storage/storage.h>

#define SAVED_STATIONS_PATH(path) APP_DATA_PATH("stations") "/" path

#define CONFIG_FILE_PATH      APP_DATA_PATH("config.ff")
#define KNOWN_ST_DIR_PATH     SAVED_STATIONS_PATH("known")
#define AUTOSAVED_ST_DIR_PATH SAVED_STATIONS_PATH("autosaved")
