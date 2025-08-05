/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2024 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401DIGILAB_SCENE_CONFIG_H
#define _401DIGILAB_SCENE_CONFIG_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <lib/toolbox/value_index.h>
#include <locale/locale.h>

#include "app_params.h" // Application specific configuration

#include "401_config.h"

typedef struct {
    uint8_t value;
} AppConfigModel;

typedef struct AppConfig {
    View* view;
    AppConfigModel* model;
} AppConfig;

#include "401DigiLab_main.h"

AppConfig* app_config_alloc();
void app_config_free(AppConfig* appConfig);
View* app_config_get_view(AppConfig* appConfig);
void app_config_render_callback(Canvas* canvas, void* ctx);
bool app_config_input_callback(InputEvent* input_event, void* ctx);
void app_scene_config_on_enter(void* ctx);
bool app_scene_config_on_event(void* ctx, SceneManagerEvent event);
void app_scene_config_on_exit(void* ctx);
#endif
