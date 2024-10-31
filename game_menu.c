#include "game_menu.h"

bool game_menu_tutorial_selected = false;
FuriApiLock game_menu_exit_lock = NULL;
void game_menu_button_callback(void* game_manager, uint32_t index) {
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    if(index == 0) {
        game_menu_tutorial_selected = false;
        notification_message(notifications, &sequence_success);
        api_lock_unlock(game_menu_exit_lock);
    } else if(index == 1) {
        game_menu_tutorial_selected = true;
        notification_message(notifications, &sequence_success);
        api_lock_unlock(game_menu_exit_lock);
    } else if(index == 2) {
        game_menu_tutorial_selected = false;
        //Settings menu
        notification_message(notifications, &sequence_single_vibro);
    } else if(index == 3) {
        //Quit
        game_menu_tutorial_selected = false;
        notification_message(notifications, &sequence_single_vibro);
        api_lock_unlock(game_menu_exit_lock);
        game_manager_game_stop((GameManager*)game_manager);
    }
}

void game_menu_open(GameManager* game_manager) {
    //Setup widget UI
    Gui* gui = furi_record_open(RECORD_GUI);
    /*
     Widget* widget = widget_alloc();
    widget_add_string_element(widget, 0, 30, AlignCenter, AlignCenter, FontPrimary, "test center");

    ViewHolder* view_holder = view_holder_alloc();

    view_holder_attach_to_gui(view_holder, gui);
    view_holder_set_view(view_holder, widget_get_view(widget));*/

    Submenu* submenu = submenu_alloc();
    submenu_add_item(submenu, "PLAY GAME", 0, game_menu_button_callback, game_manager);
    submenu_add_item(submenu, "TUTORIAL", 1, game_menu_button_callback, game_manager);
    submenu_add_item(submenu, "SETTINGS", 2, game_menu_button_callback, game_manager);
    submenu_add_item(submenu, "QUIT", 3, game_menu_button_callback, game_manager);

    ViewHolder* view_holder = view_holder_alloc();

    view_holder_attach_to_gui(view_holder, gui);
    view_holder_set_view(view_holder, submenu_get_view(submenu));

    api_lock_wait_unlock(game_menu_exit_lock);

    view_holder_set_view(view_holder, NULL);
    // Delete everything to prevent memory leaks.
    view_holder_free(view_holder);
    submenu_free(submenu);
    // End access to the GUI API.
    furi_record_close(RECORD_GUI);
}
