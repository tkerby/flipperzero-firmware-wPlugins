#include "include/domain/wc_timefmt.h"
// Every screen that opens the UART lives here: the live scan and the raw serial
// debug view share the same link plumbing, and it stays private to this file.
#include "include/scenes/wc_scenes_common.h"

// ---------------------------------------------------------------------------
// Scan (live counters)
// ---------------------------------------------------------------------------

static void scan_timer_cb(void *context) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WcCustomEventScanTick);
}

static void scan_render(WcApp *app) {
    WcCensusStats s = wc_scan_stats(&app->scan);
    char tail[40];
    if (app->autosave && app->autosave_failed > 0) {
        snprintf(tail, sizeof(tail), "!! %u chunk(s) NOT saved", app->autosave_failed);
    } else if (app->autosave) {
        snprintf(tail, sizeof(tail), "Auto-save: file %u", app->autosave_idx);
    } else if (app->scan.census.dropped > 0) {
        snprintf(tail, sizeof(tail), "FULL +%u dropped", app->scan.census.dropped);
    } else {
        snprintf(tail, sizeof(tail), "Back = stop & save");
    }
    char buf[256];
    snprintf(
        buf, sizeof(buf),
        "Scanning 2.4GHz...\nUnique: %u (stable %u)\nRandom: %u (%u%%)\nPhone %u Lap %u IoT %u\n"
        "Networks sought: %u\n%s",
        s.total, s.unique_stable, s.random_count, s.pct_random, s.by_type[WcDevicePhone],
        s.by_type[WcDeviceLaptop], s.by_type[WcDeviceIot], s.networks, tail);
    widget_reset(app->widget);
    widget_add_string_multiline_element(app->widget, 0, 0, AlignLeft, AlignTop, FontSecondary, buf);
}

// Returns false when the USART is busy; the adapter is freed again so nothing dangles.
static bool scan_serial_start(WcApp *app) {
    app->serial = wc_serial_furi_alloc(app->baud);
    if (!app->serial) {
        return false;
    }
    WcSerialPort port = wc_serial_furi_port(app->serial);
    if (!port.start(port.self, wc_scan_on_line, &app->scan)) {
        wc_serial_furi_free(app->serial);
        app->serial = NULL;
        return false;
    }
    return true;
}

static const char *const k_no_room_text =
    "Not enough memory\nfor a scan.\n\nThe app could not\nreserve room for\n%u devices.\n\n"
    "Leave a capture open?\nGo back to the menu\nand in again.";

static const char *const k_uart_busy_text =
    "UART busy\n\nAnother service holds the\nGPIO serial port.\n\nDisable Settings >\nExpansion "
    "Modules\n(or close the CLI)\nand try again.\n\nBack = menu";

static void scan_show_link_error(WcApp *app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, k_uart_busy_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);
}

static void scan_serial_stop(WcApp *app) {
    if (app->serial) {
        WcSerialPort port = wc_serial_furi_port(app->serial);
        port.stop(port.self);
        wc_serial_furi_free(app->serial);
        app->serial = NULL;
    }
}

// Returns false when the up-front reservation fails. That reservation is what keeps the serial
// worker from reallocating the array while the GUI thread is walking it, so a scan without it
// is not a slower scan, it is an unsafe one.
static bool scan_reset_census(WcApp *app) {
    wc_census_free(&app->scan.census);
    wc_scan_init(&app->scan, app->clock);
    // Reserve up front: a busy scan then never reallocs (the transient 2x copy is what
    // exhausted the heap and rebooted the Flipper mid-scan).
    return wc_census_reserve(&app->scan.census, WC_CENSUS_MAX_DEVICES);
}

// Save the current census as "<base>_<idx>" (used by auto-save). Serial must be stopped so the
// worker thread is not writing the census while we read it.
static void scan_save_chunk(WcApp *app) {
    char name[WC_TEXT_BUF_SIZE + 8];
    snprintf(name, sizeof(name), "%s_%u", app->autosave_base, app->autosave_idx);
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.label, name, WC_LABEL_MAX);
    meta.epoch = app->scan_started;
    meta.duration_s = wc_clock_now(&app->clock) - app->scan_started;
    meta.channels_mask = 0x3FFF;
    if (!wc_capture_service_save(&app->store, name, &meta, &app->scan.census)) {
        app->autosave_failed++; // a full or absent SD loses the chunk; the scan screen says so
    }
    app->autosave_idx++;
}

void wc_scene_scan_on_enter(void *context) {
    WcApp *app = context;
    // Back from the name screen re-enters this scene. Resetting here would throw away the very
    // capture the user was about to name, so a scan already in hand is resumed, not restarted.
    if (app->scan.census.count == 0) {
        if (!scan_reset_census(app)) {
            snprintf(app->result_text, WC_RESULT_TEXT_SIZE, k_no_room_text,
                     (unsigned)WC_CENSUS_MAX_DEVICES);
            scene_manager_next_scene(app->scene_manager, WcSceneMsg);
            return;
        }
        app->scan_started = wc_clock_now(&app->clock);
    }
    if (app->autosave && app->autosave_idx == 0) {
        wc_default_name(app->scan_started, "auto", app->autosave_base, sizeof(app->autosave_base));
        app->autosave_idx = 1;
        app->autosave_failed = 0;
    }
    app->scan_link_ok = scan_serial_start(app);
    if (!app->scan_link_ok) {
        // Nothing was captured and nothing is running: release the census we just reserved and
        // explain the problem instead of showing an empty scan that never counts anything.
        wc_census_free(&app->scan.census);
        app->autosave_idx = 0; // the next scan is a new series, not a continuation
        scan_show_link_error(app);
        return;
    }

    scan_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);

    app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->scan_timer, furi_ms_to_ticks(500));
}

bool wc_scene_scan_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventScanTick) {
        scan_render(app);
        // Auto-save rotation: near the ceiling, save this chunk and start a fresh census so a
        // huge venue spans several files instead of dropping devices. Stop serial first so the
        // worker thread is not touching the census while we save + reset it.
        if (app->autosave && app->scan.census.count >= WC_AUTOSAVE_ROTATE_AT) {
            scan_serial_stop(app);
            scan_save_chunk(app);
            if (!scan_reset_census(app)) {
                // Without the reservation the worker would grow the array under the GUI's feet.
                app->scan_link_ok = false;
                snprintf(app->result_text, WC_RESULT_TEXT_SIZE, k_no_room_text,
                         (unsigned)WC_CENSUS_MAX_DEVICES);
                scene_manager_next_scene(app->scene_manager, WcSceneMsg);
                return true;
            }
            if (!scan_serial_start(app)) {
                // The port went away mid-run: stop cleanly and say so rather than freezing.
                app->scan_link_ok = false;
                if (app->scan_timer) {
                    furi_timer_stop(app->scan_timer);
                    furi_timer_free(app->scan_timer);
                    app->scan_timer = NULL;
                }
                scan_show_link_error(app);
            }
        }
        return true;
    }
    if (event.type == SceneManagerEventTypeBack) {
        if (!app->scan_link_ok) {
            scene_manager_previous_scene(app->scene_manager); // never started: nothing to save
            return true;
        }
        if (app->autosave) {
            // Save the final partial chunk automatically and return to the menu.
            scan_serial_stop(app);
            if (app->scan.census.count > 0) {
                scan_save_chunk(app);
            }
            wc_census_free(&app->scan.census);
            app->autosave_idx = 0;
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        } else {
            // Stop and go to the save screen instead of dropping the scan.
            scene_manager_next_scene(app->scene_manager, WcSceneSave);
        }
        return true;
    }
    return false;
}

void wc_scene_scan_on_exit(void *context) {
    WcApp *app = context;
    if (app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    scan_serial_stop(app);
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

void wc_scene_save_on_enter(void *context) {
    WcApp *app = context;
    // Prefill a default name from the scan's start time so OK-without-typing saves at once.
    wc_default_capture_name(app->scan_started, app->text_buf, sizeof(app->text_buf));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Capture name");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_BASENAME_MAX, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_save_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        WcCaptureMeta meta;
        memset(&meta, 0, sizeof(meta));
        strncpy(meta.label, app->text_buf, WC_LABEL_MAX);
        meta.epoch = app->scan_started;
        meta.duration_s = wc_clock_now(&app->clock) - app->scan_started;
        meta.channels_mask = 0x3FFF; // channels 1-14 hopped
        if (wc_capture_service_exists(&app->store, app->text_buf)) {
            snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
                     "NOT saved.\n\n%s already exists,\nand saving would\nreplace it.\n\nThe "
                     "scan is still\nhere: press Back and\npick another name.",
                     app->text_buf);
            scene_manager_next_scene(app->scene_manager, WcSceneMsg);
            return true;
        }
        if (!wc_capture_service_save(&app->store, app->text_buf, &meta, &app->scan.census)) {
            // Keep the census: it is the whole scan, and the name screen is still behind us.
            snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
                     "NOT saved.\n\nThe SD refused the\nwrite - card full,\nabsent, or the name\n"
                     "is already taken.\n\nThe scan is still\nhere: press Back and\ntry another "
                     "name.");
            scene_manager_next_scene(app->scene_manager, WcSceneMsg);
            return true;
        }
        wc_census_free(&app->scan.census); // done with it; free until the next scan
        app->autosave_idx = 0;
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        return true;
    }
    return false;
}

void wc_scene_save_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

// ---------------------------------------------------------------------------
// Serial debug (show the raw lines Marauder sends, to learn its format)
// ---------------------------------------------------------------------------

// Runs in the serial worker thread. Appends the raw line to a rolling buffer; wraps to keep
// the most recent lines. The GUI timer renders it — a torn read is harmless here.
static void debug_on_line(void *ctx, const char *line, size_t len) {
    WcApp *app = ctx;
    size_t cur = strlen(app->debug_buf);
    if (cur + len + 2 >= sizeof(app->debug_buf)) {
        cur = 0; // wrap: drop older lines to keep the newest
    }
    size_t room = sizeof(app->debug_buf) - cur - 2;
    size_t n = len < room ? len : room;
    memcpy(app->debug_buf + cur, line, n);
    app->debug_buf[cur + n] = '\n';
    app->debug_buf[cur + n + 1] = '\0';
}

void wc_scene_serial_debug_on_enter(void *context) {
    WcApp *app = context;
    app->serial = wc_serial_furi_alloc(app->baud);
    bool ok = false;
    if (app->serial) {
        WcSerialPort port = wc_serial_furi_port(app->serial);
        ok = port.start(port.self, debug_on_line, app);
    }
    if (!ok) {
        wc_serial_furi_free(app->serial);
        app->serial = NULL;
        snprintf(app->debug_buf, sizeof(app->debug_buf), "%s", k_uart_busy_text);
    } else {
        snprintf(app->debug_buf, sizeof(app->debug_buf), "Waiting for serial...\n");
    }

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->debug_buf);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);

    if (ok) {
        app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);
        furi_timer_start(app->scan_timer, furi_ms_to_ticks(400));
    }
}

bool wc_scene_serial_debug_on_event(void *context, SceneManagerEvent event) {
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventScanTick) {
        WcApp *app = context;
        text_box_set_text(app->text_box, app->debug_buf);
        return true;
    }
    return false;
}

void wc_scene_serial_debug_on_exit(void *context) {
    WcApp *app = context;
    if (app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    scan_serial_stop(app);
    text_box_reset(app->text_box);
}
