#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SCREEN_HEIGHT 64
#define MAX_VALUES    10

typedef enum {
    SCREEN_COMBO_ENTRY,
    SCREEN_RESULTS,
    SCREEN_ABOUT
} ScreenState;

typedef struct {
    int first_lock;
    int second_lock;
    float resistance;
    bool exit;
    int selected;
    char result[256];
    ViewPort* view_port;
    Gui* gui;
    ScreenState screen_state;
} ComboCrackerState;

char* float_to_char(float num) {
    static char buffer[8];
    snprintf(buffer, sizeof(buffer), "%.1f", (double)num);
    return buffer;
}

// calculate the value for the combo lock
// Samy is my hero ;) -> https://www.youtube.com/watch?v=qkolWO6pAL8
void calculate_combo(ComboCrackerState* app) {
    float sticky_number = app->resistance;
    int sticky_as_int = (int)sticky_number;

    int first_digit;
    if((sticky_number - sticky_as_int) == 0.0f) {
        // first digit easy, resistance + 5... crazy world we live in
        first_digit = sticky_as_int + 5;
    } else {
        first_digit = (int)(sticky_number + 5) + 1; // ceiling that biih... prob. a better way
    }

    first_digit = first_digit % 40;
    int remainder = first_digit % 4;

    int a = app->first_lock;
    int b = app->second_lock;

    // third digit isn't too bad
    int third_position_values[MAX_VALUES];
    int third_count = 0;

    for(int i = 0; i < 3; i++) {
        if(a % 4 == remainder) third_position_values[third_count++] = a;
        if(b % 4 == remainder) third_position_values[third_count++] = b;
        a = (a + 10) % 40;
        b = (b + 10) % 40;
    }

    int row_1 = (remainder + 2) % 40;
    int row_2 = (row_1 + 4) % 40;

    int second_position_values[MAX_VALUES];
    int second_count = 0;
    second_position_values[second_count++] = row_1;
    second_position_values[second_count++] = row_2;

    for(int i = 0; i < 4; i++) {
        row_1 = (row_1 + 8) % 40;
        row_2 = (row_2 + 8) % 40;
        second_position_values[second_count++] = row_1;
        second_position_values[second_count++] = row_2;
    }

    // sort that biih
    for(int i = 0; i < second_count - 1; i++) {
        for(int j = i + 1; j < second_count; j++) {
            if(second_position_values[i] > second_position_values[j]) {
                int temp = second_position_values[i];
                second_position_values[i] = second_position_values[j];
                second_position_values[j] = temp;
            }
        }
    }

    // result to push to result_callback
    snprintf(app->result, sizeof(app->result), "First Pin: %d\nSecond Pin(s): ", first_digit);
    for(int i = 0; i < second_count; i++) {
        char buf[6];
        snprintf(buf, sizeof(buf), "%d", second_position_values[i]);
        strcat(app->result, buf);
        if(i < second_count - 1) strcat(app->result, ", ");
        if(i == 3) strcat(app->result, "\n -> ");
    }

    strcat(app->result, "\nThird Pin(s): ");
    for(int i = 0; i < third_count; i++) {
        char buf[5];
        snprintf(buf, sizeof(buf), "%d", third_position_values[i]);
        strcat(app->result, buf);
        if(i < third_count - 1) strcat(app->result, ", "); // may be more than one sometimes
        // deduce to 8 attempts by popping -> (third_digit +/- 2)
    }
}

// main screen to push the first, second, and resistance positions
void draw_callback(Canvas* canvas, void* ctx) {
    ComboCrackerState* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    char buf[16];

    canvas_draw_str(canvas, 2, 12, "First Lock:");
    snprintf(buf, sizeof(buf), "%s%d", app->selected == 0 ? ">" : " ", app->first_lock);
    canvas_draw_str(canvas, 100, 12, buf);

    canvas_draw_str(canvas, 2, 24, "Second Lock:");
    snprintf(buf, sizeof(buf), "%s%d", app->selected == 1 ? ">" : " ", app->second_lock);
    canvas_draw_str(canvas, 100, 24, buf);

    canvas_draw_str(canvas, 2, 36, "Resistance:");
    snprintf(
        buf, sizeof(buf), "%s%s", app->selected == 2 ? ">" : " ", float_to_char(app->resistance));
    canvas_draw_str(canvas, 100, 36, buf);

    snprintf(
        buf, sizeof(buf), "%sAbout", app->selected == 3 ? ">" : " "); // ugly but we rollin wit it
    canvas_draw_str(canvas, 2, 48, buf);

    canvas_draw_str(canvas, 2, 62, "OK to calculate"); // is there an OK icon??
}

// push the calc to screen
void result_draw_callback(Canvas* canvas, void* ctx) {
    ComboCrackerState* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    int y = 12;
    char result_copy[256];
    strncpy(result_copy, app->result, sizeof(result_copy));
    char* line = strtok(result_copy, "\n");

    while(line && y < SCREEN_HEIGHT - 10) {
        canvas_draw_str(canvas, 2, y, line);
        y += 10;
        line = strtok(NULL, "\n");
    }

    canvas_draw_str(canvas, 2, SCREEN_HEIGHT - 2, "Back to edit");
}

void about_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    int y = 12;
    canvas_draw_str(canvas, 2, y, "Combo Lock Cracker");
    y += 10;
    canvas_draw_str(canvas, 2, y, "Based on Samy Kamkar's");
    y += 10;
    canvas_draw_str(canvas, 2, y, "Master Lock research.");
    y += 10;
    canvas_draw_str(canvas, 2, y, "Crack Combo Locks in 8 tries");
    y += 10;
    canvas_draw_str(canvas, 2, y, "https://samy.pl/master/");
    y += 18;
    canvas_draw_str(canvas, 2, SCREEN_HEIGHT - 2, "Back to main menu");
}

void input_callback(InputEvent* event, void* ctx);
void result_input_callback(InputEvent* event, void* ctx) {
    ComboCrackerState* app = ctx;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->screen_state = SCREEN_COMBO_ENTRY;
        view_port_draw_callback_set(app->view_port, draw_callback, app);
        view_port_input_callback_set(app->view_port, input_callback, app);
    }
    view_port_update(app->view_port);
}

void input_callback(InputEvent* event, void* ctx) {
    ComboCrackerState* app = ctx;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            app->selected = (app->selected + 3) % 4;
            break;
        case InputKeyDown:
            app->selected = (app->selected + 1) % 4;
            break;
        case InputKeyLeft:
            if(app->selected == 0 && app->first_lock > 0) app->first_lock--;
            if(app->selected == 1 && app->second_lock > 0) app->second_lock--;
            if(app->selected == 2 && app->resistance > 0) app->resistance -= 0.5;
            break;
        case InputKeyRight: // can't figure out how to accomodate for long presses for fast incrementation??
            if(app->selected == 0 && app->first_lock < 10) app->first_lock++;
            if(app->selected == 1 && app->second_lock < 10) app->second_lock++;
            if(app->selected == 2 && app->resistance < 39.5) app->resistance += 0.5;
            if(app->selected == 3) { // about description
                app->screen_state = SCREEN_ABOUT;
                view_port_draw_callback_set(app->view_port, about_draw_callback, app);
                view_port_input_callback_set(app->view_port, result_input_callback, app);
            }
            break;
        case InputKeyOk:
            calculate_combo(app);
            app->screen_state = SCREEN_RESULTS;
            view_port_draw_callback_set(app->view_port, result_draw_callback, app);
            view_port_input_callback_set(app->view_port, result_input_callback, app);
            break;
        case InputKeyBack:
            app->exit = true;
            break;
        default:
            break;
        }
    }
    view_port_update(app->view_port);
}

int32_t combo_cracker_app(void* p) {
    UNUSED(p);

    ComboCrackerState* app = malloc(sizeof(ComboCrackerState));
    app->first_lock = 0;
    app->second_lock = 0;
    app->resistance = 0.0f;
    app->selected = 0;
    app->exit = false;
    app->screen_state = SCREEN_COMBO_ENTRY;
    app->result[0] = '\0';

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    while(!app->exit) {
        furi_delay_ms(50);
    }

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
