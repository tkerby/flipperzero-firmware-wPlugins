#pragma once

#include <storage/storage.h>

// .fff stands for (f)lipper (f)ile (f)ormat
#define CONFIG_FILE_PATH APP_DATA_PATH("config.fff")

#define STATIONS_PATH          APP_DATA_PATH("stations")
#define STATIONS_PATH_OF(path) STATIONS_PATH "/" path

#define SAVED_STATIONS_PATH     STATIONS_PATH_OF("saved")
#define AUTOSAVED_STATIONS_PATH STATIONS_PATH_OF("autosaved")
