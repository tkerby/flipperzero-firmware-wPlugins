#include "../cfw_app.h"
#include "cfw_app_scene.h"

#include "../helpers/passport_settings.h"

#define CFW_PASSPORT_SELECT_BACKGROUND 0
#define CFW_PASSPORT_SELECT_IMAGE 1
#define CFW_PASSPORT_SELECT_NAME 2
#define CFW_PASSPORT_SELECT_MOOD_SET 3
#define CFW_PASSPORT_SELECT_LEVEL 4
#define CFW_PASSPORT_SELECT_XP_TEXT 5
#define CFW_PASSPORT_SELECT_XP_MODE 6
#define CFW_PASSPORT_SELECT_MULTIPAGE 7

const char* const xp_mode_count_text[XP_MODE_COUNT] =
    {"Bar", "%", "Inv. %", "Retro 3", "Retro 5", "Bar %", "None"};

const uint32_t xp_mode_value[XP_MODE_COUNT] = {
    XP_MODE_BAR,
    XP_MODE_PERCENT,
    XP_MODE_INVERTED_PERCENT,
    XP_MODE_RETRO_3,
    XP_MODE_RETRO_5,
    XP_MODE_BAR_PERCENT,
    XP_MODE_NONE};

const char* const passport_on_off_text[PASSPORT_ON_OFF_COUNT] = {
    "OFF",
    "ON",
};

const char* const background_text[PASSPORT_BG_COUNT] = {
    "None",
    "AD Police",
    "Circuit",
    "DB",
    "DedSec",
    "Flipper",
    "Furipass",
    "GITS",
    "Mario",
    "Medieval",
    "Memory Chip",
    "Mountains",
    "Multipass",
    "Scroll",
    "Slutpass"};

const uint32_t background_value[PASSPORT_BG_COUNT] = {
    BG_NONE,
    BG_ADPOLICE,
    BG_CIRCUIT,
    BG_DB,
    BG_DEDSEC,
    BG_STOCK,
    BG_FURI,
    BG_GITS,
    BG_MARIO,
    BG_MEDIEVAL,
    BG_MEMCHIP,
    BG_MOUNTAINS,
    BG_MULTI,
    BG_SCROLL,
    BG_SLUT};

const char* const image_text[PROFILE_IMG_COUNT] = {
    "None",
    "AD Police",
    "Akira Kaneda",
    "Akira Kei",
    "Akira Tetsuo",
    "Briareos",
    "Dali Mask",
    "DEDSEC",
    "Deer",
    "Dolphin (Happy)",
    "Dolphin (Moody)",
    "ED-209",
    "FSociety",
    "FSociety 2",
    "GITS Aoi",
    "GITS Aramaki",
    "GITS Batou",
    "GITS Hideo Kuze",
    "GITS Ishikawa",
    "GITS Kusanagi",
    "GITS Project 2501",
    "GITS Saito",
    "GITS Togusa",
    "Goku (Set)",
    "Goku (Kid)",
    "Goku (Adult)",
    "Goku (SSJ)",
    "Goku (SSJ3)",
    "Guy Fawkes",
    "Haunter",
    "Lain",
    "Leeroy Jenkins",
    "Mario",
    "Marvin",
    "Moreleeloo",
    "Neuromancer",
    "Pikachu (Sleepy)",
    "Pirate",
    "Pokemon Trainer",
    "Psyduck",
    "Rabbit",
    "SC Armaroid Lady",
    "SC Cobra",
    "SC Crystal Bowie",
    "SC Dominique Royal",
    "SC Sandra",
    "SC Tarbeige",
    "Shinkai",
    "Skull",
    "Slime",
    "Sonic",
    "Spider Jerusalem",
    "Tank Girl",
    "Totoro",
    "Waifu 1",
    "Waifu 2",
    "Waifu 3",
    "Wrench"};

const uint32_t image_value[PROFILE_IMG_COUNT] = {
    PIMG_NONE,         PIMG_ADPOLICE,   PIMG_AKKAN,    PIMG_AKKEI,     PIMG_AKTET,
    PIMG_BRIAREOS,     PIMG_DALI,       PIMG_DEDSEC,   PIMG_DEER,      PIMG_DOLPHIN,
    PIMG_DOLPHINMOODY, PIMG_ED209,      PIMG_FSOCIETY, PIMG_FSOCIETY2, PIMG_GITSAOI,
    PIMG_GITSARA,      PIMG_GITSBAT,    PIMG_GITSHID,  PIMG_GITSISH,   PIMG_GITSKUS,
    PIMG_GITSPRO,      PIMG_GITSSAI,    PIMG_GITSTOG,  PIMG_GOKUSET,   PIMG_GOKUKID,
    PIMG_GOKUADULT,    PIMG_GOKUSSJ,    PIMG_GOKUSSJ3, PIMG_GUYFAWKES, PIMG_HAUNTER,
    PIMG_LAIN,         PIMG_LEEROY,     PIMG_MARIO,    PIMG_MARVIN,    PIMG_MORELEELLOO,
    PIMG_NEUROMANCER,  PIMG_PIKASLEEPY, PIMG_PIRATE,   PIMG_PKMNTR,    PIMG_PSYDUCK,
    PIMG_RABBIT,       PIMG_SCARMLA,    PIMG_SCCOBRA,  PIMG_SCCRYBO,   PIMG_SCDOMRO,
    PIMG_SCSANDRA,     PIMG_SCTARBEIGE, PIMG_SHINKAI,  PIMG_SKULL,     PIMG_SLIME,
    PIMG_SONIC,        PIMG_SPIDER,     PIMG_TANKGIRL, PIMG_TOTORO,    PIMG_WAIFU1,
    PIMG_WAIFU2,       PIMG_WAIFU3,     PIMG_WRENCH};

const uint32_t name_value[PASSPORT_ON_OFF_COUNT] = {false, true};

const char* const mood_set_text[MOOD_SET_COUNT] = {"None", "Regular", "420"};

const uint32_t mood_set_value[MOOD_SET_COUNT] = {MOOD_SET_NONE, MOOD_SET_REGULAR, MOOD_SET_420};

const uint32_t level_value[PASSPORT_ON_OFF_COUNT] = {false, true};

const uint32_t xp_text_value[PASSPORT_ON_OFF_COUNT] = {false, true};

const uint32_t multipage_value[PASSPORT_ON_OFF_COUNT] = {false, true};

static void
    cfw_app_scene_interface_passport_var_list_enter_callback(void* context, uint32_t index) {
    CfwApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cfw_app_scene_interface_passport_background_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, background_text[index]);
    app->passport.background = index;
}

static void cfw_app_scene_interface_passport_image_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, image_text[index]);
    app->passport.image = image_value[index];
}

static void cfw_app_scene_interface_passport_name_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, passport_on_off_text[index]);
    app->passport.name = name_value[index];
}

static void cfw_app_scene_interface_passport_mood_set_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, mood_set_text[index]);
    app->passport.mood_set = index;
}

static void cfw_app_scene_interface_passport_level_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, passport_on_off_text[index]);
    app->passport.level = level_value[index];
}

static void cfw_app_scene_interface_passport_xp_text_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, passport_on_off_text[index]);
    app->passport.xp_text = xp_text_value[index];
}

static void cfw_app_scene_interface_passport_xp_mode_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, xp_mode_count_text[index]);
    app->passport.xp_mode = index;
}

static void cfw_app_scene_interface_passport_multipage_changed(VariableItem* item) {
    CfwApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, passport_on_off_text[index]);
    app->passport.multipage = multipage_value[index];
}

void cfw_app_scene_interface_passport_on_enter(void* context) {
    CfwApp* app = context;
    VariableItemList* variable_item_list = app->var_item_list;

    VariableItem* item;
    uint8_t value_index;

    item = variable_item_list_add(
        variable_item_list,
        "Background",
        PASSPORT_BG_COUNT,
        cfw_app_scene_interface_passport_background_changed,
        app);

    value_index =
        value_index_uint32(app->passport.background, background_value, PASSPORT_BG_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, background_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Image",
        PROFILE_IMG_COUNT,
        cfw_app_scene_interface_passport_image_changed,
        app);

    value_index = value_index_uint32(app->passport.image, image_value, PROFILE_IMG_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, image_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Name",
        PASSPORT_ON_OFF_COUNT,
        cfw_app_scene_interface_passport_name_changed,
        app);

    value_index = value_index_uint32(app->passport.name, name_value, PASSPORT_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, passport_on_off_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Mood Text Set",
        MOOD_SET_COUNT,
        cfw_app_scene_interface_passport_mood_set_changed,
        app);

    value_index = value_index_uint32(app->passport.mood_set, mood_set_value, MOOD_SET_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, mood_set_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Level",
        PASSPORT_ON_OFF_COUNT,
        cfw_app_scene_interface_passport_level_changed,
        app);

    value_index = value_index_uint32(app->passport.level, level_value, PASSPORT_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, passport_on_off_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "XP Text",
        PASSPORT_ON_OFF_COUNT,
        cfw_app_scene_interface_passport_xp_text_changed,
        app);

    value_index = value_index_uint32(app->passport.xp_text, xp_text_value, PASSPORT_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, passport_on_off_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "XP Mode",
        XP_MODE_COUNT,
        cfw_app_scene_interface_passport_xp_mode_changed,
        app);

    value_index = value_index_uint32(app->passport.xp_mode, xp_mode_value, XP_MODE_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, xp_mode_count_text[value_index]);

    item = variable_item_list_add(
        variable_item_list,
        "Multiple Pages",
        PASSPORT_ON_OFF_COUNT,
        cfw_app_scene_interface_passport_multipage_changed,
        app);

    value_index =
        value_index_uint32(app->passport.multipage, multipage_value, PASSPORT_ON_OFF_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, passport_on_off_text[value_index]);

    variable_item_list_set_enter_callback(
        variable_item_list, cfw_app_scene_interface_passport_var_list_enter_callback, app);

    variable_item_list_set_selected_item(
        variable_item_list,
        scene_manager_get_scene_state(app->scene_manager, CfwAppSceneInterfacePassport));
    view_dispatcher_switch_to_view(app->view_dispatcher, CfwAppViewVarItemList);
}

bool cfw_app_scene_interface_passport_on_event(void* context, SceneManagerEvent sme) {
    UNUSED(context);
    bool consumed = false;

    if(sme.type == SceneManagerEventTypeCustom) {
        switch(sme.event) {
        case CFW_PASSPORT_SELECT_BACKGROUND:
            consumed = true;
            break;
        case CFW_PASSPORT_SELECT_IMAGE:
            consumed = true;
            break;
        case CFW_PASSPORT_SELECT_NAME:
            consumed = true;
            break;
        case CFW_PASSPORT_SELECT_MOOD_SET:
            consumed = true;
            break;
        case CFW_PASSPORT_SELECT_LEVEL:
            consumed = true;
            break;
        case CFW_PASSPORT_SELECT_XP_TEXT:
            consumed = true;
            break;
        case CFW_PASSPORT_SELECT_XP_MODE:
            consumed = true;
            break;
        case CFW_PASSPORT_SELECT_MULTIPAGE:
            consumed = true;
            break;
        }
    }
    return consumed;
}

void cfw_app_scene_interface_passport_on_exit(void* context) {
    CfwApp* app = context;
    variable_item_list_reset(app->var_item_list);
    passport_settings_save(&app->passport);
}