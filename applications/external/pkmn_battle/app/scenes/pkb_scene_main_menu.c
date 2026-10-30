#include <pkb/music_test.h>
#include <pkb/picture.h>

#include "../pkb_app.h"
#include "pkb_scene.h"

typedef enum {
    MainMenuStartBattle,
    MainMenuOpponent,
    MainMenuViewTeam,
    MainMenuMusic,
    MainMenuPicture,
    MainMenuPinout,
    MainMenuReset,
    MainMenuAbout,
} MainMenuIndex;

#define RESET_EVENT_NO  0x3100u
#define RESET_EVENT_YES 0x3101u

static bool confirming;

static void main_menu_callback(void* context, uint32_t index) {
    PkbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void reset_button(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    PkbApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, result == GuiButtonTypeRight ? RESET_EVENT_YES : RESET_EVENT_NO);
}

static void show_reset_confirm(PkbApp* app) {
    confirming = true;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Reset to default?");
    widget_add_string_multiline_element(
        app->widget, 64, 16, AlignCenter, AlignTop, FontSecondary, "Opponent, music\nand picture");
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Cancel", reset_button, app);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Reset", reset_button, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewWidget);
}

void pkb_scene_main_menu_on_enter(void* context) {
    PkbApp* app = context;
    Submenu* submenu = app->submenu;
    confirming = false;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Pokemon Battle");
    submenu_add_item(submenu, "Start Battle", MainMenuStartBattle, main_menu_callback, app);
    char label[40];
    int n = snprintf(label, sizeof(label), "Opponent: ");
    pkb_app_opponent_label(app, label + n, sizeof(label) - n);
    submenu_add_item(submenu, label, MainMenuOpponent, main_menu_callback, app);
    submenu_add_item(submenu, "View Team", MainMenuViewTeam, main_menu_callback, app);
    snprintf(label, sizeof(label), "Music: %s", pkb_music_setting_label(app->settings.music_test));
    submenu_add_item(submenu, label, MainMenuMusic, main_menu_callback, app);
    snprintf(
        label, sizeof(label), "Picture: %s", pkb_picture_setting_label(app->settings.picture));
    submenu_add_item(submenu, label, MainMenuPicture, main_menu_callback, app);
    submenu_add_item(submenu, "Pinout", MainMenuPinout, main_menu_callback, app);
    submenu_add_item(submenu, "Reset to Default", MainMenuReset, main_menu_callback, app);
    submenu_add_item(submenu, "About", MainMenuAbout, main_menu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, PkbSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewSubmenu);
}

bool pkb_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;

    if(confirming && event.type == SceneManagerEventTypeBack) {
        confirming = false;
        widget_reset(app->widget);
        view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewSubmenu);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == RESET_EVENT_NO) {
        confirming = false;
        widget_reset(app->widget);
        view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewSubmenu);
        return true;
    }
    if(event.event == RESET_EVENT_YES) {
        confirming = false;
        widget_reset(app->widget);
        pkb_settings_reset(app);
        pkb_scene_main_menu_on_enter(app);
        return true;
    }

    scene_manager_set_scene_state(app->scene_manager, PkbSceneMainMenu, event.event);
    switch(event.event) {
    case MainMenuStartBattle:
        scene_manager_next_scene(app->scene_manager, PkbSceneBattle);
        return true;
    case MainMenuOpponent:
        scene_manager_next_scene(app->scene_manager, PkbSceneOpponent);
        return true;
    case MainMenuViewTeam:
        scene_manager_next_scene(app->scene_manager, PkbSceneViewTeam);
        return true;
    case MainMenuMusic:
        scene_manager_next_scene(app->scene_manager, PkbSceneMusic);
        return true;
    case MainMenuPicture:
        scene_manager_next_scene(app->scene_manager, PkbScenePicture);
        return true;
    case MainMenuPinout:
        scene_manager_next_scene(app->scene_manager, PkbScenePins);
        return true;
    case MainMenuReset:
        show_reset_confirm(app);
        return true;
    case MainMenuAbout:
        scene_manager_next_scene(app->scene_manager, PkbSceneAbout);
        return true;
    default:
        return false;
    }
}

void pkb_scene_main_menu_on_exit(void* context) {
    PkbApp* app = context;
    if(confirming) {
        confirming = false;
        widget_reset(app->widget);
    }
    submenu_reset(app->submenu);
}
