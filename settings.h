#ifndef SETTINGS_H
#define SETTINGS_H

#include "storage/storage.h"
#include <stddef.h>

typedef struct Settings {
    double gravity_force;
    size_t num_planets;
    double launch_interval;
    double grid_speed;
    size_t grid_spacing;
    size_t trail_duration;
    size_t asteroid_speed;
    size_t debri_count;
} Settings;

static Settings DEFAULT_SETTINGS = {
    .gravity_force = 2000.0,
    .num_planets = 3, .launch_interval = 0.6, .grid_speed = 16,
    .grid_spacing = 16,
    .trail_duration = 1,
    .asteroid_speed = 1,
    .debri_count = 10,
};

Settings settings;

static void settings_load() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, "/ext/fzspground.cfg", FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[128] = {0};
        storage_file_read(file, buf, sizeof(buf) - 1);
        sscanf(buf,
               "%lf %zu %lf %lf %zu %zu %zu",
               &settings.gravity_force,
               &settings.num_planets,
               &settings.launch_interval,
               &settings.grid_speed,
               &settings.grid_spacing,
               &settings.trail_duration,
               &settings.debri_count);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void settings_clear() {
    settings = DEFAULT_SETTINGS;
}

void settings_init() {
    settings_clear();
    settings_load();
}


void settings_save() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, "/ext/fzspground.cfg", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "%.1f %zu %.1f %.1f %zu %zu %zu",
                 settings.gravity_force,
                 settings.num_planets,
                 settings.launch_interval,
                 settings.grid_speed,
                 settings.grid_spacing,
                 settings.trail_duration,
                 settings.debri_count);
        storage_file_write(file, buf, n);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

#endif
