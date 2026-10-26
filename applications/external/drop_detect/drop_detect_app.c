#include "drop_detect_app.h"
#include "views/drop_view.h"
#include "views/idle_view.h"
#include "views/walking_view.h"
#include "views/fidget_view.h"

#include <stdlib.h>
#include <string.h>

#include "driver/ICM42688P.h"
#include "datarecord.h"

#define TAG "drop_detect"

#define ANIM_TICK_MS 33u

// Custom event IDs — menu indices use 0-3, timer events start at 100
#define DROP_DETECT_EVENT_RETURN_TO_SUBMENU 100u

static ICM42688P* imu = NULL;
static FuriHalSpiBusHandle* imu_spi_handle = NULL;

// External function
ICM42688P* get_imu(void) {
    return imu;
}

/* =========================
 * FORWARD DECLARATIONS
 * ========================= */
static DropDetectApp* drop_detect_app_alloc(void);
static void drop_detect_app_free(DropDetectApp* app);

static void drop_detect_anim_tick(void* context);

static void drop_detect_submenu_callback(void* context, uint32_t index);
static bool drop_detect_navigation_event_callback(void* context);
static bool drop_detect_custom_event_callback(void* context, uint32_t event);

/* =========================
 * ENTRY POINT
 * ========================= */
int32_t drop_detect_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "=== APP START ===");
    FURI_LOG_I(TAG, "get_imu=%p", (void*)get_imu());

    DropDetectApp* app = drop_detect_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "alloc failed, exiting");
        return -1;
    }

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        DropDetectViewSubmenu);

    // Initialize IMU with 100Hz sample rate for both accel and gyro (if available)
    if(imu) {
        FURI_LOG_I(TAG, "configuring IMU...");
        icm42688p_accel_config(imu, AccelFullScale16G, DataRate100Hz);
        icm42688p_gyro_config(imu, GyroFullScale2000DPS, DataRate100Hz);
        FURI_LOG_I(TAG, "IMU configured");
    } else {
        FURI_LOG_E(TAG, "IMU is NULL after alloc!");
    }
    
    view_dispatcher_run(app->view_dispatcher);
    FURI_LOG_I(TAG, "dispatcher stopped");
    
    drop_detect_app_free(app);
    FURI_LOG_I(TAG, "=== APP END ===");

    return 0;
}

/* =========================
 * MENU CALLBACK
 * ========================= */
// Forward declaration
static void drop_detect_start_capture(DropDetectApp* app);

void drop_detect_app_request_view(DropDetectApp* app, DropDetectView view) {
    FURI_LOG_I(TAG, "request_view: from=%d to=%d", app ? app->active_view : -1, view);

    if(!app) return;

    if(view == DropDetectViewSubmenu && app->active_view != DropDetectViewSubmenu) {
        capture_cancel();
    }

    // Reset capture + per-view timer state so each menu entry starts fresh.
    app->capture_started = false;
    app->exit_delay_frames = 0;
    app->frame = 0;

    // Set the active view BEFORE switching to ensure proper callback handling
    DropDetectView old_view = app->active_view;
    app->active_view = view;

    // Reset the per-view model + restart icon animations so visuals begin
    // from frame 0 on every entry.
    switch(view) {
    case DropDetectViewDrop:    drop_view_reset(app->drop_view);       break;
    case DropDetectViewIdle:    idle_view_reset(app->idle_view);       break;
    case DropDetectViewWalking: walking_view_reset(app->walking_view); break;
    case DropDetectViewFidget:  fidget_view_reset(app->fidget_view);   break;
    default: break;
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, view);

    // Start capture for data recording views
    if(old_view == DropDetectViewSubmenu && view != DropDetectViewSubmenu) {
        drop_detect_start_capture(app);
    }
}

static void drop_detect_start_capture(DropDetectApp* app) {
    FURI_LOG_I(TAG, "START CAPTURE ENTER");
    
    if(!app) {
        FURI_LOG_E(TAG, "NULL app");
        return;
    }
    
    FURI_LOG_I(TAG, "app=%p", (void*)app);
    
    // Use get_imu to ensure we have valid IMU
    ICM42688P* local_imu = get_imu();
    if(!local_imu) {
        FURI_LOG_E(TAG, "IMU is NULL from get_imu()!");
        app->capturing = false;
        return;
    }
    
    FURI_LOG_I(TAG, "IMU=%p", (void*)local_imu);

    // Only capture for data recording views (not submenu)
    if(app->active_view == DropDetectViewSubmenu) {
        FURI_LOG_I(TAG, "submenu - no capture");
        app->capturing = false;
        app->capture_started = true;
        return;
    }
    
    // Drop: fixed 300-sample clip. All other views: run until back is pressed.
    uint32_t num_samples = (app->active_view == DropDetectViewDrop) ? 300 : UINT32_MAX;
    FURI_LOG_I(TAG, "starting capture: %lu samples", num_samples);

    app->total_samples_to_capture = num_samples;
    app->samples_captured = 0;
    app->capturing = true;
    app->capture_started = true;

    const char* label = "unknown";
    switch(app->active_view) {
    case DropDetectViewDrop:    label = "drop";    break;
    case DropDetectViewIdle:    label = "idle";    break;
    case DropDetectViewWalking: label = "walking"; break;
    case DropDetectViewFidget:  label = "fidget";  break;
    default: break;
    }

    FURI_LOG_I(TAG, "calling capture() with local_imu=%p label=%s", (void*)local_imu, label);
    capture(local_imu, num_samples, label);
    FURI_LOG_I(TAG, "capture called");
}

static void drop_detect_submenu_callback(void* context, uint32_t index) {
    FURI_LOG_I(TAG, "submenu callback: index=%lu", index);

    DropDetectApp* app = context;
    if(!app) {
        FURI_LOG_E(TAG, "NULL app in submenu callback");
        return;
    }

    // Send a custom event instead of switching views directly — calling
    // view_dispatcher_switch_to_view from inside an input callback causes a
    // ViewPort lockup because the GUI lock is already held at that point.
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/* =========================
 * EVENTS
 * ========================= */
static bool drop_detect_navigation_event_callback(void* context) {
    DropDetectApp* app = context;
    if(!app) return false;

    if(app->active_view == DropDetectViewSubmenu) {
        return false; // exit the app
    }

    if(app->active_view != DropDetectViewDrop) {
        drop_detect_app_request_view(app, DropDetectViewSubmenu);
        return true;
    }

    return false; // drop view: let timer handle exit
}

static bool drop_detect_custom_event_callback(void* context, uint32_t event) {
    FURI_LOG_I(TAG, "custom event: %lu", event);

    DropDetectApp* app = context;
    if(!app) return false;

    if(event == DROP_DETECT_EVENT_RETURN_TO_SUBMENU) {
        drop_detect_app_request_view(app, DropDetectViewSubmenu);
        return true;
    }

    DropDetectView target;
    switch(event) {
    case DropDetectMenuDrop:
        target = DropDetectViewDrop;
        break;
    case DropDetectMenuIdle:
        target = DropDetectViewIdle;
        break;
    case DropDetectMenuWalking:
        target = DropDetectViewWalking;
        break;
    case DropDetectMenuFidget:
        target = DropDetectViewFidget;
        break;
    default:
        return false;
    }

    drop_detect_app_request_view(app, target);
    return true;
}

/* =========================
 * ANIMATION TICK
 * ========================= */
static void drop_detect_anim_tick(void* context) {
    DropDetectApp* app = context;
    if(!app) return;

    // Skip work entirely while sitting on the submenu — it handles its own redraw
    // and burning cycles here just delays input handling.
    if(app->active_view == DropDetectViewSubmenu) {
        return;
    }

    app->frame++;

    switch(app->active_view) {
        case DropDetectViewDrop:
            drop_view_tick(app->drop_view, app->frame);
            break;
        case DropDetectViewIdle:
            idle_view_tick(app->idle_view, app->frame);
            break;
        case DropDetectViewWalking:
            walking_view_tick(app->walking_view, app->frame);
            break;
        case DropDetectViewFidget:
            fidget_view_tick(app->fidget_view, app->frame);
            break;
        default:
            break;
    }

    if(app->capturing && app->capture_started && capture_is_done()) {
        app->capturing = false;
        app->capture_started = false;

        // Drop view: stay for 3 seconds then auto-exit
        // Other views: stay until back button is pressed
        if(app->active_view == DropDetectViewDrop) {
            app->exit_delay_frames = 91;
        }
    }

    // Count down the post-capture hold for drop view
    if(app->active_view == DropDetectViewDrop && app->exit_delay_frames > 0) {
        app->exit_delay_frames--;
        if(app->exit_delay_frames == 0) {
            view_dispatcher_send_custom_event(app->view_dispatcher, DROP_DETECT_EVENT_RETURN_TO_SUBMENU);
        }
    }
}

/* =========================
 * ALLOC
 * ========================= */
static DropDetectApp* drop_detect_app_alloc(void) {
    FURI_LOG_I(TAG, "alloc start");
    
    // Comment out IMU to test if that's the issue
     FURI_LOG_I(TAG, "calling icm42688p_alloc...");
     imu_spi_handle = malloc(sizeof(FuriHalSpiBusHandle));
     memcpy(imu_spi_handle, &furi_hal_spi_bus_handle_external, sizeof(FuriHalSpiBusHandle));
     imu_spi_handle->cs = &gpio_ext_pc3;
     imu = icm42688p_alloc(imu_spi_handle, &gpio_ext_pb2);
     FURI_LOG_I(TAG, "icm42688p_alloc returned %p", (void*)imu);
     if(!imu) {
        FURI_LOG_E(TAG, "IMU alloc failed");
         return NULL;
     }
    
     FURI_LOG_I(TAG, "calling icm42688p_init...");
     if(!icm42688p_init(imu)) {
         FURI_LOG_E(TAG, "IMU init failed");
         icm42688p_free(imu);
         imu = NULL;
         return NULL;
     }
     FURI_LOG_I(TAG, "IMU init success");

    FURI_LOG_I(TAG, "IMU done - global imu=%p", (void*)imu);
    DropDetectApp* app = malloc(sizeof(DropDetectApp));
    if(!app) {
        FURI_LOG_E(TAG, "malloc app failed");
        return NULL;
    }
    FURI_LOG_I(TAG, "malloc done");

    memset(app, 0, sizeof(DropDetectApp));

    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        FURI_LOG_E(TAG, "open GUI failed");
        free(app);
        return NULL;
    }
    
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    if(!app->notif) {
        FURI_LOG_E(TAG, "open notification failed");
        furi_record_close(RECORD_GUI);
        free(app);
        return NULL;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E(TAG, "alloc dispatcher failed");
        free(app);
        return NULL;
    }
    FURI_LOG_I(TAG, "dispatcher done");

    view_dispatcher_attach_to_gui(
        app->view_dispatcher,
        app->gui,
        ViewDispatcherTypeFullscreen);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher,
        drop_detect_navigation_event_callback);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher,
        drop_detect_custom_event_callback);

    /* submenu */
    FURI_LOG_I(TAG, "alloc submenu");
    app->submenu = submenu_alloc();
    if(!app->submenu) {
        FURI_LOG_E(TAG, "alloc submenu failed");
        free(app);
        return NULL;
    }
    FURI_LOG_I(TAG, "submenu done");

    submenu_set_header(app->submenu, "FliPort");

    submenu_add_item(app->submenu, "Drop",    DropDetectMenuDrop,    drop_detect_submenu_callback, app);
    submenu_add_item(app->submenu, "Idle",    DropDetectMenuIdle,    drop_detect_submenu_callback, app);
    submenu_add_item(app->submenu, "Walking", DropDetectMenuWalking, drop_detect_submenu_callback, app);
    submenu_add_item(app->submenu, "Fidget",  DropDetectMenuFidget,  drop_detect_submenu_callback, app);

    view_dispatcher_add_view(
        app->view_dispatcher,
        DropDetectViewSubmenu,
        submenu_get_view(app->submenu));
    FURI_LOG_I(TAG, "submenu view added");

    /* views */
    FURI_LOG_I(TAG, "alloc drop view");
    app->drop_view = drop_view_alloc();
    if(!app->drop_view) {
        FURI_LOG_E(TAG, "drop view alloc failed");
        free(app);
        return NULL;
    }
    drop_view_set_notification(app->notif);
    view_dispatcher_add_view(app->view_dispatcher, DropDetectViewDrop, app->drop_view);

    FURI_LOG_I(TAG, "alloc idle view");
    app->idle_view = idle_view_alloc();
    if(!app->idle_view) {
        FURI_LOG_E(TAG, "idle view alloc failed");
        free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, DropDetectViewIdle, app->idle_view);

    FURI_LOG_I(TAG, "alloc walking view");
    app->walking_view = walking_view_alloc();
    if(!app->walking_view) {
        FURI_LOG_E(TAG, "walking view alloc failed");
        free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, DropDetectViewWalking, app->walking_view);

    FURI_LOG_I(TAG, "alloc fidget view");
    app->fidget_view = fidget_view_alloc();
    if(!app->fidget_view) {
        FURI_LOG_E(TAG, "fidget view alloc failed");
        free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, DropDetectViewFidget, app->fidget_view);

    /* timer */
    FURI_LOG_I(TAG, "alloc timer");
    app->anim_timer = furi_timer_alloc(
        drop_detect_anim_tick,
        FuriTimerTypePeriodic,
        app);

    if(!app->anim_timer) {
        FURI_LOG_E(TAG, "timer alloc failed");
        free(app);
        return NULL;
    }
    
    if(app->anim_timer) {
        furi_timer_start(app->anim_timer, furi_ms_to_ticks(ANIM_TICK_MS));
    }
    FURI_LOG_I(TAG, "timer done");

    app->active_view = DropDetectViewSubmenu;

    FURI_LOG_I(TAG, "alloc complete");

    return app;
}

/* =========================
 * FREE
 * ========================= */
static void drop_detect_app_free(DropDetectApp* app) {
    FURI_LOG_I(TAG, "free start - global imu=%p", (void*)imu);
    
    if(!app) {
        FURI_LOG_E(TAG, "NULL app in free");
        return;
    }

    if(app->anim_timer) {
        furi_timer_stop(app->anim_timer);
        furi_timer_free(app->anim_timer);
    }

    // Cancel and join any running capture thread before touching the IMU
    capture_cancel();
    capture_join();

    view_dispatcher_remove_view(app->view_dispatcher, DropDetectViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, DropDetectViewDrop);
    view_dispatcher_remove_view(app->view_dispatcher, DropDetectViewIdle);
    view_dispatcher_remove_view(app->view_dispatcher, DropDetectViewWalking);
    view_dispatcher_remove_view(app->view_dispatcher, DropDetectViewFidget);

    submenu_free(app->submenu);

    drop_view_free(app->drop_view);
    idle_view_free(app->idle_view);
    walking_view_free(app->walking_view);
    fidget_view_free(app->fidget_view);

     view_dispatcher_free(app->view_dispatcher);
     
     FURI_LOG_I(TAG, "freeing IMU...");
     if(imu) {
         icm42688p_deinit(imu);
         icm42688p_free(imu);
         imu = NULL;
     }
     if(imu_spi_handle) {
         free(imu_spi_handle);
         imu_spi_handle = NULL;
     }
     furi_record_close(RECORD_NOTIFICATION);
     furi_record_close(RECORD_GUI);

     free(app);
     FURI_LOG_I(TAG, "free complete");
}