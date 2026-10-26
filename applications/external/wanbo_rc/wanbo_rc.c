#include "wanbo_rc.h"
#include "wanbo_ir.h"
#include "wanbo_rc_icons.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/icon.h>
#include <input/input.h>

#define GRID_COLS 3
#define BUTTON_COUNT 12

typedef struct {
    const char* label;
    WanboCommand command;
    bool opens_dpad;
    bool repeatable;
    const Icon* icon;
    const Icon* icon_selected;
} WanboButton;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    uint8_t selected;
    bool dpad_mode;
    bool running;
} WanboRcApp;

static const WanboButton wanbo_buttons[BUTTON_COUNT] = {
    {"Power", WanboCommandPower, false, false, &I_power_20x21, &I_power_hover_20x21},
    {"Mouse", WanboCommandMouse, false, false, &I_mouse_20x21, &I_mouse_hover_20x21},
    {"Options", WanboCommandSettings, false, false, &I_options_20x21, &I_options_hover_20x21},

    {"Setup", WanboCommandMenu, false, false, &I_setup_20x21, &I_setup_hover_20x21},
    {"DPad", WanboCommandOk, true, false, &I_dpad_20x21, &I_dpad_hover_20x21},
    {"Mute", WanboCommandMute, false, false, &I_mute_20x21, &I_mute_hover_20x21},

    {"Focus+", WanboCommandFocusPlus, false, true, &I_focus_up_20x21, &I_focus_up_hover_20x21},
    {"Return", WanboCommandReturn, false, false, &I_return_20x21, &I_return_hover_20x21},
    {"Vol+", WanboCommandVolumePlus, false, true, &I_vol_up_20x21, &I_vol_up_hover_20x21},

    {"Focus-", WanboCommandFocusMinus, false, true, &I_focus_dw_20x21, &I_focus_dw_hover_20x21},
    {"Home", WanboCommandHome, false, false, &I_home_20x21, &I_home_hover_20x21},
    {"Vol-", WanboCommandVolumeMinus, false, true, &I_vol_dw_20x21, &I_vol_dw_hover_20x21},
};

static const uint8_t wanbo_icon_x[GRID_COLS] = {1, 22, 43};
static const uint8_t wanbo_icon_y[4] = {17, 43, 69, 90};

static void wanbo_rc_send_with_feedback(WanboCommand command) {
    wanbo_ir_send(command);
}

static void wanbo_rc_select_up(WanboRcApp* app) {
    if(app->selected >= GRID_COLS) {
        app->selected -= GRID_COLS;
    }
}

static void wanbo_rc_select_down(WanboRcApp* app) {
    if(app->selected + GRID_COLS < BUTTON_COUNT) {
        app->selected += GRID_COLS;
    }
}

static void wanbo_rc_select_left(WanboRcApp* app) {
    if((app->selected % GRID_COLS) > 0) {
        app->selected--;
    }
}

static void wanbo_rc_select_right(WanboRcApp* app) {
    if((app->selected % GRID_COLS) < (GRID_COLS - 1)) {
        app->selected++;
    }
}

static void wanbo_rc_draw_main(Canvas* canvas, WanboRcApp* app) {
/*
    Os assets enviados ficaram visualmente invertidos no Flipper:
    - arquivo normal: botão escuro
    - arquivo hover: botão claro
    Por isso, nesta versão usamos:
    - selecionado: arquivo normal
    - não selecionado: arquivo hover
*/
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 9, 10, "Wanbo RC");

    for(uint8_t i = 0; i < BUTTON_COUNT; i++) {
        uint8_t col = i % GRID_COLS;
        uint8_t row = i / GRID_COLS;

        const Icon* icon = (i == app->selected) ?
                               wanbo_buttons[i].icon :
                               wanbo_buttons[i].icon_selected;

        canvas_draw_icon(canvas, wanbo_icon_x[col], wanbo_icon_y[row], icon);
    }
}

static void wanbo_rc_draw_dpad(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 17, 9, "D-Pad");

    canvas_draw_icon(canvas, 2, 16, &I_dpad_enter_60x60);
    canvas_draw_icon(canvas, 12, 80, &I_return_enter_40x40);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 126, "Hold Back: menu");
}

static void wanbo_rc_draw_callback(Canvas* canvas, void* context) {
    WanboRcApp* app = context;

    if(app->dpad_mode) {
        wanbo_rc_draw_dpad(canvas);
    } else {
        wanbo_rc_draw_main(canvas, app);
    }
}

static void wanbo_rc_input_callback(InputEvent* input_event, void* context) {
    FuriMessageQueue* input_queue = context;
    furi_message_queue_put(input_queue, input_event, FuriWaitForever);
}

/*
    Tela principal:
    A navegação fica no padrão normal do Flipper.
    Up sobe na grade, Down desce, Left vai à esquerda, Right vai à direita.

    Tela D-Pad:
    O Flipper será usado fisicamente na vertical, com o IR apontando para frente.
    Por isso somente aqui os comandos IR são rotacionados:
    Left físico  -> Up do controle
    Right físico -> Down do controle
    Down físico  -> Left do controle
    Up físico    -> Right do controle
*/

static void wanbo_rc_handle_main_input(WanboRcApp* app, InputEvent* event) {
    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyUp:
            wanbo_rc_select_up(app);
            view_port_update(app->view_port);
            break;

        case InputKeyDown:
            wanbo_rc_select_down(app);
            view_port_update(app->view_port);
            break;

        case InputKeyLeft:
            wanbo_rc_select_left(app);
            view_port_update(app->view_port);
            break;

        case InputKeyRight:
            wanbo_rc_select_right(app);
            view_port_update(app->view_port);
            break;

        case InputKeyOk:
            if(wanbo_buttons[app->selected].opens_dpad) {
                app->dpad_mode = true;
                view_port_update(app->view_port);
            } else {
                wanbo_rc_send_with_feedback(wanbo_buttons[app->selected].command);
            }
            break;

        default:
            break;
        }
    }

    if(event->type == InputTypeRepeat && event->key == InputKeyOk) {
        if(wanbo_buttons[app->selected].repeatable) {
            wanbo_rc_send_with_feedback(wanbo_buttons[app->selected].command);
        }
    }

    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        app->running = false;
    }
}

static void wanbo_rc_handle_dpad_input(WanboRcApp* app, InputEvent* event) {
    if(event->type == InputTypePress || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyLeft:
            wanbo_rc_send_with_feedback(WanboCommandLeft);
            break;

        case InputKeyRight:
            wanbo_rc_send_with_feedback(WanboCommandRight);
            break;

        case InputKeyDown:
            wanbo_rc_send_with_feedback(WanboCommandDown);
            break;

        case InputKeyUp:
            wanbo_rc_send_with_feedback(WanboCommandUp);
            break;

        case InputKeyOk:
            wanbo_rc_send_with_feedback(WanboCommandOk);
            break;

        default:
            break;
        }
    }

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        wanbo_rc_send_with_feedback(WanboCommandReturn);
    }

    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        app->dpad_mode = false;
        view_port_update(app->view_port);
    }
}

static WanboRcApp* wanbo_rc_app_alloc(void) {
    WanboRcApp* app = malloc(sizeof(WanboRcApp));

    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();

    app->selected = 0;
    app->dpad_mode = false;
    app->running = true;

    view_port_set_orientation(app->view_port, ViewPortOrientationVertical);
    view_port_draw_callback_set(app->view_port, wanbo_rc_draw_callback, app);
    view_port_input_callback_set(app->view_port, wanbo_rc_input_callback, app->input_queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void wanbo_rc_app_free(WanboRcApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);

    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);

    free(app);
}

int32_t wanbo_rc_app(void* p) {
    UNUSED(p);

    WanboRcApp* app = wanbo_rc_app_alloc();

    InputEvent event;

    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(app->dpad_mode) {
                wanbo_rc_handle_dpad_input(app, &event);
            } else {
                wanbo_rc_handle_main_input(app, &event);
            }
        }
    }

    wanbo_rc_app_free(app);

    return 0;
}
