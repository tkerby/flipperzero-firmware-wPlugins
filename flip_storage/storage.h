#pragma once

#include <furi.h>
#include <storage/storage.h>
#include <flip_world.h>

#define SETTINGS_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/settings.bin"

void save_settings(
    const char *ssid,
    const char *password);

bool load_settings(
    char *ssid,
    size_t ssid_size,
    char *password,
    size_t password_size);

bool save_char(
    const char *path_name, const char *value);

bool load_char(
    const char *path_name,
    char *value,
    size_t value_size);

bool save_world(
    const char *name,
    const char *world_data);

bool load_world(
    const char *name,
    char *json_data,
    size_t json_data_size);