/* Levels of the chosen opponent: the level range of random teams, or a
 * boss's levels (the game's, or scaled so its ace has the chosen level).
 * Changes apply at once; Back or OK returns to the main menu and saves. */
#include "../pkb_app.h"
#include "pkb_scene.h"

#define LEVEL_VALUES (PKB_LEVEL_MAX - PKB_RANDOM_LEVEL_MIN + 1)
/* boss: "Original", then the ace at 50, 55, ... 100 */
#define ACE_FIRST    50
#define ACE_STEP     5
#define ACE_VALUES   (1 + (PKB_LEVEL_MAX - ACE_FIRST) / ACE_STEP + 1)

#define LEVELS_EVENT_DONE 0x3000

static VariableItem* item_lo;
static VariableItem* item_hi;

static void show_level(VariableItem* item, uint8_t level) {
    char text[8];
    snprintf(text, sizeof(text), "%u", level);
    variable_item_set_current_value_index(item, level - PKB_RANDOM_LEVEL_MIN);
    variable_item_set_current_value_text(item, text);
}

static void lo_changed(VariableItem* item) {
    PkbApp* app = variable_item_get_context(item);
    PkbSettings* s = &app->settings;
    s->level_lo = (uint8_t)(PKB_RANDOM_LEVEL_MIN + variable_item_get_current_value_index(item));
    if(s->level_hi < s->level_lo) {
        s->level_hi = s->level_lo;
        show_level(item_hi, s->level_hi);
    }
    show_level(item, s->level_lo);
    app->have_team = false;
}

static void hi_changed(VariableItem* item) {
    PkbApp* app = variable_item_get_context(item);
    PkbSettings* s = &app->settings;
    s->level_hi = (uint8_t)(PKB_RANDOM_LEVEL_MIN + variable_item_get_current_value_index(item));
    if(s->level_lo > s->level_hi) {
        s->level_lo = s->level_hi;
        show_level(item_lo, s->level_lo);
    }
    show_level(item, s->level_hi);
    app->have_team = false;
}

static void show_ace(VariableItem* item, const PkbApp* app) {
    char text[16];
    uint8_t ace = app->settings.boss_ace;
    if(ace)
        snprintf(text, sizeof(text), "Lv%u", ace);
    else
        snprintf(text, sizeof(text), "Game (Lv%u)", pkb_boss_ace_level(app->settings.boss));
    variable_item_set_current_value_index(item, ace ? 1 + (ace - ACE_FIRST) / ACE_STEP : 0);
    variable_item_set_current_value_text(item, text);
}

static void ace_changed(VariableItem* item) {
    PkbApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.boss_ace = i ? (uint8_t)(ACE_FIRST + (i - 1) * ACE_STEP) : 0;
    show_ace(item, app);
    app->have_team = false;
}

static void levels_enter(void* context, uint32_t index) {
    UNUSED(index);
    PkbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LEVELS_EVENT_DONE);
}

void pkb_scene_levels_on_enter(void* context) {
    PkbApp* app = context;
    VariableItemList* list = app->variable_item_list;

    variable_item_list_reset(list);
    if(app->settings.kind == PkbOpponentRandom) {
        item_lo = variable_item_list_add(list, "Lowest level", LEVEL_VALUES, lo_changed, app);
        item_hi = variable_item_list_add(list, "Highest level", LEVEL_VALUES, hi_changed, app);
        show_level(item_lo, app->settings.level_lo);
        show_level(item_hi, app->settings.level_hi);
    } else {
        char label[32];
        snprintf(label, sizeof(label), "%s's ace", pkb_boss_label(app->settings.boss));
        show_ace(variable_item_list_add(list, label, ACE_VALUES, ace_changed, app), app);
    }
    variable_item_list_set_enter_callback(list, levels_enter, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewVariableItemList);
}

bool pkb_scene_levels_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;
    bool done = event.type == SceneManagerEventTypeBack ||
                (event.type == SceneManagerEventTypeCustom && event.event == LEVELS_EVENT_DONE);
    if(!done) return false;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, PkbSceneMainMenu);
    return true;
}

void pkb_scene_levels_on_exit(void* context) {
    PkbApp* app = context;
    pkb_settings_save(app);
    variable_item_list_reset(app->variable_item_list);
    item_lo = item_hi = NULL;
}
