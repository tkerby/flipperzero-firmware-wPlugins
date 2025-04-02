/* 
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero 
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "sio2flip_icons.h"

#include "scenes.h"

#include "app/app.h"

static void popup_callback(void* context) {
    App* app = (App*)context;

    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneMainMenu);
}

void scene_splash_screen_on_enter(void* context) {
    App* app = (App*)context;

    popup_set_icon(app->popup, 0, 0, &I_sio2flip_splash);

    popup_set_timeout(app->popup, 1500);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, popup_callback);

    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewPopup);
}

bool scene_splash_screen_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    UNUSED(context);
    UNUSED(event);

    return consumed;
}

void scene_splash_screen_on_exit(void* context) {
    App* app = (App*)context;

    popup_reset(app->popup);
}
