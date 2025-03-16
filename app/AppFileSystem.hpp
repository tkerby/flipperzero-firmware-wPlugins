#pragma once

#include <storage/storage.h>

// .fff stands for (f)lipper (f)ile (f)ormat
#define CONFIG_FILE_PATH APP_DATA_PATH("config.fff")

#define SAVED_STATIONS_PATH          APP_DATA_PATH("stations")
#define SAVED_STATIONS_PATH_OF(path) SAVED_STATIONS_PATH "/" path

#define KNOWN_STATIONS_DIR_PATH     SAVED_STATIONS_PATH_OF("known")
#define AUTOSAVED_STATIONS_DIR_PATH SAVED_STATIONS_PATH_OF("autosaved")
