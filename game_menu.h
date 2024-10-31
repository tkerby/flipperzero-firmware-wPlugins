#pragma once
#include <engine/engine.h>
#include <furi_hal.h>

#include <notification/notification_messages.h>

#include <gui/view_holder.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <api_lock.h>

extern bool game_menu_tutorial_selected;
extern FuriApiLock game_menu_exit_lock;

void game_menu_button_callback(void* game_manager, uint32_t index);
void game_menu_open(GameManager* gameManager);