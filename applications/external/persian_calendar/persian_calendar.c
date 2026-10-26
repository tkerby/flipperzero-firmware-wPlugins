#include <furi.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <stdio.h>

#include "jalali.h"

/* Shahanshahi (Imperial) year = Solar Hijri year + 1180 (epoch: Cyrus, 559 BC) */
#define SHAHANSHAHI_OFFSET 1180

#define SETTINGS_PATH    APP_DATA_PATH("settings.bin")
#define SETTINGS_MAGIC   0x50
#define SETTINGS_VERSION 1

typedef enum {
    ScreenHome,
    ScreenMenu,
    ScreenEdit,
    ScreenResult,
    ScreenSettings,
} Screen;

typedef enum {
    ModeSetDate, /* edit Gregorian, write to RTC */
    ModeG2P, /* edit Gregorian, show Persian */
    ModeP2G, /* edit Persian, show Gregorian */
    ModeCount,
} Mode;

typedef enum {
    MenuSetDate,
    MenuG2P,
    MenuP2G,
    MenuSettings,
    MenuCount,
} MenuItem;

typedef enum {
    CalSolarHijri,
    CalShahanshahi,
    CalVariantCount,
} CalVariant;

typedef struct {
    uint8_t variant;
} PersianCalSettings;

typedef enum {
    FieldDay,
    FieldMonth,
    FieldYear,
    FieldCount,
} Field;

typedef struct {
    Screen screen;
    int menu_index;
    Mode mode;
    Field field;
    CalVariant variant;
    int e_day, e_month, e_year;
    /* conversion result */
    int src_d, src_m, src_y;
    bool src_is_jalali;
    int dst_d, dst_m, dst_y;
    int weekday; /* 0=Sunday..6=Saturday */
    bool running;
    FuriMutex* mutex;
    ViewPort* view_port;
} App;

static const char* gregorian_month_names[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
};

static const char* jalali_month_names[12] = {
    "Farvardin",
    "Ordibehesht",
    "Khordad",
    "Tir",
    "Mordad",
    "Shahrivar",
    "Mehr",
    "Aban",
    "Azar",
    "Dey",
    "Bahman",
    "Esfand",
};

static const char* weekday_names_en[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static const char* weekday_names_fa[7] = {
    "Yekshanbeh",
    "Doshanbeh",
    "Seshanbeh",
    "Chaharshanbeh",
    "Panjshanbeh",
    "Jomeh",
    "Shanbeh",
};

static const char* menu_items[MenuCount] = {
    "Set today's date",
    "Gregorian > Persian",
    "Persian > Gregorian",
    "Settings",
};

static const char* variant_names[CalVariantCount] = {
    "Solar Hijri",
    "Shahanshahi",
};

/* Solar Hijri year -> year number shown to the user */
static int display_persian_year(App* app, int jy) {
    return (app->variant == CalShahanshahi) ? jy + SHAHANSHAHI_OFFSET : jy;
}

static void settings_load(App* app) {
    PersianCalSettings s = {.variant = CalSolarHijri};
    if(saved_struct_load(SETTINGS_PATH, &s, sizeof(s), SETTINGS_MAGIC, SETTINGS_VERSION) &&
       s.variant < CalVariantCount) {
        app->variant = (CalVariant)s.variant;
    }
}

static void settings_save(App* app) {
    PersianCalSettings s = {.variant = (uint8_t)app->variant};
    saved_struct_save(SETTINGS_PATH, &s, sizeof(s), SETTINGS_MAGIC, SETTINGS_VERSION);
}

static void draw_header(Canvas* canvas, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, title);
    canvas_set_color(canvas, ColorBlack);
}

/* FontPrimary if it fits, FontSecondary otherwise */
static void draw_fitted_str(Canvas* canvas, int y, const char* str) {
    canvas_set_font(canvas, FontPrimary);
    if(canvas_string_width(canvas, str) > 126) canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignCenter, str);
}

static void draw_home(Canvas* canvas, App* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    int jy, jm, jd;
    gregorian_to_jalali(dt.year, dt.month, dt.day, &jy, &jm, &jd);
    int wd = weekday_from_gregorian(dt.year, dt.month, dt.day);

    draw_header(canvas, "Persian Calendar");

    char buf[48];
    snprintf(
        buf,
        sizeof(buf),
        "%d %s %d",
        jd,
        jalali_month_names[jm - 1],
        display_persian_year(app, jy));
    draw_fitted_str(canvas, 24, buf);

    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%s (%s)", weekday_names_fa[wd], weekday_names_en[wd]);
    canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignCenter, buf);

    snprintf(
        buf, sizeof(buf), "%d %s %d", dt.day, gregorian_month_names[dt.month - 1], dt.year);
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, buf);

    canvas_draw_str_aligned(canvas, 1, 63, AlignLeft, AlignBottom, "Back: Exit");
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "OK: Menu");
}

static void draw_menu(Canvas* canvas, App* app) {
    draw_header(canvas, "Menu");
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < MenuCount; i++) {
        int box_y = 15 + i * 12;
        if(i == app->menu_index) {
            canvas_draw_box(canvas, 0, box_y, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 6, box_y + 6, AlignLeft, AlignCenter, menu_items[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_settings(Canvas* canvas, App* app) {
    draw_header(canvas, "Settings");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "Persian calendar:");

    canvas_set_font(canvas, FontPrimary);
    char buf[32];
    snprintf(buf, sizeof(buf), "< %s >", variant_names[app->variant]);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, buf);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    int jy, jm, jd;
    gregorian_to_jalali(dt.year, dt.month, dt.day, &jy, &jm, &jd);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Current year: %d", display_persian_year(app, jy));
    canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignCenter, buf);

    canvas_draw_str_aligned(canvas, 1, 63, AlignLeft, AlignBottom, "<>: change");
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "Back: Save");
}

static void draw_edit(Canvas* canvas, App* app) {
    static const char* titles[ModeCount] = {
        "Set today's date",
        "Gregorian date",
        "Persian date",
    };
    draw_header(canvas, titles[app->mode]);

    bool jalali = (app->mode == ModeP2G);

    char day_s[4], month_s[4], year_s[8];
    snprintf(day_s, sizeof(day_s), "%02d", app->e_day);
    snprintf(month_s, sizeof(month_s), "%02d", app->e_month);
    snprintf(year_s, sizeof(year_s), "%d", app->e_year);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 44, 29, AlignCenter, AlignCenter, "/");
    canvas_draw_str_aligned(canvas, 76, 29, AlignCenter, AlignCenter, "/");

    const char* strs[FieldCount] = {day_s, month_s, year_s};
    const int xs[FieldCount] = {28, 60, 98};
    for(int i = 0; i < FieldCount; i++) {
        canvas_draw_str_aligned(canvas, xs[i], 29, AlignCenter, AlignCenter, strs[i]);
        if(i == (int)app->field) {
            int w = canvas_string_width(canvas, strs[i]);
            canvas_draw_rframe(canvas, xs[i] - w / 2 - 3, 20, w + 7, 18, 2);
        }
    }

    canvas_set_font(canvas, FontSecondary);
    const char* month_name =
        jalali ? jalali_month_names[app->e_month - 1] : gregorian_month_names[app->e_month - 1];
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, month_name);

    canvas_draw_str_aligned(canvas, 1, 63, AlignLeft, AlignBottom, "^v value  <> field");
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "OK: Go");
}

static void draw_result(Canvas* canvas, App* app) {
    draw_header(canvas, "Conversion");

    char buf[48];
    canvas_set_font(canvas, FontSecondary);
    const char* src_month = app->src_is_jalali ? jalali_month_names[app->src_m - 1] :
                                                 gregorian_month_names[app->src_m - 1];
    snprintf(buf, sizeof(buf), "%d %s %d  =", app->src_d, src_month, app->src_y);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, buf);

    const char* dst_month = app->src_is_jalali ? gregorian_month_names[app->dst_m - 1] :
                                                 jalali_month_names[app->dst_m - 1];
    int dst_y = app->src_is_jalali ? app->dst_y : display_persian_year(app, app->dst_y);
    snprintf(buf, sizeof(buf), "%d %s %d", app->dst_d, dst_month, dst_y);
    draw_fitted_str(canvas, 33, buf);

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf,
        sizeof(buf),
        "%s (%s)",
        weekday_names_fa[app->weekday],
        weekday_names_en[app->weekday]);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, buf);

    canvas_draw_str_aligned(canvas, 1, 63, AlignLeft, AlignBottom, "Back: Menu");
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, "OK: Home");
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    switch(app->screen) {
    case ScreenHome:
        draw_home(canvas, app);
        break;
    case ScreenMenu:
        draw_menu(canvas, app);
        break;
    case ScreenEdit:
        draw_edit(canvas, app);
        break;
    case ScreenResult:
        draw_result(canvas, app);
        break;
    case ScreenSettings:
        draw_settings(canvas, app);
        break;
    }
    furi_mutex_release(app->mutex);
}

static void app_input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, 0);
}

static void timer_callback(void* ctx) {
    /* Redraw once a second so the home screen rolls over at midnight */
    view_port_update((ViewPort*)ctx);
}

/* Editor year is shown/edited in the selected variant; math wants Solar Hijri */
static int edit_internal_year(App* app) {
    if(app->mode == ModeP2G && app->variant == CalShahanshahi) {
        return app->e_year - SHAHANSHAHI_OFFSET;
    }
    return app->e_year;
}

static int edit_max_day(App* app) {
    return (app->mode == ModeP2G) ? jalali_month_days(edit_internal_year(app), app->e_month) :
                                    gregorian_month_days(app->e_year, app->e_month);
}

static void edit_init(App* app, Mode mode) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    app->mode = mode;
    app->field = FieldDay;
    if(mode == ModeP2G) {
        gregorian_to_jalali(dt.year, dt.month, dt.day, &app->e_year, &app->e_month, &app->e_day);
        app->e_year = display_persian_year(app, app->e_year);
    } else {
        app->e_year = dt.year;
        app->e_month = dt.month;
        app->e_day = dt.day;
    }
    app->screen = ScreenEdit;
}

static void edit_change(App* app, int delta) {
    if(app->field == FieldDay) {
        int md = edit_max_day(app);
        app->e_day += delta;
        if(app->e_day < 1) app->e_day = md;
        if(app->e_day > md) app->e_day = 1;
    } else if(app->field == FieldMonth) {
        app->e_month += delta;
        if(app->e_month < 1) app->e_month = 12;
        if(app->e_month > 12) app->e_month = 1;
    } else {
        bool jalali = (app->mode == ModeP2G);
        int ymin = jalali ? 1179 : 1800;
        int ymax = jalali ? 1578 : 2200;
        if(jalali && app->variant == CalShahanshahi) {
            ymin += SHAHANSHAHI_OFFSET;
            ymax += SHAHANSHAHI_OFFSET;
        }
        app->e_year += delta;
        if(app->e_year < ymin) app->e_year = ymin;
        if(app->e_year > ymax) app->e_year = ymax;
    }
    int md = edit_max_day(app);
    if(app->e_day > md) app->e_day = md;
}

static void edit_confirm(App* app) {
    if(app->mode == ModeSetDate) {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        dt.day = app->e_day;
        dt.month = app->e_month;
        dt.year = app->e_year;
        int wd = weekday_from_gregorian(app->e_year, app->e_month, app->e_day);
        dt.weekday = (wd == 0) ? 7 : wd; /* RTC weekday: 1=Mon..7=Sun */
        furi_hal_rtc_set_datetime(&dt);
        app->screen = ScreenHome;
    } else if(app->mode == ModeG2P) {
        app->src_d = app->e_day;
        app->src_m = app->e_month;
        app->src_y = app->e_year;
        app->src_is_jalali = false;
        gregorian_to_jalali(
            app->e_year, app->e_month, app->e_day, &app->dst_y, &app->dst_m, &app->dst_d);
        app->weekday = weekday_from_gregorian(app->e_year, app->e_month, app->e_day);
        app->screen = ScreenResult;
    } else {
        app->src_d = app->e_day;
        app->src_m = app->e_month;
        app->src_y = app->e_year; /* display units: matches what the user typed */
        app->src_is_jalali = true;
        jalali_to_gregorian(
            edit_internal_year(app),
            app->e_month,
            app->e_day,
            &app->dst_y,
            &app->dst_m,
            &app->dst_d);
        app->weekday = weekday_from_gregorian(app->dst_y, app->dst_m, app->dst_d);
        app->screen = ScreenResult;
    }
}

static void handle_input(App* app, InputEvent* event) {
    bool press = (event->type == InputTypeShort);
    bool nav = (event->type == InputTypeShort || event->type == InputTypeRepeat);

    switch(app->screen) {
    case ScreenHome:
        if(press && event->key == InputKeyOk) {
            app->screen = ScreenMenu;
        } else if(press && event->key == InputKeyBack) {
            app->running = false;
        }
        break;
    case ScreenMenu:
        if(nav && event->key == InputKeyUp) {
            app->menu_index = (app->menu_index + MenuCount - 1) % MenuCount;
        } else if(nav && event->key == InputKeyDown) {
            app->menu_index = (app->menu_index + 1) % MenuCount;
        } else if(press && event->key == InputKeyOk) {
            if(app->menu_index == MenuSettings) {
                app->screen = ScreenSettings;
            } else {
                edit_init(app, (Mode)app->menu_index);
            }
        } else if(press && event->key == InputKeyBack) {
            app->screen = ScreenHome;
        }
        break;
    case ScreenEdit:
        if(nav && event->key == InputKeyUp) {
            edit_change(app, +1);
        } else if(nav && event->key == InputKeyDown) {
            edit_change(app, -1);
        } else if(nav && event->key == InputKeyLeft) {
            app->field = (app->field + FieldCount - 1) % FieldCount;
        } else if(nav && event->key == InputKeyRight) {
            app->field = (app->field + 1) % FieldCount;
        } else if(press && event->key == InputKeyOk) {
            edit_confirm(app);
        } else if(press && event->key == InputKeyBack) {
            app->screen = ScreenMenu;
        }
        break;
    case ScreenResult:
        if(press && event->key == InputKeyOk) {
            app->screen = ScreenHome;
        } else if(press && event->key == InputKeyBack) {
            app->screen = ScreenMenu;
        }
        break;
    case ScreenSettings:
        if(press &&
           (event->key == InputKeyLeft || event->key == InputKeyRight ||
            event->key == InputKeyOk)) {
            app->variant = (app->variant + 1) % CalVariantCount;
        } else if(press && event->key == InputKeyBack) {
            settings_save(app);
            app->screen = ScreenMenu;
        }
        break;
    }
}

int32_t persian_calendar_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    app->screen = ScreenHome;
    app->menu_index = 0;
    app->variant = CalSolarHijri;
    app->running = true;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    settings_load(app);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    view_port_input_callback_set(app->view_port, app_input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);

    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app->view_port);
    furi_timer_start(timer, furi_kernel_get_tick_frequency());

    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            handle_input(app, &event);
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        }
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    gui_remove_view_port(gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
