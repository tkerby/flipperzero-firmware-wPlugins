
/** 
  @defgroup USB-CAN-MAIN
  @brief
  These are main application files. They are an adaptation of Xtreme firmware USB-UART bridge App in order to send data on CAN bus instead of UART.
  @{
  @file usb_can_bridge.hpp
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <api_lock.h>
#include <furi_hal.h>
#include <furi_hal_usb_cdc.h>
#include "usb_can_app_i.hpp"

UsbCanBridge* usb_can_enable(UsbCanApp* app, usbCanState state);
void usb_can_disable(UsbCanBridge* usb_can);
void usb_can_set_config(UsbCanBridge* usb_can, UsbCanConfig* cfg);
void usb_can_get_config(UsbCanBridge* usb_can, UsbCanConfig* cfg);
void usb_can_get_state(UsbCanBridge* usb_can, UsbCanConfig* st);
/** @}*/