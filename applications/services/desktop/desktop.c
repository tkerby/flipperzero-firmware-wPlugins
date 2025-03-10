#include <assets_icons.h>
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <gui/view_stack.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <furi.h>
#include <furi_hal.h>
#include <cli/cli.h>
#include <cli/cli_vcp.h>
#include <locale/locale.h>

#include "animations/animation_manager.h"
#include "desktop/scenes/desktop_scene.h"
#include "desktop/scenes/desktop_scene_i.h"
#include "desktop/views/desktop_view_locked.h"
#include "desktop/views/desktop_view_pin_input.h"
#include "desktop/views/desktop_view_pin_timeout.h"
#include "desktop_i.h"
#include "helpers/pin.h"
#include <cfw/private.h>

#define TAG "Desktop"

static void desktop_auto_lock_arm(Desktop*);
static void desktop_auto_lock_inhibit(Desktop*);
static void desktop_start_auto_lock_timer(Desktop*);

static void desktop_loader_callback(const void* message, void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    const LoaderEvent* event = message;

    if(event->type == LoaderEventTypeApplicationBeforeLoad) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalBeforeAppStarted);
        furi_check(furi_semaphore_acquire(desktop->animation_semaphore, 3000) == FuriStatusOk);
    } else if(
        event->type == LoaderEventTypeApplicationLoadFailed ||
        event->type == LoaderEventTypeApplicationStopped) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalAfterAppFinished);
    }
}

static void desktop_sdcard_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);

    canvas_draw_icon(canvas, 0, 0, &I_SDcardMounted_11x8);
}

static void storage_Desktop_status_callback(const void* message, void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    const StorageEvent* storage_event = message;

    if((storage_event->type == StorageEventTypeCardUnmount) ||
       (storage_event->type == StorageEventTypeCardMountError)) {
        view_port_enabled_set(desktop->sdcard_icon_viewport, false);
        view_port_enabled_set(desktop->sdcard_icon_slim_viewport, false);
        desktop->sdcard_status = false;
    }

    if(storage_event->type == StorageEventTypeCardMount) {
        switch(desktop->settings.icon_style) {
        case ICON_STYLE_SLIM:
            view_port_enabled_set(desktop->sdcard_icon_viewport, false);
            view_port_enabled_set(desktop->sdcard_icon_slim_viewport, desktop->settings.sdcard);
            view_port_update(desktop->sdcard_icon_slim_viewport);
            break;
        case ICON_STYLE_STOCK:
            view_port_enabled_set(desktop->sdcard_icon_viewport, desktop->settings.sdcard);
            view_port_enabled_set(desktop->sdcard_icon_slim_viewport, false);
            view_port_update(desktop->sdcard_icon_viewport);
            break;
        }
        desktop->sdcard_status = true;
    }
}

static void desktop_bt_icon_draw_idle_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);

    canvas_draw_icon(canvas, 0, 0, &I_Bluetooth_Idle_5x8);
}

static void desktop_bt_icon_draw_connected_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);

    canvas_draw_icon(canvas, 0, 0, &I_Bluetooth_Connected_16x8);
}

static void desktop_bt_connection_status_update_icon(BtStatus status, void* context) {
    furi_assert(context);
    Desktop* desktop = context;

    if(status == BtStatusAdvertising) {
        switch(desktop->settings.icon_style) {
        case ICON_STYLE_SLIM:
            view_port_set_width(
                desktop->bt_icon_slim_viewport, icon_get_width(&I_Bluetooth_Idle_5x8));
            view_port_draw_callback_set(
                desktop->bt_icon_slim_viewport, desktop_bt_icon_draw_idle_callback, desktop);
            view_port_enabled_set(desktop->bt_icon_viewport, false);
            view_port_enabled_set(desktop->bt_icon_slim_viewport, desktop->settings.bt_icon);
            view_port_update(desktop->bt_icon_slim_viewport);
            break;
        case ICON_STYLE_STOCK:
            view_port_set_width(desktop->bt_icon_viewport, icon_get_width(&I_Bluetooth_Idle_5x8));
            view_port_draw_callback_set(
                desktop->bt_icon_viewport, desktop_bt_icon_draw_idle_callback, desktop);
            view_port_enabled_set(desktop->bt_icon_viewport, desktop->settings.bt_icon);
            view_port_enabled_set(desktop->bt_icon_slim_viewport, false);
            view_port_update(desktop->bt_icon_viewport);
            break;
        }
    } else if(status == BtStatusConnected) {
        switch(desktop->settings.icon_style) {
        case ICON_STYLE_SLIM:
            view_port_set_width(
                desktop->bt_icon_slim_viewport, icon_get_width(&I_Bluetooth_Connected_16x8));
            view_port_draw_callback_set(
                desktop->bt_icon_slim_viewport, desktop_bt_icon_draw_connected_callback, desktop);
            view_port_enabled_set(desktop->bt_icon_viewport, false);
            view_port_enabled_set(desktop->bt_icon_slim_viewport, desktop->settings.bt_icon);
            view_port_update(desktop->bt_icon_slim_viewport);
            break;
        case ICON_STYLE_STOCK:
            view_port_set_width(
                desktop->bt_icon_viewport, icon_get_width(&I_Bluetooth_Connected_16x8));
            view_port_draw_callback_set(
                desktop->bt_icon_viewport, desktop_bt_icon_draw_connected_callback, desktop);
            view_port_enabled_set(desktop->bt_icon_viewport, desktop->settings.bt_icon);
            view_port_enabled_set(desktop->bt_icon_slim_viewport, false);
            view_port_update(desktop->bt_icon_viewport);
            break;
        }
    } else {
        view_port_enabled_set(desktop->bt_icon_slim_viewport, false);
        view_port_enabled_set(desktop->bt_icon_viewport, false);
        view_port_update(desktop->bt_icon_viewport);
        view_port_update(desktop->bt_icon_slim_viewport);
    }
}

static void desktop_lock_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_Lock_7x8);
}

static void desktop_dummy_mode_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_GameMode_11x8);
}

static void desktop_topbar_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);

    canvas_set_bitmap_mode(canvas, 1);
    canvas_draw_icon(canvas, 0, 0, &I_Background_128x11);
    canvas_set_bitmap_mode(canvas, 0);
}

static void desktop_clock_upd_time(Desktop* desktop, bool forced) {
    furi_assert(desktop);
    DateTime curr_dt;
    furi_hal_rtc_get_datetime(&curr_dt);
    bool time_format_12 = locale_get_time_format() == LocaleTimeFormat12h;

    if(desktop->clock.hour != curr_dt.hour || desktop->clock.minute != curr_dt.minute ||
       desktop->clock.format_12 != time_format_12 || forced) {
        desktop->clock.format_12 = time_format_12;
        desktop->clock.hour = curr_dt.hour;
        desktop->clock.minute = curr_dt.minute;
        view_port_update(desktop->clock_viewport);
    }
}

static void desktop_clock_toggle_view(Desktop* desktop, bool is_enabled) {
    furi_assert(desktop);

    desktop_clock_upd_time(desktop, true);

    if(is_enabled) { // && !furi_timer_is_running(desktop->update_clock_timer)) {
        furi_timer_start(desktop->update_clock_timer, furi_ms_to_ticks(1000));
    } else if(!is_enabled) { //&& furi_timer_is_running(desktop->update_clock_timer)) {
        furi_timer_stop(desktop->update_clock_timer);
    }

    switch(desktop->settings.icon_style) {
    case ICON_STYLE_SLIM:
        view_port_enabled_set(desktop->clock_slim_viewport, is_enabled);
        view_port_enabled_set(desktop->clock_viewport, false);
        break;
    case ICON_STYLE_STOCK:
        view_port_enabled_set(desktop->clock_slim_viewport, false);
        view_port_enabled_set(desktop->clock_viewport, is_enabled);
        break;
    }
}

static void desktop_clock_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    furi_assert(canvas);

    Desktop* desktop = context;

    canvas_set_font(canvas, FontPrimary);

    uint8_t hour = desktop->clock.hour;
    if(desktop->clock.format_12) {
        if(hour > 12) {
            hour -= 12;
        }
        if(hour == 0) {
            hour = 12;
        }
    }

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02u:%02u", hour, desktop->clock.minute);

    view_port_set_width(
        desktop->clock_viewport,
        canvas_string_width(canvas, buffer) - 1 + (desktop->clock.minute % 10 == 1));

    canvas_draw_str_aligned(canvas, 0, 8, AlignLeft, AlignBottom, buffer);
}

static void desktop_stealth_mode_icon_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    furi_assert(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_Muted_8x8);
}

static bool desktop_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Desktop* desktop = (Desktop*)context;

    switch(event) {
    case DesktopGlobalBeforeAppStarted:
        if(animation_manager_is_animation_loaded(desktop->animation_manager)) {
            animation_manager_unload_and_stall_animation(desktop->animation_manager);
        }
        desktop_auto_lock_inhibit(desktop);
        furi_semaphore_release(desktop->animation_semaphore);
        return true;
    case DesktopGlobalAfterAppFinished:
        animation_manager_load_and_continue_animation(desktop->animation_manager);
        DESKTOP_SETTINGS_LOAD(&desktop->settings);
        desktop_clock_toggle_view(desktop, desktop->settings.display_clock);
        if(!furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock)) {
            desktop_auto_lock_arm(desktop);
        }
        return true;
    case DesktopGlobalAutoLock:
        if(!loader_is_locked(desktop->loader) && !desktop->locked) {
            desktop_lock(desktop);
        }
        return true;
    }

    return scene_manager_handle_custom_event(desktop->scene_manager, event);
}

static bool desktop_back_event_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = (Desktop*)context;
    return scene_manager_handle_back_event(desktop->scene_manager);
}

static void desktop_tick_event_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = context;

    if(desktop->settings.bt_icon) {
        BtStatus status = bt_get_status(desktop->bt);
        desktop_bt_connection_status_update_icon(status, desktop);
    } else {
        view_port_enabled_set(desktop->bt_icon_viewport, false);
        view_port_enabled_set(desktop->bt_icon_slim_viewport, false);
    }

    view_port_enabled_set(desktop->topbar_icon_viewport, desktop->settings.top_bar);

    switch(desktop->settings.icon_style) {
    case ICON_STYLE_SLIM:
        //dummy mode icon
        if(desktop->settings.dumbmode_icon) {
            view_port_enabled_set(desktop->dummy_mode_icon_viewport, false);
            view_port_enabled_set(
                desktop->dummy_mode_icon_slim_viewport, desktop->settings.dummy_mode);
        }
        //stealth icon
        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
            view_port_enabled_set(desktop->stealth_mode_icon_viewport, false);
            view_port_enabled_set(
                desktop->stealth_mode_icon_slim_viewport, desktop->settings.stealth_icon);
        }
        if(desktop->sdcard_status) {
            //sdcard icon
            view_port_enabled_set(desktop->sdcard_icon_viewport, false);
            view_port_enabled_set(desktop->sdcard_icon_slim_viewport, desktop->settings.sdcard);
        }
        break;
    case ICON_STYLE_STOCK:
        //dummy mode icon
        if(desktop->settings.dumbmode_icon) {
            view_port_enabled_set(desktop->dummy_mode_icon_viewport, desktop->settings.dummy_mode);
            view_port_enabled_set(desktop->dummy_mode_icon_slim_viewport, false);
        }
        //stealth icon
        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
            view_port_enabled_set(
                desktop->stealth_mode_icon_viewport, desktop->settings.stealth_icon);
            view_port_enabled_set(desktop->stealth_mode_icon_slim_viewport, false);
        }
        if(desktop->sdcard_status) {
            //sdcard icon
            view_port_enabled_set(desktop->sdcard_icon_viewport, desktop->settings.sdcard);
            view_port_enabled_set(desktop->sdcard_icon_slim_viewport, false);
        }
        break;
    }

    scene_manager_handle_tick_event(desktop->scene_manager);
}

static void desktop_input_event_callback(const void* value, void* context) {
    furi_assert(value);
    furi_assert(context);
    const InputEvent* event = value;
    Desktop* desktop = context;
    if(event->type == InputTypePress) {
        desktop_start_auto_lock_timer(desktop);
    }
}

static void desktop_auto_lock_timer_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopGlobalAutoLock);
}

static void desktop_start_auto_lock_timer(Desktop* desktop) {
    furi_timer_start(
        desktop->auto_lock_timer, furi_ms_to_ticks(desktop->settings.auto_lock_delay_ms));
}

static void desktop_stop_auto_lock_timer(Desktop* desktop) {
    furi_timer_stop(desktop->auto_lock_timer);
}

static void desktop_auto_lock_arm(Desktop* desktop) {
    if(desktop->settings.auto_lock_delay_ms) {
        if(!desktop->input_events_subscription) {
            desktop->input_events_subscription = furi_pubsub_subscribe(
                desktop->input_events_pubsub, desktop_input_event_callback, desktop);
            desktop_start_auto_lock_timer(desktop);
        }
    }
}

static void desktop_auto_lock_inhibit(Desktop* desktop) {
    desktop_stop_auto_lock_timer(desktop);
    if(desktop->input_events_subscription) {
        furi_pubsub_unsubscribe(desktop->input_events_pubsub, desktop->input_events_subscription);
        desktop->input_events_subscription = NULL;
    }
}

static void desktop_clock_timer_callback(void* context) {
    furi_assert(context);
    Desktop* desktop = context;

    switch(desktop->settings.icon_style) {
    case ICON_STYLE_SLIM:
        if(gui_get_count_of_enabled_view_port_in_layer(desktop->gui, GuiLayerStatusBarLeftSlim) <
           6) {
            desktop_clock_upd_time(desktop, false);
            DateTime curr_dt;
            furi_hal_rtc_get_datetime(&curr_dt);

            if(desktop->clock.minute != curr_dt.minute) {
                if(desktop->clock.format_12) {
                    desktop->clock.hour = curr_dt.hour;
                } else {
                    desktop->clock.hour = (curr_dt.hour > 12) ?
                                              curr_dt.hour - 12 :
                                              ((curr_dt.hour == 0) ? 12 : curr_dt.hour);
                }
                desktop->clock.minute = curr_dt.minute;
                view_port_update(desktop->clock_slim_viewport);
            }
            view_port_enabled_set(desktop->clock_slim_viewport, true);
            view_port_enabled_set(desktop->clock_viewport, false);
        } else {
            view_port_enabled_set(desktop->clock_slim_viewport, false);
            view_port_enabled_set(desktop->clock_viewport, false);
        }
        break;
    case ICON_STYLE_STOCK:
        if(gui_get_count_of_enabled_view_port_in_layer(desktop->gui, GuiLayerStatusBarLeft) < 6) {
            desktop_clock_upd_time(desktop, false);
            DateTime curr_dt;
            furi_hal_rtc_get_datetime(&curr_dt);

            if(desktop->clock.minute != curr_dt.minute) {
                if(desktop->clock.format_12) {
                    desktop->clock.hour = curr_dt.hour;
                } else {
                    desktop->clock.hour = (curr_dt.hour > 12) ?
                                              curr_dt.hour - 12 :
                                              ((curr_dt.hour == 0) ? 12 : curr_dt.hour);
                }
                desktop->clock.minute = curr_dt.minute;
                view_port_update(desktop->clock_viewport);
            }

            view_port_enabled_set(desktop->clock_slim_viewport, false);
            view_port_enabled_set(desktop->clock_viewport, true);
        } else {
            view_port_enabled_set(desktop->clock_slim_viewport, false);
            view_port_enabled_set(desktop->clock_viewport, false);
        }
        break;
    }
}

void desktop_lock(Desktop* desktop) {
    furi_assert(!desktop->locked);

    furi_hal_rtc_set_flag(FuriHalRtcFlagLock);

    if(desktop->settings.pin_code.length) {
        Cli* cli = furi_record_open(RECORD_CLI);
        cli_session_close(cli);
        furi_record_close(RECORD_CLI);
    }

    desktop_auto_lock_inhibit(desktop);
    scene_manager_set_scene_state(
        desktop->scene_manager, DesktopSceneLocked, SCENE_LOCKED_FIRST_ENTER);
    scene_manager_next_scene(desktop->scene_manager, DesktopSceneLocked);

    DesktopStatus status = {.locked = true};
    furi_pubsub_publish(desktop->status_pubsub, &status);

    desktop->locked = true;
}

void desktop_unlock(Desktop* desktop) {
    furi_assert(desktop->locked);

    view_port_enabled_set(desktop->lock_icon_viewport, false);
    view_port_enabled_set(desktop->lock_icon_slim_viewport, false);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_set_lockdown(gui, false);
    furi_record_close(RECORD_GUI);
    desktop_view_locked_unlock(desktop->locked_view);
    scene_manager_search_and_switch_to_previous_scene(desktop->scene_manager, DesktopSceneMain);
    desktop_auto_lock_arm(desktop);
    furi_hal_rtc_reset_flag(FuriHalRtcFlagLock);
    furi_hal_rtc_set_pin_fails(0);

    if(desktop->settings.pin_code.length) {
        Cli* cli = furi_record_open(RECORD_CLI);
        cli_session_open(cli, &cli_vcp);
        furi_record_close(RECORD_CLI);
    }

    DesktopStatus status = {.locked = false};
    furi_pubsub_publish(desktop->status_pubsub, &status);

    desktop->locked = false;
}

void desktop_set_dummy_mode_state(Desktop* desktop, bool enabled) {
    desktop->in_transition = true;
    if(desktop->settings.dumbmode_icon) {
        switch(desktop->settings.icon_style) {
        case ICON_STYLE_SLIM:
            view_port_enabled_set(desktop->dummy_mode_icon_viewport, false);
            view_port_enabled_set(desktop->dummy_mode_icon_slim_viewport, enabled);
            break;
        case ICON_STYLE_STOCK:
            view_port_enabled_set(desktop->dummy_mode_icon_viewport, enabled);
            view_port_enabled_set(desktop->dummy_mode_icon_slim_viewport, false);
            break;
        }
    }
    desktop_main_set_dummy_mode_state(desktop->main_view, enabled);
    animation_manager_set_dummy_mode_state(desktop->animation_manager, enabled);
    desktop->settings.dummy_mode = enabled;
    DESKTOP_SETTINGS_SAVE(&desktop->settings);
    desktop->in_transition = false;
}

void desktop_set_stealth_mode_state(Desktop* desktop, bool enabled) {
    desktop->in_transition = true;
    if(enabled) {
        furi_hal_rtc_set_flag(FuriHalRtcFlagStealthMode);
    } else {
        furi_hal_rtc_reset_flag(FuriHalRtcFlagStealthMode);
    }
    if(desktop->settings.stealth_icon) {
        switch(desktop->settings.icon_style) {
        case ICON_STYLE_SLIM:
            view_port_enabled_set(desktop->stealth_mode_icon_viewport, false);
            view_port_enabled_set(desktop->stealth_mode_icon_slim_viewport, enabled);
            break;
        case ICON_STYLE_STOCK:
            view_port_enabled_set(desktop->stealth_mode_icon_viewport, enabled);
            view_port_enabled_set(desktop->stealth_mode_icon_slim_viewport, false);
            break;
        }
    }
    desktop->in_transition = false;
}

Desktop* desktop_alloc(void) {
    Desktop* desktop = malloc(sizeof(Desktop));

    desktop->animation_semaphore = furi_semaphore_alloc(1, 0);
    desktop->animation_manager = animation_manager_alloc();
    desktop->gui = furi_record_open(RECORD_GUI);
    desktop->scene_thread = furi_thread_alloc();
    desktop->view_dispatcher = view_dispatcher_alloc();
    desktop->scene_manager = scene_manager_alloc(&desktop_scene_handlers, desktop);

    view_dispatcher_enable_queue(desktop->view_dispatcher);
    view_dispatcher_attach_to_gui(
        desktop->view_dispatcher, desktop->gui, ViewDispatcherTypeDesktop);
    view_dispatcher_set_tick_event_callback(
        desktop->view_dispatcher, desktop_tick_event_callback, 500);

    view_dispatcher_set_event_callback_context(desktop->view_dispatcher, desktop);
    view_dispatcher_set_custom_event_callback(
        desktop->view_dispatcher, desktop_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        desktop->view_dispatcher, desktop_back_event_callback);

    desktop->lock_menu = desktop_lock_menu_alloc();
    desktop->popup = popup_alloc();
    desktop->locked_view = desktop_view_locked_alloc();
    desktop->pin_input_view = desktop_view_pin_input_alloc();
    desktop->pin_timeout_view = desktop_view_pin_timeout_alloc();
    desktop->slideshow_view = desktop_view_slideshow_alloc();

    desktop->main_view_stack = view_stack_alloc();
    desktop->main_view = desktop_main_alloc();
    View* dolphin_view = animation_manager_get_animation_view(desktop->animation_manager);
    view_stack_add_view(desktop->main_view_stack, desktop_main_get_view(desktop->main_view));
    view_stack_add_view(desktop->main_view_stack, dolphin_view);
    view_stack_add_view(
        desktop->main_view_stack, desktop_view_locked_get_view(desktop->locked_view));

    /* locked view (as animation view) attends in 2 scenes: main & locked,
     * because it has to draw "Unlocked" label on main scene */
    desktop->locked_view_stack = view_stack_alloc();
    view_stack_add_view(desktop->locked_view_stack, dolphin_view);
    view_stack_add_view(
        desktop->locked_view_stack, desktop_view_locked_get_view(desktop->locked_view));

    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdMain,
        view_stack_get_view(desktop->main_view_stack));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdLocked,
        view_stack_get_view(desktop->locked_view_stack));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdLockMenu,
        desktop_lock_menu_get_view(desktop->lock_menu));
    view_dispatcher_add_view(
        desktop->view_dispatcher, DesktopViewIdPopup, popup_get_view(desktop->popup));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdPinTimeout,
        desktop_view_pin_timeout_get_view(desktop->pin_timeout_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdPinInput,
        desktop_view_pin_input_get_view(desktop->pin_input_view));
    view_dispatcher_add_view(
        desktop->view_dispatcher,
        DesktopViewIdSlideshow,
        desktop_view_slideshow_get_view(desktop->slideshow_view));

    // Lock icon
    desktop->lock_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->lock_icon_viewport, icon_get_width(&I_Lock_7x8));
    view_port_draw_callback_set(
        desktop->lock_icon_viewport, desktop_lock_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->lock_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->lock_icon_viewport, GuiLayerStatusBarLeft);

    // Lock icon - Slim
    desktop->lock_icon_slim_viewport = view_port_alloc();
    view_port_set_width(desktop->lock_icon_slim_viewport, icon_get_width(&I_Lock_7x8));
    view_port_draw_callback_set(
        desktop->lock_icon_slim_viewport, desktop_lock_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->lock_icon_slim_viewport, false);
    gui_add_view_port(desktop->gui, desktop->lock_icon_slim_viewport, GuiLayerStatusBarLeftSlim);

    // Dummy mode icon
    desktop->dummy_mode_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->dummy_mode_icon_viewport, icon_get_width(&I_GameMode_11x8));
    view_port_draw_callback_set(
        desktop->dummy_mode_icon_viewport, desktop_dummy_mode_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->dummy_mode_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->dummy_mode_icon_viewport, GuiLayerStatusBarLeft);

    // Dummy mode icon - Slim
    desktop->dummy_mode_icon_slim_viewport = view_port_alloc();
    view_port_set_width(desktop->dummy_mode_icon_slim_viewport, icon_get_width(&I_GameMode_11x8));
    view_port_draw_callback_set(
        desktop->dummy_mode_icon_slim_viewport, desktop_dummy_mode_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->dummy_mode_icon_slim_viewport, false);
    gui_add_view_port(
        desktop->gui, desktop->dummy_mode_icon_slim_viewport, GuiLayerStatusBarLeftSlim);

    // SD card icon hack
    desktop->sdcard_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->sdcard_icon_viewport, icon_get_width(&I_SDcardMounted_11x8));
    view_port_draw_callback_set(
        desktop->sdcard_icon_viewport, desktop_sdcard_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->sdcard_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->sdcard_icon_viewport, GuiLayerStatusBarLeft);

    // SD card icon hack - Slim
    desktop->sdcard_icon_slim_viewport = view_port_alloc();
    view_port_set_width(desktop->sdcard_icon_slim_viewport, icon_get_width(&I_SDcardMounted_11x8));
    view_port_draw_callback_set(
        desktop->sdcard_icon_slim_viewport, desktop_sdcard_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->sdcard_icon_slim_viewport, false);
    gui_add_view_port(desktop->gui, desktop->sdcard_icon_slim_viewport, GuiLayerStatusBarLeftSlim);

    // BT icon hack
    desktop->bt_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->bt_icon_viewport, icon_get_width(&I_Bluetooth_Idle_5x8));
    view_port_draw_callback_set(
        desktop->bt_icon_viewport, desktop_bt_icon_draw_idle_callback, desktop);
    view_port_enabled_set(desktop->bt_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->bt_icon_viewport, GuiLayerStatusBarLeft);

    // BT icon hack - Slim
    desktop->bt_icon_slim_viewport = view_port_alloc();
    view_port_set_width(desktop->bt_icon_slim_viewport, icon_get_width(&I_Bluetooth_Idle_5x8));
    view_port_draw_callback_set(
        desktop->bt_icon_slim_viewport, desktop_bt_icon_draw_idle_callback, desktop);
    view_port_enabled_set(desktop->bt_icon_slim_viewport, false);
    gui_add_view_port(desktop->gui, desktop->bt_icon_slim_viewport, GuiLayerStatusBarLeftSlim);

    // Clock
    desktop->clock_viewport = view_port_alloc();
    view_port_set_width(desktop->clock_viewport, 25);
    view_port_draw_callback_set(desktop->clock_viewport, desktop_clock_draw_callback, desktop);
    view_port_enabled_set(desktop->clock_viewport, false);
    gui_add_view_port(desktop->gui, desktop->clock_viewport, GuiLayerStatusBarRight);

    // Clock Slim
    desktop->clock_slim_viewport = view_port_alloc();
    view_port_set_width(desktop->clock_slim_viewport, 25);
    view_port_draw_callback_set(
        desktop->clock_slim_viewport, desktop_clock_draw_callback, desktop);
    view_port_enabled_set(desktop->clock_slim_viewport, false);
    gui_add_view_port(desktop->gui, desktop->clock_slim_viewport, GuiLayerStatusBarRightSlim);

    // Stealth mode icon
    desktop->stealth_mode_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->stealth_mode_icon_viewport, icon_get_width(&I_Muted_8x8));
    view_port_draw_callback_set(
        desktop->stealth_mode_icon_viewport, desktop_stealth_mode_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->stealth_mode_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->stealth_mode_icon_viewport, GuiLayerStatusBarLeft);

    // Stealth mode Slim icon
    desktop->stealth_mode_icon_slim_viewport = view_port_alloc();
    view_port_set_width(desktop->stealth_mode_icon_slim_viewport, icon_get_width(&I_Muted_8x8));
    view_port_draw_callback_set(
        desktop->stealth_mode_icon_slim_viewport,
        desktop_stealth_mode_icon_draw_callback,
        desktop);
    view_port_enabled_set(desktop->stealth_mode_icon_slim_viewport, false);
    gui_add_view_port(
        desktop->gui, desktop->stealth_mode_icon_slim_viewport, GuiLayerStatusBarLeftSlim);

    // Top bar icon
    desktop->topbar_icon_viewport = view_port_alloc();
    view_port_set_width(desktop->topbar_icon_viewport, icon_get_width(&I_Background_128x11));
    view_port_draw_callback_set(
        desktop->topbar_icon_viewport, desktop_topbar_icon_draw_callback, desktop);
    view_port_enabled_set(desktop->topbar_icon_viewport, false);
    gui_add_view_port(desktop->gui, desktop->topbar_icon_viewport, GuiLayerStatusBarTop);

    // Special case: autostart application is already running
    desktop->loader = furi_record_open(RECORD_LOADER);
    if(loader_is_locked(desktop->loader) &&
       animation_manager_is_animation_loaded(desktop->animation_manager)) {
        animation_manager_unload_and_stall_animation(desktop->animation_manager);
    }

    desktop->notification = furi_record_open(RECORD_NOTIFICATION);
    desktop->app_start_stop_subscription = furi_pubsub_subscribe(
        loader_get_pubsub(desktop->loader), desktop_loader_callback, desktop);

    desktop->input_events_pubsub = furi_record_open(RECORD_INPUT_EVENTS);
    desktop->input_events_subscription = NULL;

    desktop->auto_lock_timer =
        furi_timer_alloc(desktop_auto_lock_timer_callback, FuriTimerTypeOnce, desktop);

    desktop->status_pubsub = furi_pubsub_alloc();

    desktop->update_clock_timer =
        furi_timer_alloc(desktop_clock_timer_callback, FuriTimerTypePeriodic, desktop);

    DateTime curr_dt;
    furi_hal_rtc_get_datetime(&curr_dt);

    desktop_clock_upd_time(desktop, true);

    desktop->sdcard_status = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    desktop->storage_sub = furi_pubsub_subscribe(
        storage_get_pubsub(storage), storage_Desktop_status_callback, desktop);
    furi_record_close(RECORD_STORAGE);

    desktop->bt = furi_record_open(RECORD_BT);

    furi_record_create(RECORD_DESKTOP, desktop);

    return desktop;
}

static bool desktop_check_file_flag(const char* flag_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool exists = storage_common_stat(storage, flag_path, NULL) == FSE_OK;
    furi_record_close(RECORD_STORAGE);

    return exists;
}

bool desktop_api_is_locked(Desktop* instance) {
    furi_assert(instance);
    return furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock);
}

void desktop_api_unlock(Desktop* instance) {
    furi_assert(instance);
    view_dispatcher_send_custom_event(instance->view_dispatcher, DesktopGlobalApiUnlock);
}

FuriPubSub* desktop_api_get_status_pubsub(Desktop* instance) {
    furi_assert(instance);
    return instance->status_pubsub;
}

int32_t desktop_srv(void* p) {
    UNUSED(p);

    if(!furi_hal_is_normal_boot()) {
        FURI_LOG_W(TAG, "Skipping start in special boot mode");

        furi_thread_suspend(furi_thread_get_current_id());
        return 0;
    }

    cfw_settings_load();

    Desktop* desktop = desktop_alloc();

    bool loaded = DESKTOP_SETTINGS_LOAD(&desktop->settings);
    if(!loaded) {
        memset(&desktop->settings, 0, sizeof(desktop->settings));
        desktop->settings.displayBatteryPercentage = DISPLAY_BATTERY_BAR_PERCENT;
        desktop->settings.icon_style = ICON_STYLE_SLIM;
        desktop->settings.lock_icon = true;
        desktop->settings.bt_icon = true;
        desktop->settings.rpc_icon = true;
        desktop->settings.sdcard = true;
        desktop->settings.stealth_icon = true;
        desktop->settings.top_bar = false;
        desktop->settings.dummy_mode = false;
        desktop->settings.dumbmode_icon = true;
        DESKTOP_SETTINGS_SAVE(&desktop->settings);
    }

    view_port_enabled_set(desktop->topbar_icon_viewport, desktop->settings.top_bar);

    switch(desktop->settings.icon_style) {
    case ICON_STYLE_SLIM:
        //dummy mode icon
        if(desktop->settings.dumbmode_icon) {
            view_port_enabled_set(desktop->dummy_mode_icon_viewport, false);
            view_port_enabled_set(
                desktop->dummy_mode_icon_slim_viewport, desktop->settings.dummy_mode);
        }
        //stealth icon
        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
            view_port_enabled_set(desktop->stealth_mode_icon_viewport, false);
            view_port_enabled_set(
                desktop->stealth_mode_icon_slim_viewport, desktop->settings.stealth_icon);
        }
        //sdcard icon
        view_port_enabled_set(desktop->sdcard_icon_viewport, false);
        view_port_enabled_set(desktop->sdcard_icon_slim_viewport, desktop->settings.sdcard);
        //bt icon
        view_port_enabled_set(desktop->bt_icon_viewport, false);
        view_port_enabled_set(desktop->bt_icon_slim_viewport, desktop->settings.bt_icon);
        break;
    case ICON_STYLE_STOCK:
        //dummy mode icon
        if(desktop->settings.dumbmode_icon) {
            view_port_enabled_set(desktop->dummy_mode_icon_viewport, desktop->settings.dummy_mode);
            view_port_enabled_set(desktop->dummy_mode_icon_slim_viewport, false);
        }
        //stealth icon
        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
            view_port_enabled_set(
                desktop->stealth_mode_icon_viewport, desktop->settings.stealth_icon);
            view_port_enabled_set(desktop->stealth_mode_icon_slim_viewport, false);
        }
        //sdcard icon
        view_port_enabled_set(desktop->sdcard_icon_viewport, desktop->settings.sdcard);
        view_port_enabled_set(desktop->sdcard_icon_slim_viewport, false);
        //bt icon
        view_port_enabled_set(desktop->bt_icon_viewport, desktop->settings.bt_icon);
        view_port_enabled_set(desktop->bt_icon_slim_viewport, false);
        break;
    }

    view_port_enabled_set(desktop->dummy_mode_icon_viewport, desktop->settings.dummy_mode);

    desktop_clock_toggle_view(desktop, desktop->settings.display_clock);

    desktop_main_set_dummy_mode_state(desktop->main_view, desktop->settings.dummy_mode);
    animation_manager_set_dummy_mode_state(
        desktop->animation_manager, desktop->settings.dummy_mode);

    scene_manager_next_scene(desktop->scene_manager, DesktopSceneMain);

    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagLock)) {
        desktop_lock(desktop);
    } else {
        if(!loader_is_locked(desktop->loader)) {
            desktop_auto_lock_arm(desktop);
        }
    }

    if(desktop_check_file_flag(SLIDESHOW_FS_PATH)) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneSlideshow);
    }

    if(!furi_hal_version_do_i_belong_here()) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneHwMismatch);
    }

    if(furi_hal_rtc_get_fault_data()) {
        scene_manager_next_scene(desktop->scene_manager, DesktopSceneFault);
    }

    uint8_t keys_total, keys_valid;
    if(!furi_hal_crypto_enclave_verify(&keys_total, &keys_valid)) {
        FURI_LOG_E(
            TAG,
            "Secure Enclave verification failed: total %hhu, valid %hhu",
            keys_total,
            keys_valid);

        scene_manager_next_scene(desktop->scene_manager, DesktopSceneSecureEnclave);
    }

    // Special case: autostart application is already running
    if(loader_is_locked(desktop->loader) &&
       animation_manager_is_animation_loaded(desktop->animation_manager)) {
        animation_manager_unload_and_stall_animation(desktop->animation_manager);
    }

    view_dispatcher_run(desktop->view_dispatcher);

    furi_crash("That was unexpected");

    return 0;
}
