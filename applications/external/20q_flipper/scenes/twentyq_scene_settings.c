#include "../twentyq_app_i.h"

typedef enum {
    SettingIndexQuestions,
    SettingIndexCategory,
    SettingIndexSound,
    SettingIndexVibro,
    SettingIndexDefaultAnswer,
    SettingIndexResetLearned,
    SettingIndexResetStats,
} SettingIndex;

static const uint8_t kQuestionLimits[] = {15, 20, 25};
static const char* const kQuestionLabels[] = {"15", "20", "25"};
static const char* const kOnOff[] = {"Off", "On"};
static const char* const kAskSkip[] = {"Skip", "Ask"};
static const char* const kDefaultAnswer[] = {"Middle", "Yes"};

static void twentyq_settings_questions_changed(VariableItem* item) {
    TwentyQApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, kQuestionLabels[index]);
    app->settings.question_limit = kQuestionLimits[index];
    twentyq_settings_save(app->storage, &app->settings);
}

static void twentyq_settings_category_changed(VariableItem* item) {
    TwentyQApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, kAskSkip[index]);
    app->settings.ask_category = index;
    twentyq_settings_save(app->storage, &app->settings);
}

static void twentyq_settings_sound_changed(VariableItem* item) {
    TwentyQApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, kOnOff[index]);
    app->settings.sound = index;
    twentyq_settings_save(app->storage, &app->settings);
}

static void twentyq_settings_vibro_changed(VariableItem* item) {
    TwentyQApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, kOnOff[index]);
    app->settings.vibro = index;
    twentyq_settings_save(app->storage, &app->settings);
}

static void twentyq_settings_default_changed(VariableItem* item) {
    TwentyQApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, kDefaultAnswer[index]);
    app->settings.default_answer_yes = index;
    twentyq_settings_save(app->storage, &app->settings);
}

static void twentyq_settings_enter_callback(void* context, uint32_t index) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void twentyq_scene_settings_on_enter(void* context) {
    TwentyQApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    uint8_t limit_index = 1;
    for(uint8_t i = 0; i < COUNT_OF(kQuestionLimits); i++) {
        if(kQuestionLimits[i] == app->settings.question_limit) limit_index = i;
    }

    VariableItem* item;
    item = variable_item_list_add(
        list, "Questions", COUNT_OF(kQuestionLimits), twentyq_settings_questions_changed, app);
    variable_item_set_current_value_index(item, limit_index);
    variable_item_set_current_value_text(item, kQuestionLabels[limit_index]);

    item =
        variable_item_list_add(list, "Category hint", 2, twentyq_settings_category_changed, app);
    variable_item_set_current_value_index(item, app->settings.ask_category ? 1 : 0);
    variable_item_set_current_value_text(item, kAskSkip[app->settings.ask_category ? 1 : 0]);

    item = variable_item_list_add(list, "Sound", 2, twentyq_settings_sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, kOnOff[app->settings.sound ? 1 : 0]);

    item = variable_item_list_add(list, "Vibration", 2, twentyq_settings_vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro ? 1 : 0);
    variable_item_set_current_value_text(item, kOnOff[app->settings.vibro ? 1 : 0]);

    item = variable_item_list_add(list, "Start on", 2, twentyq_settings_default_changed, app);
    variable_item_set_current_value_index(item, app->settings.default_answer_yes ? 1 : 0);
    variable_item_set_current_value_text(
        item, kDefaultAnswer[app->settings.default_answer_yes ? 1 : 0]);

    variable_item_list_add(list, "Reset learned words", 0, NULL, app);
    variable_item_list_add(list, "Reset stats", 0, NULL, app);

    variable_item_list_set_enter_callback(list, twentyq_settings_enter_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewVarItemList);
}

bool twentyq_scene_settings_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == SettingIndexResetLearned) {
        tq_learned_clear(&app->learned);
        twentyq_learned_delete_file(app->storage);
        twentyq_show_message(app, "Done", "Learned words cleared.");
        return true;
    }
    if(event.event == SettingIndexResetStats) {
        memset(&app->stats, 0, sizeof(app->stats));
        app->stats.magic = TWENTYQ_STATS_MAGIC;
        app->stats.version = TWENTYQ_STATS_VERSION;
        twentyq_stats_save(app->storage, &app->stats);
        twentyq_show_message(app, "Done", "Stats reset.");
        return true;
    }
    return false;
}

void twentyq_scene_settings_on_exit(void* context) {
    TwentyQApp* app = context;
    twentyq_settings_save(app->storage, &app->settings);
    variable_item_list_reset(app->var_item_list);
}
