#pragma once

#include <gui/view.h>

/** Main menu: a small custom list (not the stock Submenu widget) so we can
 * draw a little ghost icon under the item list, like the app's own icon. */

typedef struct MenuView MenuView;

typedef void (*MenuSelectCallback)(void* context, uint32_t index);

MenuView* menu_view_alloc(void);
void menu_view_free(MenuView* instance);
View* menu_view_get_view(MenuView* instance);

/** Called with the selected item's index when OK is pressed. */
void menu_view_set_select_callback(MenuView* instance, MenuSelectCallback callback, void* context);
