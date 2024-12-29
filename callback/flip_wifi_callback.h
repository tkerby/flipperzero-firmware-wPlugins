#pragma once
#include <flip_wifi.h>
#include <flip_storage/flip_wifi_storage.h>
#include <flip_wifi_icons.h>

void flip_wifi_free_all(void *context);
uint32_t callback_exit_app(void *context);
void callback_submenu_choices(void *context, uint32_t index);