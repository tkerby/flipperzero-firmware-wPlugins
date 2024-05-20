#include "../cfw_app.h"
#include "cfw_app_scene.h"

#define CFW_DESKTOP_SELECT_ICON_STYLE 0
#define CFW_DESKTOP_SELECT_BATTERY_DISPLAY 1
#define CFW_DESKTOP_SELECT_BT_ICON 3
#define CFW_DESKTOP_SELECT_CLOCK_DISPLAY 4
#define CFW_DESKTOP_SELECT_DUMBMODE_ICON 5
#define CFW_DESKTOP_SELECT_LOCK_ICON 6
#define CFW_DESKTOP_SELECT_RPC_ICON 7
#define CFW_DESKTOP_SELECT_SDCARD_ICON 8
#define CFW_DESKTOP_SELECT_STEALTH_ICON 9
#define CFW_DESKTOP_SELECT_TOP_BAR 10

#define FILE_NAME_LEN_MAX 256

#define BATTERY_VIEW_COUNT 7
const char* const battery_view_count_text[BATTERY_VIEW_COUNT] =
    {"Bar", "%", "Inv. %", "Retro 3", "Retro 5", "Bar %", "None"};

const uint32_t displayBatteryPercentage_value[BATTERY_VIEW_COUNT] = {
    DISPLAY_BATTERY_BAR,
    DISPLAY_BATTERY_PERCENT,
    DISPLAY_BATTERY_INVERTED_PERCENT,
    DISPLAY_BATTERY_RETRO_3,
    DISPLAY_BATTERY_RETRO_5,
    DISPLAY_BATTERY_BAR_PERCENT,
    DISPLAY_BATTERY_NONE};

uint8_t origBattDisp_value = 0;

//Dolphin Manifest Switcher menu.
struct ManifestInfo {
    char* FileName;
    char* MenuName;
};
uint8_t ManifestFileCount; //Anymore than 255 manifests and kaboom!
ManifestFilesArray_t ManifestFiles;

#define CFW_DESKTOP_ICON_STYLE_COUNT 2

const char* const icon_style_count_text[CFW_DESKTOP_ICON_STYLE_COUNT] = {"Stock", "Slim"};

const uint32_t icon_style_value[CFW_DESKTOP_ICON_STYLE_COUNT] = {ICON_STYLE_STOCK, ICON_STYLE_SLIM};
uint8_t origIconStyle_value = 0;

#define CFW_DESKTOP_ON_OFF_COUNT 2

const char* const cfw_desktop_on_off_text[CFW_DESKTOP_ON_OFF_COUNT] = {
    "OFF",
    "ON",
};

const uint32_t clock_enable_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

const uint32_t lockicon_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

const uint32_t bticon_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

const uint32_t rpc_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

const uint32_t sdcard_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

const uint32_t stealth_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

const uint32_t topbar_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

const uint32_t dumbmode_icon_value[CFW_DESKTOP_ON_OFF_COUNT] = {false, true};

static void cfw_app_scene_interface_desktop_var_item_list_callback(void* context, uint32_t index) {
    CfwApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cfw_app_scene_interface_desktop_anim_style_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    ManifestInfo* CurrentManifest = *ManifestFilesArray_get(ManifestFiles, index);
    variable_item_set_current_value_text(item, CurrentManifest->MenuName);
    cfw_settings.manifest_name = CurrentManifest->FileName;
    app->save_settings = true;
}

static void cfw_app_scene_interface_desktop_clock_enable_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.display_clock = index;
}

static void cfw_app_scene_interface_desktop_battery_view_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, battery_view_count_text[index]);
    app->desktop.displayBatteryPercentage = index;
}

static void cfw_app_scene_interface_desktop_icon_style_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, icon_style_count_text[index]);
    app->desktop.icon_style = icon_style_value[index];
}

static void cfw_app_scene_interface_desktop_lockicon_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.lock_icon = lockicon_value[index];
}

static void cfw_app_scene_interface_desktop_bticon_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.bt_icon = bticon_value[index];
}

static void cfw_app_scene_interface_desktop_rpc_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.rpc_icon = rpc_value[index];
}

static void cfw_app_scene_interface_desktop_sdcard_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.sdcard = sdcard_value[index];
}

static void cfw_app_scene_interface_desktop_stealth_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.stealth_icon = stealth_value[index];
}

static void cfw_app_scene_interface_desktop_topbar_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.top_bar = topbar_value[index];
}

static void cfw_app_scene_interface_desktop_dumbmode_icon_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[index]);
    app->desktop.dumbmode_icon = dumbmode_icon_value[index];
}

void cfw_app_scene_interface_desktop_on_enter(void* context) {
    CfwApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;

    VariableItem* item;
    uint8_t value_index;

    origIconStyle_value = app->desktop.icon_style;
    origBattDisp_value = app->desktop.displayBatteryPercentage;

    //Initialization.
    ManifestFileCount = 0;
    ManifestFilesArray_init(ManifestFiles);

    //Open up Storage.
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* folder = storage_file_alloc(storage);
    FileInfo info;
    char* name = malloc(FILE_NAME_LEN_MAX);
    uint8_t current_manifest = 0;

    //Lets walk the dolphin folder and get the filenamess.
    if(storage_dir_open(folder, EXT_PATH("dolphin"))) {
        while(storage_dir_read(folder, &info, name, FILE_NAME_LEN_MAX)) {
            if(!(info.flags & FSF_DIRECTORY)) {
                //Create the struct to add to the array of Manifests.
                ManifestInfo* NewManifestInfo = malloc(sizeof(ManifestInfo));
                NewManifestInfo->FileName = strdup(name);

                //Are we on a manifest>
                if(strncasecmp(NewManifestInfo->FileName, "manifest", 8) == 0) {
                    if(strlen(NewManifestInfo->FileName) == 12) {
                        //Default Manifest on every Flipper (You'd Hope!)
                        NewManifestInfo->MenuName = "Default";

                        //Add to the list of names.
                        ManifestFilesArray_push_back(ManifestFiles, NewManifestInfo);
                        ManifestFileCount++;
                    } else {
                        //Allocate memory for the menuname
                        NewManifestInfo->MenuName = malloc(strlen(NewManifestInfo->FileName) - 12);
                        snprintf(
                            NewManifestInfo->MenuName,
                            strlen(NewManifestInfo->FileName) - 12,
                            "%s",
                            NewManifestInfo->FileName + 9);

                        //Sanity Check.
                        if(strcmp(NewManifestInfo->MenuName, "") != 0) {
                            //Add to the list of Manifests.
                            ManifestFilesArray_push_back(ManifestFiles, NewManifestInfo);

                            //Select in menu if its our manifest.
                            if(strcmp(NewManifestInfo->FileName, cfw_settings.manifest_name) == 0)
                                current_manifest = ManifestFileCount;

                            //Count the added Files.
                            ManifestFileCount++;
                        }
                    }
                }
            }
        }
    }

    //Close up and free.
    free(name);
    storage_file_free(folder);

    //Add items to list.
    item = variable_item_list_add(
        var_item_list,
        "Animations",
        ManifestFileCount,
        cfw_app_scene_interface_desktop_anim_style_changed,
        app);
    variable_item_set_current_value_index(item, current_manifest);
    ManifestInfo* CurrentManifest = *ManifestFilesArray_get(ManifestFiles, current_manifest);
    variable_item_set_current_value_text(item, CurrentManifest->MenuName);

    item = variable_item_list_add(
        var_item_list,
        "Icon Style",
        CFW_DESKTOP_ICON_STYLE_COUNT,
        cfw_app_scene_interface_desktop_icon_style_changed,
        app);

    value_index = value_index_uint32(
        app->desktop.icon_style, icon_style_value, CFW_DESKTOP_ICON_STYLE_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, icon_style_count_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Battery View",
        BATTERY_VIEW_COUNT,
        cfw_app_scene_interface_desktop_battery_view_changed,
        app);

    value_index = value_index_uint32(
        app->desktop.displayBatteryPercentage, displayBatteryPercentage_value, BATTERY_VIEW_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, battery_view_count_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "BT Icon",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_bticon_changed,
        app);

    value_index = value_index_uint32(app->desktop.bt_icon, bticon_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Clock",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_clock_enable_changed, //
        app);

    value_index = value_index_uint32(
        app->desktop.display_clock, clock_enable_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Games Only Icon",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_dumbmode_icon_changed,
        app);

    value_index = value_index_uint32(
        app->desktop.dumbmode_icon, dumbmode_icon_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Lock Icon",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_lockicon_changed,
        app);

    value_index =
        value_index_uint32(app->desktop.lock_icon, lockicon_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "RPC Icon",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_rpc_changed,
        app);

    value_index = value_index_uint32(app->desktop.rpc_icon, rpc_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "SD Card Icon",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_sdcard_changed,
        app);

    value_index = value_index_uint32(app->desktop.sdcard, sdcard_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Stealth Icon",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_stealth_changed,
        app);

    value_index =
        value_index_uint32(app->desktop.stealth_icon, stealth_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Top Bar",
        CFW_DESKTOP_ON_OFF_COUNT,
        cfw_app_scene_interface_desktop_topbar_changed,
        app);

    value_index = value_index_uint32(app->desktop.top_bar, topbar_value, CFW_DESKTOP_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, cfw_desktop_on_off_text[value_index]);

    variable_item_list_set_enter_callback(
        var_item_list, cfw_app_scene_interface_desktop_var_item_list_callback, app);

    variable_item_list_set_selected_item(
        var_item_list,
        scene_manager_get_scene_state(app->scene_manager, CfwAppSceneInterfaceDesktop));

    view_dispatcher_switch_to_view(app->view_dispatcher, CfwAppViewVarItemList);
}

bool cfw_app_scene_interface_desktop_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case CFW_DESKTOP_SELECT_ICON_STYLE:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_BATTERY_DISPLAY:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_LOCK_ICON:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_BT_ICON:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_RPC_ICON:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_SDCARD_ICON:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_STEALTH_ICON:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_TOP_BAR:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_DUMBMODE_ICON:
            consumed = true;
            break;
        case CFW_DESKTOP_SELECT_CLOCK_DISPLAY:
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_interface_desktop_on_exit(void* context) {
    CfwApp* app = context;
    variable_item_list_reset(app->var_item_list);
    DESKTOP_SETTINGS_SAVE(&app->desktop);

    //Free the Manifest List.
    ManifestFilesArray_it_t ManifestFiles_it;
    for(ManifestFilesArray_it(ManifestFiles_it, ManifestFiles);
        !ManifestFilesArray_end_p(ManifestFiles_it);
        ManifestFilesArray_next(ManifestFiles_it)) {
        free(*ManifestFilesArray_cref(ManifestFiles_it));
    }

    if((app->desktop.icon_style != origIconStyle_value) ||
       (app->desktop.displayBatteryPercentage != origBattDisp_value)) {
        // Trigger UI update in case we changed battery layout
        Power* power = furi_record_open(RECORD_POWER);
        power_update_viewport(power);
        furi_record_close(RECORD_POWER);
    }
}
