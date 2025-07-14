#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include "gui/view.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>

#include "settings.h"
#include "common.h"

VariableItemList* settings_list;

void settings_ui_clear() {
    if(settings_list) {
        variable_item_list_free(settings_list);
        settings_list = NULL;
    }
}

/*--------------------------------------------------------------------------*/
/*  hard limits                                                             */
/*--------------------------------------------------------------------------*/
#define GRAVITY_MIN 100.0
#define GRAVITY_MAX 10000.0

#define PLANETS_MIN 1
#define PLANETS_MAX 8

#define LAUNCH_MIN 0.20 /* s */
#define LAUNCH_MAX 2.00

#define GRID_SPEED_MIN 4.0
#define GRID_SPEED_MAX 32.0

#define GRID_SPACING_MIN 4
#define GRID_SPACING_MAX 32

#define TRAIL_MIN 0 /* s */
#define TRAIL_MAX 5

#define ASTEROID_SPEED_MIN 1
#define ASTEROID_SPEED_MAX 5

#define DEBRI_MIN 0
#define DEBRI_MAX 50

/*--------------------------------------------------------------------------*/
/*  one-line helpers                                                        */
/*--------------------------------------------------------------------------*/
#define MAKE_CB(name, fld, minv, maxv, fmt, cast)                       \
    static void name(VariableItem* i) {                                 \
        int index = variable_item_get_current_value_index(i);           \
        cast value = (maxv - minv) * index + minv; /* linear mapping */ \
        if(value <= (cast)minv) value = (cast)minv;                     \
        if(value >= (cast)maxv) value = (cast)maxv;                     \
        settings.fld = value;                                           \
        char buf[12];                                                   \
        snprintf(buf, sizeof(buf), fmt, settings.fld);                  \
        variable_item_set_current_value_text(i, buf);                   \
    }

/* call-backs */
MAKE_CB(gravity_cb, gravity_force, GRAVITY_MIN, GRAVITY_MAX, "%.0f", double)
MAKE_CB(planets_cb, num_planets, PLANETS_MIN, PLANETS_MAX, "%u", size_t)
MAKE_CB(launch_cb, launch_interval, LAUNCH_MIN, LAUNCH_MAX, "%.2f", double)
MAKE_CB(grid_speed_cb, grid_speed, GRID_SPEED_MIN, GRID_SPEED_MAX, "%.0f", double)
MAKE_CB(grid_space_cb, grid_spacing, GRID_SPACING_MIN, GRID_SPACING_MAX, "%u", size_t)
MAKE_CB(trail_cb, trail_duration, TRAIL_MIN, TRAIL_MAX, "%u", size_t)
MAKE_CB(asteroid_cb, asteroid_speed, ASTEROID_SPEED_MIN, ASTEROID_SPEED_MAX, "%u", size_t)
MAKE_CB(debri_cb, debri_count, DEBRI_MIN, DEBRI_MAX, "%u", size_t)

/*--------------------------------------------------------------------------*/
void settings_ui_init(void) {
    settings_ui_clear();
    settings_list = variable_item_list_alloc();

    VariableItem* it;

    it = variable_item_list_add(settings_list, "Gravity", 40, gravity_cb, NULL);
    variable_item_set_current_value_index(it, settings.gravity_force > (double)GRAVITY_MIN);
    gravity_cb(it);

    it = variable_item_list_add(settings_list, "Planets", 8, planets_cb, NULL);
    variable_item_set_current_value_index(it, settings.num_planets > PLANETS_MIN);
    planets_cb(it);

    it = variable_item_list_add(settings_list, "Asteroid Interval", 40, launch_cb, NULL);
    variable_item_set_current_value_index(it, settings.launch_interval > (double)LAUNCH_MIN);
    launch_cb(it);

    it = variable_item_list_add(settings_list, "Grid speed", 40, grid_speed_cb, NULL);
    variable_item_set_current_value_index(it, settings.grid_speed > (double)GRID_SPEED_MIN);
    grid_speed_cb(it);

    it = variable_item_list_add(settings_list, "Grid spacing", 40, grid_space_cb, NULL);
    variable_item_set_current_value_index(it, settings.grid_spacing > GRID_SPACING_MIN);
    grid_space_cb(it);

    it = variable_item_list_add(settings_list, "Trail duration", 40, trail_cb, NULL);
    variable_item_set_current_value_index(it, settings.trail_duration > TRAIL_MIN);
    trail_cb(it);

    it = variable_item_list_add(settings_list, "Asteroid speed", 5, asteroid_cb, NULL);
    variable_item_set_current_value_index(it, settings.asteroid_speed > ASTEROID_SPEED_MIN);
    asteroid_cb(it);

    it = variable_item_list_add(settings_list, "Debri count", 40, debri_cb, NULL);
    variable_item_set_current_value_index(it, settings.debri_count > DEBRI_MIN);
    debri_cb(it);
}

void settings_ui_open(ViewDispatcher* view_dispatcher) {
    view_dispatcher_switch_to_view(view_dispatcher, SETTINGS_VIEW_ID);
}

uint32_t settings_ui_previous_callback(void* ctx) {
    UNUSED(ctx);
    view_dispatcher_switch_to_view(view_dispatcher, MAIN_VIEW_ID);

    return 0;
}

#endif
