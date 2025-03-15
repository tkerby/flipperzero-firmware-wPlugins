#pragma once

#include <storage/storage.h>

#define SAVED_STATIONS_PATH(path) APP_DATA_PATH("stations") "/" path

// .fff stands for (f)lipper (f)ile (f)ormat
#define CONFIG_FILE_PATH      APP_DATA_PATH("config.fff")
#define KNOWN_ST_DIR_PATH     SAVED_STATIONS_PATH("known")
#define AUTOSAVED_ST_DIR_PATH SAVED_STATIONS_PATH("autosaved")
