#include "tea_timer_i.h"

#include <string.h>

#include "scenes/tea_timer_scene.h"
#include "tea_timer_icons.h"
#include "unit_tests/test_index.h"

#define TAG "TeaTimer"

/** Период тика отсчёта, мс. */
#define TEA_TIMER_TICK_PERIOD_MS 1000

/** Пересылка custom-события в активную сцену. */
static bool tea_timer_custom_event_callback(void* context, uint32_t event) {
    TeaTimerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/** BACK: сцена решает, вернуться назад или завершить приложение. */
static bool tea_timer_back_event_callback(void* context) {
    TeaTimerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/** Тик 1 Гц: пересылаем сцене как custom-событие. */
static void tea_timer_tick_event_callback(void* context) {
    TeaTimerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TeaTimerEventTick);
}

static TeaTimerApp* tea_timer_alloc(void) {
    TeaTimerApp* app = malloc(sizeof(TeaTimerApp));
    furi_check(app != NULL);
    memset(app, 0, sizeof(TeaTimerApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // Буфер разбора пресета — в куче, не на стеке (стек 2 КБ).
    app->parse_buf = malloc(TEA_PARSE_BUF_SIZE);
    furi_check(app->parse_buf != NULL);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&tea_timer_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, tea_timer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, tea_timer_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, tea_timer_tick_event_callback, TEA_TIMER_TICK_PERIOD_MS);

    // Главное меню.
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewMenu, submenu_get_view(app->submenu));

    // Системный выбор файла `.tea`.
    app->file_path = furi_string_alloc_set(TEA_TIMER_APP_DATA_PATH);
    app->file_browser = file_browser_alloc(app->file_path);
    file_browser_configure(
        app->file_browser, ".tea", TEA_TIMER_APP_DATA_PATH, false, true, &I_tea_file_10px, true);
    file_browser_set_callback(app->file_browser, tea_timer_scene_file_select_file_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewFileSelect, file_browser_get_view(app->file_browser));

    // Карточка пресета = экран ожидания.
    app->preset_card = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewPresetCard, widget_get_view(app->preset_card));

    // Экран заваривания — единственный собственный View.
    app->brew_view = brew_view_alloc();
    view_set_context(brew_view_get_view(app->brew_view), app);
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewBrew, brew_view_get_view(app->brew_view));

    // «Нет пресетов» и «нет SD-карты» — стандартные Widget.
    app->no_presets = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewNoPresets, widget_get_view(app->no_presets));

    app->error = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TeaTimerViewError, widget_get_view(app->error));

    // Сцены 002: меню действий, форма создания/правки, результат операции.
    app->actions_menu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewActions, submenu_get_view(app->actions_menu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewEditName, text_input_get_view(app->text_input));

    // Один экземпляр number_input на все шесть числовых шагов формы.
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewEditNumber, number_input_get_view(app->number_input));

    app->edit_summary = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewEditSummary, widget_get_view(app->edit_summary));

    app->delete_confirm = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeaTimerViewDeleteConfirm, widget_get_view(app->delete_confirm));

    app->saved = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TeaTimerViewSaved, widget_get_view(app->saved));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, TeaTimerSceneMenu);

#if defined(FURI_DEBUG) || defined(TEA_TIMER_TESTS)
    tea_timer_tests_register();
#endif

    return app;
}

static void tea_timer_free(TeaTimerApp* app) {
    furi_check(app != NULL);

#if defined(FURI_DEBUG) || defined(TEA_TIMER_TESTS)
    tea_timer_tests_unregister();
#endif

    // Активный отсчёт не должен оставить звук, вибро и принудительную подсветку.
    if(app->session.phase == BrewPhaseRUNNING) {
        brew_session_abort(&app->session);
    }
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
    notification_message(app->notification, &sequence_reset_sound);
    notification_message(app->notification, &sequence_reset_vibro);

    // Если выход случился прямо из выбора файла, on_exit сцены мог не выполниться.
    if(app->file_browser_started) {
        file_browser_stop(app->file_browser);
        app->file_browser_started = false;
    }

    // Снятие view в порядке, строго обратном регистрации (принцип V).
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewSaved);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewDeleteConfirm);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewEditSummary);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewEditNumber);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewEditName);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewActions);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewError);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewNoPresets);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewBrew);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewPresetCard);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewFileSelect);
    view_dispatcher_remove_view(app->view_dispatcher, TeaTimerViewMenu);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    brew_view_free(app->brew_view);
    widget_free(app->saved);
    widget_free(app->delete_confirm);
    widget_free(app->edit_summary);
    number_input_free(app->number_input);
    text_input_free(app->text_input);
    submenu_free(app->actions_menu);
    widget_free(app->preset_card);
    widget_free(app->no_presets);
    widget_free(app->error);
    file_browser_free(app->file_browser);
    submenu_free(app->submenu);

    furi_string_free(app->file_path);

    free(app->parse_buf);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t tea_timer_app(void* p) {
    UNUSED(p);
    TeaTimerApp* app = tea_timer_alloc();
    FURI_LOG_I(TAG, "started");

    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "stopped");
    tea_timer_free(app);
    return 0;
}
