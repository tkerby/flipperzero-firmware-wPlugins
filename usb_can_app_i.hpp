#pragma once
#ifdef __cplusplus
#include "scenes/usbCan_scene.hpp"
#include "usb_can_custom_event.hpp"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include "usb_can_custom_event.hpp"
#include "Longan_CANFD/src/mcp2518fd_can.hpp"
#include "api_lock.h"
#include "furi_hal_usb_cdc.h"
#define USB_CDC_PKT_LEN CDC_DATA_SZ

typedef struct {
    uint8_t vcp_ch;
} UsbCanConfig;
typedef void (*usbCanViewCallBack_t)(UsbCanCustomEvent event, void* context);

typedef struct {
    View* view;
    usbCanViewCallBack_t callback;
    void* context;
} usbCanView;

typedef struct {
    uint32_t rx_cnt;
    uint32_t tx_cnt;
} UsbCanStatus;

typedef enum { UsbCanLoopBackTestState, UsbCanPingTestState, UsbCanNominalState } usbCanState;

typedef struct {
    UsbCanConfig cfg;
    UsbCanConfig cfg_new;

    usbCanState state;

    mcp2518fd* can;

    FuriThread* thread;
    FuriThread* tx_thread;

    FuriStreamBuffer* rx_stream;

    FuriMutex* usb_mutex;

    FuriMutex* tx_mutex;

    UsbCanStatus st;

    FuriApiLock cfg_lock;
    FuriTimer* timer;

    uint8_t rx_buf[USB_CDC_PKT_LEN];
} UsbCanBridge;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Widget* usb_busy_popup;
    DialogEx* quit_dialog;
    VariableItemList* menu;
    VariableItem* var_item_flow;
    usbCanView* usb_can;
} UsbCanAppViews;

typedef struct {
    NotificationApp* notifications;
    SceneManager* scene_manager;
    UsbCanAppViews views;
    UsbCanBridge* usb_can_bridge;
} UsbCanApp;

typedef enum {
    UsbCanAppViewMenu,
    UsbCanAppViewCfg,
    UsbCanAppViewUsbCan,
    UsbCanAppViewUsbBusy,
    UsbCanAppViewExitConfirm
} UsbCanAppView;

#define EXTERNC extern "C"
#else
#define EXTERNC extern
#endif

EXTERNC int32_t __attribute__((used)) usb_can_app(void* p);