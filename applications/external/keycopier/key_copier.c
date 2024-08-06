#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include "key_copier_icons.h"
#include "key_formats.h"
#include "key_copier.h"
#include <applications/services/storage/storage.h>
#include <applications/services/dialogs/dialogs.h>
#include <stdbool.h>
#include <math.h>
#include <flipper_format.h>

#define TAG "KeyCopier"

#define INCHES_PER_PX 0.00978

// Change this to BACKLIGHT_AUTO if you don't want the backlight to be continuously on.
#define BACKLIGHT_ON 1

// Our application menu has 3 items.  You can add more items if you want.
typedef enum {
    KeyCopierSubmenuIndexMeasure,
    KeyCopierSubmenuIndexConfigure,
    KeyCopierSubmenuIndexSave,
    KeyCopierSubmenuIndexLoad,
    KeyCopierSubmenuIndexAbout,
} KeyCopierSubmenuIndex;

// Each view is a screen we show the user.
typedef enum {
    KeyCopierViewSubmenu, // The menu when the app starts
    KeyCopierViewTextInput, // Input for configuring text settings
    KeyCopierViewConfigure_i, // The configuration screen
    KeyCopierViewConfigure_e, // The configuration screen
    KeyCopierViewSave,
    KeyCopierViewLoad,
    KeyCopierViewMeasure, // The main screen
    KeyCopierViewAbout, // The about screen with directions, link to social channel, etc.
} KeyCopierView;

typedef enum {
    KeyCopierEventIdRedrawScreen = 0, // Custom event to redraw the screen
    KeyCopierEventIdOkPressed = 42, // Custom event to process OK button getting pressed down
} KeyCopierEventId;

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    NotificationApp* notifications; // Used for controlling the backlight
    Submenu* submenu; // The application menu
    TextInput* text_input; // The text input screen
    VariableItemList* variable_item_list_config; // The configuration screen
    View* view_measure; // The main screen
    View* view_config_e; // The configuration screen
    View* view_save;
    View* view_load; // The load view
    Widget* widget_about; // The about screen
    VariableItem* key_name_item; // The name setting item (so we can update the text)
    char* temp_buffer; // Temporary buffer for text input
    uint32_t temp_buffer_size; // Size of temporary buffer

    DialogsApp* dialogs;
    FuriString* file_path;
    FuriTimer* timer; // Timer for redrawing the screen
} KeyCopierApp;

typedef struct {
    uint32_t format_index; // The index for total number of pins
    FuriString* key_name_str; // The name setting
    uint8_t pin_slc; // The pin that is being adjusted
    uint8_t* depth; // The cutting depth
    bool data_loaded;
    KeyFormat format;
} KeyCopierMeasureModel;

void initialize_model(KeyCopierMeasureModel* model) {
    if(model->depth != NULL) {
        free(model->depth);
    }
    model->format_index = 0;
    memcpy(&model->format, &all_formats[model->format_index], sizeof(KeyFormat));
    model->depth = (uint8_t*)malloc((model->format.pin_num + 1) * sizeof(uint8_t));
    for(uint8_t i = 0; i <= model->format.pin_num; i++) {
        model->depth[i] = model->format.min_depth_ind;
    }
    model->pin_slc = 1;
    model->data_loaded = 0;
    model->key_name_str = furi_string_alloc();
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t key_copier_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the submenu.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t key_copier_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return KeyCopierViewSubmenu;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - KeyCopierApp object.
 * @param      index     The KeyCopierSubmenuIndex item that was clicked.
*/
static void key_copier_submenu_callback(void* context, uint32_t index) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    switch(index) {
    case KeyCopierSubmenuIndexMeasure:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewMeasure);
        break;
    case KeyCopierSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewConfigure_e);
        break;
    case KeyCopierSubmenuIndexSave:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSave);
        break;
    case KeyCopierSubmenuIndexLoad:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewLoad);
        break;
    case KeyCopierSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewAbout);
        break;
    default:
        break;
    }
}

char* format_names[COUNT_OF(all_formats)];
void initialize_format_names(char** format_names) {
    // Populate the format_names array
    for(size_t i = 0; i < COUNT_OF(all_formats); i++) {
        format_names[i] = all_formats[i].format_name;
    }
}

static const char* format_config_label = "Key Format";
static void key_copier_format_change(VariableItem* item) {
    KeyCopierApp* app = variable_item_get_context(item);
    KeyCopierMeasureModel* model = view_get_model(app->view_measure);
    if(model->data_loaded) {
        variable_item_set_current_value_index(item, model->format_index);
    } else {
        uint8_t format_index = variable_item_get_current_value_index(item);
        model->format_index = format_index;
        model->format = all_formats[format_index];
    }
    model->data_loaded = false;
    variable_item_set_current_value_text(item, model->format.format_name);

    model->format = all_formats[model->format_index];
    if(model->depth != NULL) {
        free(model->depth);
    }
    model->depth = (uint8_t*)malloc((model->format.pin_num + 1) * sizeof(uint8_t));
    for(uint8_t i = 0; i <= model->format.pin_num; i++) {
        model->depth[i] = model->format.min_depth_ind;
    }
}

/**
 * Our 2nd sample setting is a text field.  When the user clicks OK on the configuration 
 * setting we use a text input screen to allow the user to enter a name.  This function is
 * called when the user clicks OK on the text input screen.
*/
static const char* key_name_entry_text = "Enter name";
static void key_copier_file_saver(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    KeyCopierMeasureModel* model = view_get_model(app->view_measure);
    bool redraw = true;
    with_view_model(
        app->view_measure,
        KeyCopierMeasureModel * model,
        { furi_string_set(model->key_name_str, app->temp_buffer); },
        redraw);
    FuriString* file_path = furi_string_alloc();
    furi_string_printf(
        file_path,
        "%s/%s%s",
        STORAGE_APP_DATA_PATH_PREFIX,
        furi_string_get_cstr(model->key_name_str),
        KEY_COPIER_SAVE_EXTENSION);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    FURI_LOG_D(TAG, "mkdir finished");
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);
    do {
        const uint32_t version = 1;
        const uint32_t pin_num_buffer = (uint32_t)model->format.pin_num;
        const uint32_t macs_buffer = (uint32_t)model->format.macs;
        if(!flipper_format_file_open_always(flipper_format, furi_string_get_cstr(file_path)))
            break;
        if(!flipper_format_write_header_cstr(flipper_format, "Flipper Key Copier File", version))
            break;
        if(!flipper_format_write_string_cstr(
               flipper_format, "Key Format", model->format.format_name))
            break;
        if(!flipper_format_write_string_cstr(
               flipper_format, "Key Format Short", model->format.format_short_name))
            break;
        if(!flipper_format_write_uint32(flipper_format, "Number of Pins", &pin_num_buffer, 1))
            break;
        if(!flipper_format_write_uint32(
               flipper_format, "Maximum Adjacent Cut Specification (MACS)", &macs_buffer, 1))
            break;
        FuriString* buffer = furi_string_alloc();
        for(int i = 0; i < model->format.pin_num; i++) {
            if(i < model->format.pin_num - 1) {
                furi_string_cat_printf(buffer, "%d-", model->depth[i]);
            } else {
                furi_string_cat_printf(buffer, "%d", model->depth[i]);
            }
        }
        if(!flipper_format_write_string(
               flipper_format, "Bitting Pattern", buffer))
            break;
        furi_string_free(buffer);
        // signal that the file was written successfully
    } while(0);
    flipper_format_free(flipper_format);

    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSubmenu);
}
/**
 * @brief      Callback when item in configuration screen is clicked.
 * @details    This function is called when user clicks OK on an item in the configuration screen.
 *            If the item clicked is our text field then we switch to the text input screen.
 * @param      context  The context - KeyCopierApp object.
 * @param      index - The index of the item that was clicked.
*/
static void key_copier_view_save_callback(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    // Header to display on the text input screen.
    text_input_set_header_text(app->text_input, key_name_entry_text);

    // Copy the current name into the temporary buffer.
    bool redraw = false;
    with_view_model(
        app->view_measure,
        KeyCopierMeasureModel * model,
        {
            strncpy(
                app->temp_buffer,
                furi_string_get_cstr(model->key_name_str),
                app->temp_buffer_size);
        },
        redraw);

    // Configure the text input.  When user enters text and clicks OK, key_copier_setting_text_updated be called.
    bool clear_previous_text = false;
    text_input_set_result_callback(
        app->text_input,
        key_copier_file_saver,
        app,
        app->temp_buffer,
        app->temp_buffer_size,
        clear_previous_text);

    // Pressing the BACK button will reload the configure screen.
    view_set_previous_callback(
        text_input_get_view(app->text_input), key_copier_navigation_submenu_callback);

    // Show text input dialog.
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewTextInput);
}

static inline int min(int a, int b) {
    return (a < b) ? a : b;
}

static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

static double inches_per_px = (double)INCHES_PER_PX;

/**
 * @brief      Callback for drawing the game screen.
 * @details    This function is called when the screen needs to be redrawn, like when the model gets updated.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void key_copier_view_measure_draw_callback(Canvas* canvas, void* model) {
    KeyCopierMeasureModel* my_model = (KeyCopierMeasureModel*)model;
    KeyFormat my_format = my_model->format;

    int pin_half_width_px = (int)round((my_format.pin_width_inch / inches_per_px) / 2);
    int pin_step_px = (int)round(my_format.pin_increment_inch / inches_per_px);
    double drill_radians =
        (180 - my_format.drill_angle) / 2 / 180 * (double)M_PI; // Convert angle to radians
    double tangent = tan(drill_radians);

    int post_extra_x_px = 0;
    int pre_extra_x_px = 0;
    for(int current_pin = 1; current_pin <= my_model->format.pin_num; current_pin += 1) {
        double current_center_px =
            my_format.first_pin_inch + (current_pin - 1) * my_format.pin_increment_inch;
        int pin_center_px = (int)round(current_center_px / inches_per_px);

        int top_contour_px = (int)round(63 - my_format.uncut_depth_inch / inches_per_px);
        canvas_draw_line(canvas, pin_center_px, 25, pin_center_px, 50);
        int current_depth = my_model->depth[current_pin - 1] - my_format.min_depth_ind;
        int current_depth_px =
            (int)round(current_depth * my_format.depth_step_inch / inches_per_px);
        canvas_draw_line(
            canvas,
            pin_center_px - pin_half_width_px,
            top_contour_px + current_depth_px,
            pin_center_px + pin_half_width_px,
            top_contour_px + current_depth_px);
        int last_depth = my_model->depth[current_pin - 2] - my_format.min_depth_ind;
        int next_depth = my_model->depth[current_pin] - my_format.min_depth_ind;
        if(current_pin == 1) {
            canvas_draw_line(
                canvas,
                0,
                top_contour_px,
                pin_center_px - pin_half_width_px - current_depth_px,
                top_contour_px);
            last_depth = 0;
            pre_extra_x_px = max(current_depth_px + pin_half_width_px, 0);
        }
        if(current_pin == my_model->format.pin_num) {
            next_depth = 0;
        }
        if((last_depth + current_depth) > my_format.clearance &&
           current_depth != 0) { //yes intersection

            if(current_pin != 1) {
                pre_extra_x_px =
                    min(max(pin_step_px - post_extra_x_px, pin_half_width_px),
                        pin_step_px - pin_half_width_px);
            }
            canvas_draw_line(
                canvas,
                pin_center_px - pre_extra_x_px,
                top_contour_px +
                    max((int)round(
                            (current_depth_px - (pre_extra_x_px - pin_half_width_px)) * tangent),
                        0),
                pin_center_px - pin_half_width_px,
                top_contour_px + (int)round(current_depth_px * tangent));
        } else {
            int last_depth_px = (int)round(last_depth * my_format.depth_step_inch / inches_per_px);
            int down_slope_start_x_px = pin_center_px - pin_half_width_px - current_depth_px;
            canvas_draw_line(
                canvas,
                pin_center_px - pin_half_width_px - current_depth_px,
                top_contour_px,
                pin_center_px - pin_half_width_px,
                top_contour_px + (int)round(current_depth_px * tangent));
            canvas_draw_line(
                canvas,
                min(pin_center_px - pin_step_px + pin_half_width_px + last_depth_px,
                    down_slope_start_x_px),
                top_contour_px,
                down_slope_start_x_px,
                top_contour_px);
        }
        if((current_depth + next_depth) > my_format.clearance &&
           current_depth != 0) { //yes intersection
            double numerator = (double)current_depth;
            double denominator = (double)(current_depth + next_depth);
            double product = (numerator / denominator) * pin_step_px;
            post_extra_x_px =
                (int)min(max(product, pin_half_width_px), pin_step_px - pin_half_width_px);
            canvas_draw_line(
                canvas,
                pin_center_px + pin_half_width_px,
                top_contour_px + current_depth_px,
                pin_center_px + post_extra_x_px,
                top_contour_px +
                    max(current_depth_px -
                            (int)round((post_extra_x_px - pin_half_width_px) * tangent),
                        0));
        } else { // no intersection
            canvas_draw_line(
                canvas,
                pin_center_px + pin_half_width_px,
                top_contour_px + (int)round(current_depth_px * tangent),
                pin_center_px + pin_half_width_px + current_depth_px,
                top_contour_px);
        }
    }

    int level_contour_px =
        (int)round((my_format.last_pin_inch + my_format.pin_increment_inch) / inches_per_px - 4);
    canvas_draw_line(canvas, 0, 62, level_contour_px, 62);

    int slc_pin_px = (int)round(
        (my_format.first_pin_inch + (my_model->pin_slc - 1) * my_format.pin_increment_inch) /
        inches_per_px);
    canvas_draw_str(canvas, slc_pin_px - 2, 23, "*");

    FuriString* xstr = furi_string_alloc();
    int buffer_size = my_model->format.pin_num + 1;
    char depth_str[buffer_size];
    depth_str[0] = '\0'; // Initialize the string
    // Manual string concatenation
    char* pos = depth_str;
    for(int i = 0; i < my_model->format.pin_num; i++) {
        int written = snprintf(pos, buffer_size - (pos - depth_str), "%u", my_model->depth[i]);
        if(written < 0 || written >= buffer_size - (pos - depth_str)) {
            // Handle error
            break;
        }
        pos += written;
    }
    furi_string_printf(xstr, "bitting: %s", depth_str);
    canvas_draw_str(canvas, 0, 10, furi_string_get_cstr(xstr));

    furi_string_printf(xstr, "%s", my_format.format_short_name);
    canvas_draw_str(canvas, 110, 10, furi_string_get_cstr(xstr));

    //furi_string_printf(xstr, "Num of Pins: %s", format_names[my_model->format_index]);
    //canvas_draw_str(canvas, 44, 24, furi_string_get_cstr(xstr));
    furi_string_free(xstr);
}

/**
 * @brief      Callback for timer elapsed.
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - KeyCopierApp object.
*/
static void key_copier_view_measure_timer_callback(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, KeyCopierEventIdRedrawScreen);
}

/**
 * @brief      Callback when the user starts the game screen.
 * @details    This function is called when the user enters the game screen.  We start a timer to
 *           redraw the screen periodically (so the random number is refreshed).
 * @param      context  The context - KeyCopierApp object.
*/
static void key_copier_view_measure_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(500);
    KeyCopierApp* app = (KeyCopierApp*)context;
    furi_assert(app->timer == NULL);
    app->timer =
        furi_timer_alloc(key_copier_view_measure_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer, period);
}

/**
 * @brief      Callback when the user exits the game screen.
 * @details    This function is called when the user exits the game screen.  We stop the timer.
 * @param      context  The context - KeyCopierApp object.
*/
static void key_copier_view_measure_exit_callback(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - KeyCopierEventId value.
 * @param      context  The context - KeyCopierApp object.
*/
static bool key_copier_view_measure_custom_event_callback(uint32_t event, void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    switch(event) {
    case KeyCopierEventIdRedrawScreen:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_measure, KeyCopierMeasureModel * _model, { UNUSED(_model); }, redraw);
            return true;
        }
    case KeyCopierEventIdOkPressed:
        // Process the OK button.  We go to the saving scene.
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewTextInput);
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback for game screen input.
 * @details    This function is called when the user presses a button while on the game screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - KeyCopierApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool key_copier_view_measure_input_callback(InputEvent* event, void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft: {
            // Left button clicked, reduce x coordinate.
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierMeasureModel * model,
                {
                    if(model->pin_slc > 1) {
                        model->pin_slc--;
                    }
                },
                redraw);
            break;
        }
        case InputKeyRight: {
            // Left button clicked, reduce x coordinate.
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierMeasureModel * model,
                {
                    if(model->pin_slc < model->format.pin_num) {
                        model->pin_slc++;
                    }
                },
                redraw);
            break;
        }
        case InputKeyUp: {
            // Left button clicked, reduce x coordinate.
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierMeasureModel * model,
                {
                    if(model->depth[model->pin_slc - 1] > model->format.min_depth_ind) {
                        if(model->pin_slc == 1) { //first pin only limited by the next one
                            if(model->depth[model->pin_slc] - model->depth[model->pin_slc - 1] <
                               model->format.macs)
                                model->depth[model->pin_slc - 1]--;
                        } else if(
                            model->pin_slc ==
                            model->format.pin_num) { //last pin only limited by the previous one
                            if(model->depth[model->pin_slc - 2] -
                                   model->depth[model->pin_slc - 1] <
                               model->format.macs) {
                                model->depth[model->pin_slc - 1]--;
                            }
                        } else {
                            if(model->depth[model->pin_slc] - model->depth[model->pin_slc - 1] <
                                   model->format.macs &&
                               model->depth[model->pin_slc - 2] -
                                       model->depth[model->pin_slc - 1] <
                                   model->format.macs) {
                                model->depth[model->pin_slc - 1]--;
                            }
                        }
                    }
                },
                redraw);
            break;
        }
        case InputKeyDown: {
            // Right button clicked, increase x coordinate.
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierMeasureModel * model,
                {
                    if(model->depth[model->pin_slc - 1] < model->format.max_depth_ind) {
                        if(model->pin_slc == 1) { //first pin only limited by the next one
                            if(model->depth[model->pin_slc - 1] - model->depth[model->pin_slc] <
                               model->format.macs)
                                model->depth[model->pin_slc - 1]++;
                        } else if(
                            model->pin_slc ==
                            model->format.pin_num) { //last pin only limited by the previous one
                            if(model->depth[model->pin_slc - 1] -
                                   model->depth[model->pin_slc - 2] <
                               model->format.macs) {
                                model->depth[model->pin_slc - 1]++;
                            }
                        } else {
                            if(model->depth[model->pin_slc - 1] - model->depth[model->pin_slc] <
                                   model->format.macs &&
                               model->depth[model->pin_slc - 1] -
                                       model->depth[model->pin_slc - 2] <
                                   model->format.macs) {
                                model->depth[model->pin_slc - 1]++;
                            }
                        }
                    }
                },
                redraw);
            break;
        }
        default:
            // Handle other keys or do nothing
            break;
        }
    } else if(event->key == InputKeyOk) {
        // We choose to send a custom event when user presses OK button.  key_copier_custom_event_callback will
        // handle our KeyCopierEventIdOkPressed event.  We could have just put the code from
        // key_copier_custom_event_callback here, it's a matter of preference.
        view_dispatcher_send_custom_event(app->view_dispatcher, KeyCopierEventIdOkPressed);
        return true;
    }

    return false;
}

/**
 * @brief      Allocate the key_copier application.
 * @details    This function allocates the key_copier application resources.
 * @return     KeyCopierApp object.
*/
static KeyCopierApp* key_copier_app_alloc() {
    KeyCopierApp* app = (KeyCopierApp*)malloc(sizeof(KeyCopierApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Measure", KeyCopierSubmenuIndexMeasure, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Config", KeyCopierSubmenuIndexConfigure, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Save", KeyCopierSubmenuIndexSave, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Load", KeyCopierSubmenuIndexLoad, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", KeyCopierSubmenuIndexAbout, key_copier_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu), key_copier_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSubmenu);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewTextInput, text_input_get_view(app->text_input));
    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);
    app->temp_buffer = "";

    app->view_measure = view_alloc();
    view_set_draw_callback(app->view_measure, key_copier_view_measure_draw_callback);
    view_set_input_callback(app->view_measure, key_copier_view_measure_input_callback);
    view_set_previous_callback(app->view_measure, key_copier_navigation_submenu_callback);
    view_set_enter_callback(app->view_measure, key_copier_view_measure_enter_callback);
    view_set_exit_callback(app->view_measure, key_copier_view_measure_exit_callback);
    view_set_context(app->view_measure, app);
    view_set_custom_callback(app->view_measure, key_copier_view_measure_custom_event_callback);
    view_allocate_model(app->view_measure, ViewModelTypeLockFree, sizeof(KeyCopierMeasureModel));
    KeyCopierMeasureModel* model = view_get_model(app->view_measure);

    initialize_model(model);
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewMeasure, app->view_measure);

    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);
    VariableItem* item = variable_item_list_add(
        app->variable_item_list_config,
        format_config_label,
        COUNT_OF(format_names),
        key_copier_format_change,
        app);

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        KeyCopierViewConfigure_e,
        variable_item_list_get_view(app->variable_item_list_config));
    variable_item_set_current_value_index(item, model->format_index);
    variable_item_set_current_value_text(item, model->format.format_name);

    app->view_save = view_alloc();
    view_set_context(app->view_save, app);
    view_set_enter_callback(app->view_save, key_copier_view_save_callback);
    view_set_previous_callback(app->view_save, key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewSave, app->view_save);

    app->view_load = view_alloc();
    view_set_context(app->view_load, app);
    view_set_enter_callback(app->view_load, key_copier_view_save_callback);
    view_set_previous_callback(app->view_load, key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewLoad, app->view_load);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Key Maker App 0.3\n\nTo measure your key:\n\n1. Place it on top of the screen.\n\n2. Use the horizontal lines to align your key.\n\n3. Adjust each pin's bitting depth until they match. It's easier if you close one eye.\n\n4. Save the bitting pattern and key format by pressing the OK button or in the Config menu.\n\nGithub: github.com/zinongli/KeyCopier \n\nSpecial thanks to Derek Jamison's Skeleton App Template.");
    view_set_previous_callback(
        widget_get_view(app->widget_about), key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    return app;
}

/**
 * @brief      Free the key_copier application.
 * @details    This function frees the key_copier application resources.
 * @param      app  The key_copier application object.
*/
static void key_copier_app_free(KeyCopierApp* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewTextInput);
    text_input_free(app->text_input);
    free(app->temp_buffer);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewMeasure);
    with_view_model(
        app->view_measure,
        KeyCopierMeasureModel * model,
        {
            if(model->depth != NULL) {
                free(model->depth);
            }
        },
        false);
    view_free(app->view_measure);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewConfigure_e);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewSave);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewLoad);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

/**
 * @brief      Main function for key_copier application.
 * @details    This function is the entry point for the key_copier application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t main_key_copier_app(void* _p) {
    UNUSED(_p);

    KeyCopierApp* app = key_copier_app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    key_copier_app_free(app);
    return 0;
}
