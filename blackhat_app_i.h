#pragma once

#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>

#include "blackhat_app.h"
#include "blackhat_custom_event.h"
#include "blackhat_uart.h"
#include "scenes/blackhat_scene.h"

#define NUM_MENU_ITEMS (15)

#define BLACKHAT_TEXT_BOX_STORE_SIZE (4096)
#define UART_CH FuriHalSerialIdUsart

#define SHELL_CMD "whoami"
#define WIFI_CON_CMD "bh wifi connect"
#define SET_INET_SSID_CMD "bh set SSID"
#define SET_INET_PWD_CMD "bh set PASS"
#define SET_AP_SSID_CMD "bh set AP_SSID"
#define LIST_AP_CMD "bh wifi list"
#define DEV_CMD "bh wifi dev"
#define START_AP_CMD "bh wifi ap"
#define GET_IP_CMD "bh wifi ip"
#define START_SSH_CMD "bh ssh"
#define START_EVIL_TWIN "bh evil_twin"
#define START_EVIL_PORT "bh evil_portal"
#define START_RAT "bh evil_portal"
#define GET_CMD "bh get"
#define HELP_CMD "cat /mnt/help.txt"

typedef enum { NO_ARGS = 0, INPUT_ARGS, TOGGLE_ARGS } InputArgs;

typedef enum {
    FOCUS_CONSOLE_END = 0,
    FOCUS_CONSOLE_START,
    FOCUS_CONSOLE_TOGGLE
} FocusConsole;

#define MAX_OPTIONS (9)
typedef struct {
    const char* item_string;
    char* options_menu[MAX_OPTIONS];
    int num_options_menu;
    char* selected_option;
    char* actual_command;
    FocusConsole focus_console;
} BlackhatItem;

#define ENTER_NAME_LENGTH 25

struct BlackhatApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    FuriString* text_box_store;
    size_t text_box_store_strlen;
    TextBox* text_box;

    VariableItemList* var_item_list;
    BlackhatUart* uart;
    TextInput* text_input;
    DialogsApp* dialogs;

    int selected_menu_index;
    int selected_option_index[NUM_MENU_ITEMS];
    char* selected_tx_string;
    const char* selected_option_item_text;
    bool focus_console_start;
    char text_store[128];
    char text_input_ch[ENTER_NAME_LENGTH];
};

typedef enum {
    BlackhatAppViewVarItemList,
    BlackhatAppViewConsoleOutput,
    BlackhatAppViewStartPortal,
    BlackhatAppViewTextInput,
} BlackhatAppView;
