#pragma once
#include <flip_world.h>

void callback_submenu_choices(void *context, uint32_t index);
bool message_input_callback(InputEvent *event, void *context);
void message_draw_callback(Canvas *canvas, void *model);
void wifi_settings_select(void *context, uint32_t index);
void updated_wifi_ssid(void *context);
void updated_wifi_pass(void *context);
void updated_username(void *context);
void updated_password(void *context);
void fps_change(VariableItem *item);
void game_settings_select(void *context, uint32_t index);
void user_settings_select(void *context, uint32_t index);
void screen_on_change(VariableItem *item);
void sound_on_change(VariableItem *item);
void vibration_on_change(VariableItem *item);
void player_on_change(VariableItem *item);
void vgm_x_change(VariableItem *item);
void vgm_y_change(VariableItem *item);