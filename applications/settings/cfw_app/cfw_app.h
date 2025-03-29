#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <dialogs/dialogs.h>
#include <gui/modules/dialog_ex.h>
#include <applications.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include <lib/toolbox/path.h>
#include <lib/toolbox/value_index.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include "scenes/cfw_app_scene.h"
#include "dolphin/helpers/dolphin_state.h"
#include "dolphin/dolphin.h"
#include "dolphin/dolphin_i.h"
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/subghz_setting.h>
#include <flipper_application/flipper_application.h>
#include <loader/loader.h>
#include <loader/loader_mainmenu.h>
#include <notification/notification_app.h>
#include <power/power_service/power.h>
#include <expansion/expansion.h>
#include <rgb_backlight.h>
#include <m-array.h>
#include <cfw/cfw.h>
#include <cfw/namespoof.h>
#include "cfw_icons.h"
#include <applications.h>
#include <desktop/desktop_settings.h>
#include "helpers/passport_settings.h"

#define CFW_SUBGHZ_FREQ_BUFFER_SIZE 6

ARRAY_DEF(CharList, char*)

//For the Dolphin Manifest Switcher Menu.
typedef struct ManifestInfo ManifestInfo;
ARRAY_DEF(ManifestFilesArray, ManifestInfo*, M_POD_OPLIST)

static const struct {
    char* name;
    RgbColor color;
} lcd_colors[] = {
    {"Orange", {255, 60, 0}},  {"Red", {255, 0, 0}},     {"Maroon", {128, 0, 0}},
    {"Yellow", {255, 150, 0}}, {"Olive", {128, 128, 0}}, {"Lime", {0, 255, 0}},
    {"Green", {0, 128, 0}},    {"Aqua", {0, 255, 127}},  {"Cyan", {0, 210, 210}},
    {"Azure", {0, 127, 255}},  {"Teal", {0, 128, 128}},  {"Blue", {0, 0, 255}},
    {"Navy", {0, 0, 128}},     {"Purple", {96, 0, 255}}, {"Fuchsia", {255, 0, 255}},
    {"Pink", {255, 0, 127}},   {"Brown", {165, 42, 42}}, {"White", {255, 192, 203}},
    {"Off", {0, 0, 0}},
};

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    Expansion* expansion;
    NotificationApp* notification;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    VariableItemList* var_item_list;
    Submenu* submenu;
    TextInput* text_input;
    ByteInput* byte_input;
    Popup* popup;
    DialogEx* dialog_ex;

    DesktopSettings desktop;
    PassportSettings passport;

    uint8_t main_style_index;
    uint8_t game_style_index;

    CharList_t mainmenu_app_names;
    CharList_t mainmenu_app_paths;
    uint8_t mainmenu_app_index;

    CharList_t gamemenu_app_names;
    CharList_t gamemenu_app_paths;
    uint8_t gamemenu_app_index;

    uint8_t start_point_index;
    uint8_t game_start_point_index;

    bool subghz_use_defaults;
    FrequencyList_t subghz_static_freqs;
    uint8_t subghz_static_index;
    FrequencyList_t subghz_hopper_freqs;
    uint8_t subghz_hopper_index;
    char subghz_freq_buffer[CFW_SUBGHZ_FREQ_BUFFER_SIZE];
    bool subghz_extend;
    bool subghz_bypass;
    RgbColor lcd_color;
    Rgb565Color vgm_color;
    char device_name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    FuriString* version_tag;

    bool save_mainmenu_apps;
    bool save_gamemenu_apps;
    bool save_subghz_frequencies;
    bool save_subghz;
    bool save_name;
    bool save_backlight;
    bool save_settings;
    bool require_reboot;
} CfwApp;

typedef enum {
    CfwAppViewVarItemList,
    CfwAppViewSubmenu,
    CfwAppViewTextInput,
    CfwAppViewByteInput,
    CfwAppViewPopup,
    CfwAppViewDialogEx,
} CfwAppView;

bool cfw_app_apply(CfwApp* app);
