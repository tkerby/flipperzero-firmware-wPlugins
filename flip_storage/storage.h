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

bool save_world_furi(FuriString *name, FuriString *world_data);

bool load_world(
    const char *name,
    char *json_data,
    size_t json_data_size);

FuriString *load_furi_world(
    const char *name);

bool world_exists(
    const char *name);

bool save_world_names(const FuriString *json);
FuriString *flip_social_info(char *key);
bool is_logged_in_to_flip_social();