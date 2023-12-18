
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <api_lock.h>
#include <furi_hal.h>
#include <furi_hal_usb_cdc.h>
#include "./Longan_CANFD/src/mcp2518fd_can.hpp"
#include "usb_can_app_i.hpp"
UsbCanBridge* usb_can_enable(UsbCanApp* app, usbCanState state);

void usb_can_disable(UsbCanBridge* usb_can);

void usb_can_set_config(UsbCanBridge* usb_can, UsbCanConfig* cfg);

void usb_can_get_config(UsbCanBridge* usb_can, UsbCanConfig* cfg);

void usb_can_get_state(UsbCanBridge* usb_can, UsbCanConfig* st);
