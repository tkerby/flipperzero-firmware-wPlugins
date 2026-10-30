#include "tt_main_view.h"

#include <furi.h>
#include <gui/elements.h>
#include "talk_timer_icons.h"

/* So lange bleibt der Hinweis "Zurück halten" nach einem kurzen Druck stehen */
#define TT_EXIT_HINT_MS 2000

/* Fortschrittsbalken */
#define TT_BAR_X 4
#define TT_BAR_Y 32
#define TT_BAR_W 120
#define TT_BAR_H 7

struct TtMainView {
    View* view;
    TtMainViewCallback callback;
    void* context;
};

/* Modell der View: alles, was zum Zeichnen gebraucht wird */
typedef struct {
    TtSettings settings;
    TtTimerState state;
    uint32_t total_seconds;
    uint32_t elapsed_seconds;
    bool connected;
    bool show_elapsed; /* große Zahl zeigt abgelaufene statt verbleibende Zeit */
    uint32_t exit_hint_until; /* Tick, bis zu dem der Verlassen-Hinweis gezeigt wird */
} TtMainViewModel;

/* ---- Text mit echten Umlauten -------------------------------------------- */

/*
 * Die Firmware-Schriften enthalten nur ASCII (u8g2 "_tr"-Fonts), ein UTF-8-"ü"
 * würde einfach fehlen. Deshalb hier: Grundbuchstabe zeichnen und die beiden
 * Punkte selbst setzen. Unterstützt ä ö ü Ä Ö Ü; ß wird als "ss" gezeichnet.
 */
typedef struct {
    uint8_t cap_height; /* Höhe der Großbuchstaben */
    uint8_t x_height; /* Höhe der Kleinbuchstaben ohne Oberlänge */
    uint8_t dot_width; /* Breite eines Umlautpunkts (fette Schrift: 2) */
} TtFontMetrics;

static const TtFontMetrics tt_metrics_primary = {.cap_height = 8, .x_height = 6, .dot_width = 2};
static const TtFontMetrics tt_metrics_secondary = {.cap_height = 7, .x_height = 5, .dot_width = 1};

/* Liest ein Zeichen; liefert den Grundbuchstaben und ob es ein Umlaut ist */
static const char* tt_utf8_next(const char* str, char* base, bool* umlaut, bool* sharp_s) {
    *umlaut = false;
    *sharp_s = false;
    unsigned char c = (unsigned char)str[0];
    if(c == 0xC3 && str[1] != '\0') {
        unsigned char d = (unsigned char)str[1];
        switch(d) {
        case 0xA4:
            *base = 'a';
            *umlaut = true;
            return str + 2;
        case 0xB6:
            *base = 'o';
            *umlaut = true;
            return str + 2;
        case 0xBC:
            *base = 'u';
            *umlaut = true;
            return str + 2;
        case 0x84:
            *base = 'A';
            *umlaut = true;
            return str + 2;
        case 0x96:
            *base = 'O';
            *umlaut = true;
            return str + 2;
        case 0x9C:
            *base = 'U';
            *umlaut = true;
            return str + 2;
        case 0x9F:
            *base = 's';
            *sharp_s = true;
            return str + 2;
        default:
            *base = '?';
            return str + 2;
        }
    }
    *base = (char)c;
    return str + 1;
}

static uint16_t tt_utf8_width(Canvas* canvas, const char* str) {
    uint16_t width = 0;
    char base;
    bool umlaut, sharp_s;
    while(*str) {
        str = tt_utf8_next(str, &base, &umlaut, &sharp_s);
        width += canvas_glyph_width(canvas, base) * (sharp_s ? 2 : 1);
    }
    return width;
}

/* y ist die Grundlinie; horizontal ausgerichtet nach align */
static void tt_draw_utf8(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align align,
    const char* str,
    const TtFontMetrics* metrics) {
    if(align == AlignCenter) {
        x -= tt_utf8_width(canvas, str) / 2;
    } else if(align == AlignRight) {
        x -= tt_utf8_width(canvas, str);
    }

    char base;
    bool umlaut, sharp_s;
    while(*str) {
        str = tt_utf8_next(str, &base, &umlaut, &sharp_s);
        uint8_t glyph_width = canvas_glyph_width(canvas, base);
        canvas_draw_glyph(canvas, x, y, base);
        if(sharp_s) {
            x += glyph_width;
            canvas_draw_glyph(canvas, x, y, base);
        }
        if(umlaut) {
            bool upper = (base >= 'A' && base <= 'Z');
            uint8_t height = upper ? metrics->cap_height : metrics->x_height;
            /* Eine Zeile Abstand über dem Buchstaben, Punkte über den beiden Stämmen */
            int32_t dot_y = y - height - 2;
            uint8_t ink = glyph_width - 1; /* rechts steht 1 px Zeichenabstand */
            int32_t left = x;
            int32_t right = x + ink - metrics->dot_width;
            if(upper) {
                /* Großbuchstaben sind breit: Punkte etwas nach innen rücken */
                left += 1;
                right -= 1;
            }
            canvas_draw_box(canvas, left, dot_y, metrics->dot_width, 1);
            canvas_draw_box(canvas, right, dot_y, metrics->dot_width, 1);
        }
        x += glyph_width;
    }
}

/* ---- Zeichnen ------------------------------------------------------------ */

static void tt_format_time(uint32_t seconds, const char* prefix, char* buf, size_t buf_size) {
    snprintf(
        buf,
        buf_size,
        "%s%02lu:%02lu",
        prefix,
        (unsigned long)(seconds / 60),
        (unsigned long)(seconds % 60));
}

/* Kleine Markierung unter dem Balken an der Stelle, an der eine Warnung kommt */
static void tt_draw_warn_mark(Canvas* canvas, uint32_t warn_seconds, uint32_t total_seconds) {
    if(warn_seconds == 0 || warn_seconds >= total_seconds) return;
    uint32_t x = TT_BAR_X + 1 + ((total_seconds - warn_seconds) * (TT_BAR_W - 2)) / total_seconds;
    canvas_draw_line(canvas, x, TT_BAR_Y + TT_BAR_H, x, TT_BAR_Y + TT_BAR_H + 1);
}

static void tt_main_view_draw(Canvas* canvas, void* model_raw) {
    TtMainViewModel* model = model_raw;
    canvas_clear(canvas);

    bool overtime = model->elapsed_seconds >= model->total_seconds && model->total_seconds > 0 &&
                    model->state != TtTimerIdle;
    /* Blinktakt 2 Hz aus der Systemzeit, damit kein eigener Zähler nötig ist */
    bool blink_on = ((furi_get_tick() / furi_ms_to_ticks(500)) % 2) == 0;

    /* Kopfzeile: invertierter Balken mit App-Name, Profil und Verbindungs-Icon */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Talk Timer");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        112,
        10,
        AlignRight,
        AlignBottom,
        tt_profile_get(model->settings.profile)->short_name);
    /* Verbindungs-Icon rechts; ohne Verbindung durchgestrichen */
    if(model->settings.conn == TtConnUsb) {
        canvas_draw_icon(canvas, 118, 1, &I_usb_7x11);
    } else {
        canvas_draw_icon(canvas, 119, 2, &I_bt_5x9);
    }
    if(!model->connected) {
        canvas_draw_line(canvas, 115, 11, 126, 0);
        canvas_draw_line(canvas, 116, 11, 127, 0);
    }
    canvas_set_color(canvas, ColorBlack);

    /* Große Zeit: Restzeit (Standard) oder abgelaufene Zeit; überzogen mit "+" und invertiert */
    char big[12];
    if(overtime) {
        tt_format_time(model->elapsed_seconds - model->total_seconds, "+", big, sizeof(big));
    } else if(model->show_elapsed) {
        tt_format_time(model->elapsed_seconds, "", big, sizeof(big));
    } else {
        tt_format_time(model->total_seconds - model->elapsed_seconds, "", big, sizeof(big));
    }
    if(overtime) {
        canvas_draw_box(canvas, 0, 13, 128, 18);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, big);
    canvas_set_color(canvas, ColorBlack);

    /* Fortschrittsbalken mit Markierungen für die beiden Warnungen */
    canvas_draw_frame(canvas, TT_BAR_X, TT_BAR_Y, TT_BAR_W, TT_BAR_H);
    if(model->total_seconds > 0) {
        uint32_t done = overtime ? model->total_seconds : model->elapsed_seconds;
        uint32_t fill = (done * (TT_BAR_W - 2)) / model->total_seconds;
        if(fill > 0) canvas_draw_box(canvas, TT_BAR_X + 1, TT_BAR_Y + 1, fill, TT_BAR_H - 2);
        tt_draw_warn_mark(canvas, model->settings.warn1_seconds, model->total_seconds);
        tt_draw_warn_mark(canvas, model->settings.warn2_seconds, model->total_seconds);
    }

    /* Statuszeile (oder kurzzeitig der Hinweis zum Verlassen) */
    if(furi_get_tick() < model->exit_hint_until) {
        canvas_set_font(canvas, FontSecondary);
        tt_draw_utf8(
            canvas,
            64,
            51,
            AlignCenter,
            tt_tr("Hold Back to exit", "Zurück halten = beenden"),
            &tt_metrics_secondary);
    } else {
        const char* status;
        if(overtime) {
            status = tt_tr("OVERTIME", "ÜBERZOGEN");
        } else if(model->state == TtTimerRunning) {
            status = tt_tr("RUNNING", "LÄUFT");
        } else if(model->state == TtTimerPaused) {
            status = tt_tr("PAUSED", "PAUSE");
        } else {
            status = NULL; /* bereit: statt Status eine Bedienhilfe zeigen */
        }

        if(status == NULL) {
            canvas_set_font(canvas, FontSecondary);
            bool has_start = tt_profile_key(model->settings.profile, TtActionStart) != 0;
            if(model->settings.autostart && has_start) {
                canvas_draw_str(
                    canvas, 4, 51, tt_tr("OK: start timer + slides", "OK: Timer + Folien starten"));
            } else {
                /* Links was OK tut, rechts der Weg zur Präsentation (Hoch lang) */
                canvas_draw_str(canvas, 4, 51, "OK: Timer");
                if(has_start) {
                    const char* hint = tt_tr("hold: slides", "lang: Folien");
                    uint16_t width = canvas_string_width(canvas, hint);
                    canvas_draw_str(canvas, 124 - width, 51, hint);
                    canvas_draw_icon(canvas, 124 - width - 10, 45, &I_arrow_up_7x4);
                }
            }
        } else {
            canvas_set_font(canvas, FontPrimary);
            /* Überzogen blinkt, solange der Timer läuft */
            if(!(overtime && model->state == TtTimerRunning && !blink_on)) {
                tt_draw_utf8(canvas, 4, 51, AlignLeft, status, &tt_metrics_primary);
            }

            /* Rechts die jeweils andere Angabe */
            char side[24];
            char value[12];
            if(overtime || model->show_elapsed) {
                /* große Zahl = Überziehung bzw. abgelaufen -> rechts die Redezeit */
                tt_format_time(model->total_seconds, "", value, sizeof(value));
                snprintf(side, sizeof(side), "%s %s", tt_tr("of", "von"), value);
            } else {
                /* große Zahl = Restzeit -> rechts die abgelaufene Zeit */
                tt_format_time(model->elapsed_seconds, "", value, sizeof(value));
                snprintf(side, sizeof(side), "%s %s", tt_tr("used", "bisher"), value);
            }
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 124, 51, AlignRight, AlignBottom, side);
        }
    }

    /* Tastenleiste unten im Flipper-Stil (Firmware-Schrift: ohne Umlaute) */
    elements_button_left(canvas, tt_tr("Slide", "Folie"));
    elements_button_right(canvas, tt_tr("Slide", "Folie"));
    /* "Start" auch zum Fortsetzen: "Weiter" wäre mit "nächste Folie" verwechselbar */
    elements_button_center(canvas, (model->state == TtTimerRunning) ? "Pause" : "Start");
}

/* ---- Eingabe ------------------------------------------------------------- */

static bool tt_main_view_input(InputEvent* event, void* context) {
    TtMainView* main_view = context;
    furi_assert(main_view);

    /*
     * Schutz vor Fehlbedienung: Zurück kurz wird verbraucht und zeigt nur einen
     * Hinweis; erst Zurück lang meldet den Wunsch zu verlassen an die App.
     */
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            with_view_model(
                main_view->view,
                TtMainViewModel * model,
                { model->exit_hint_until = furi_get_tick() + furi_ms_to_ticks(TT_EXIT_HINT_MS); },
                true);
        } else if(event->type == InputTypeLong && main_view->callback) {
            main_view->callback(TtMainEventExit, main_view->context);
        }
        return true;
    }
    if(!main_view->callback) return true;

    /*
     * Folientasten reagieren sofort beim Drücken und lösen genau EINEN
     * Tastendruck aus. Gedrückt halten wird bewusst nicht durchgereicht, sonst
     * würde die Tastenwiederholung des Rechners mehrere Folien überspringen.
     */
    if(event->key == InputKeyRight || event->key == InputKeyLeft) {
        if(event->type == InputTypePress) {
            main_view->callback(
                (event->key == InputKeyRight) ? TtMainEventNext : TtMainEventPrev,
                main_view->context);
        }
        return true;
    }

    bool is_short = (event->type == InputTypeShort);
    bool is_long = (event->type == InputTypeLong);
    if(!is_short && !is_long) return true;

    switch(event->key) {
    case InputKeyOk:
        main_view->callback(
            is_short ? TtMainEventStartPause : TtMainEventReset, main_view->context);
        break;
    case InputKeyUp:
        main_view->callback(
            is_short ? TtMainEventBlack : TtMainEventStartShow, main_view->context);
        break;
    case InputKeyDown:
        if(is_short) {
            /* Reine Anzeigeumschaltung: Restzeit <-> abgelaufene Zeit */
            with_view_model(
                main_view->view,
                TtMainViewModel * model,
                { model->show_elapsed = !model->show_elapsed; },
                true);
        } else {
            main_view->callback(TtMainEventEndShow, main_view->context);
        }
        break;
    default:
        break;
    }
    return true;
}

/* ---- Auf- und Abbau ------------------------------------------------------ */

TtMainView* tt_main_view_alloc(void) {
    TtMainView* main_view = malloc(sizeof(TtMainView));
    main_view->callback = NULL;
    main_view->context = NULL;
    main_view->view = view_alloc();
    view_set_context(main_view->view, main_view);
    view_allocate_model(main_view->view, ViewModelTypeLocking, sizeof(TtMainViewModel));
    view_set_draw_callback(main_view->view, tt_main_view_draw);
    view_set_input_callback(main_view->view, tt_main_view_input);
    /* Das Modell ist nach der Allokation genullt: Idle, nicht verbunden, kein Hinweis */
    return main_view;
}

void tt_main_view_free(TtMainView* main_view) {
    furi_assert(main_view);
    view_free(main_view->view);
    free(main_view);
}

View* tt_main_view_get_view(TtMainView* main_view) {
    furi_assert(main_view);
    return main_view->view;
}

void tt_main_view_set_callback(TtMainView* main_view, TtMainViewCallback callback, void* context) {
    furi_assert(main_view);
    main_view->callback = callback;
    main_view->context = context;
}

void tt_main_view_set_config(TtMainView* main_view, const TtSettings* settings) {
    furi_assert(main_view);
    with_view_model(
        main_view->view,
        TtMainViewModel * model,
        {
            model->settings = *settings;
            model->show_elapsed = false;
            model->exit_hint_until = 0;
        },
        true);
}

void tt_main_view_update(
    TtMainView* main_view,
    TtTimerState state,
    uint32_t total_seconds,
    uint32_t elapsed_seconds,
    bool connected) {
    furi_assert(main_view);
    with_view_model(
        main_view->view,
        TtMainViewModel * model,
        {
            model->state = state;
            model->total_seconds = total_seconds;
            model->elapsed_seconds = elapsed_seconds;
            model->connected = connected;
        },
        true);
}
