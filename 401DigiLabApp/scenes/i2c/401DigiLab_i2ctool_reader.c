/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2024 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "scenes/i2c/401DigiLab_i2ctool_reader.h"
static const char* TAG = "401_DigiLabI2CToolReader";

#include <401_gui.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_rtc.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <toolbox/path.h>
////#include <u8g2_glue.h>

#include "401_err.h"
#include "cJSON/cJSON.h"

/**
 * @brief Render callback for the app i2ctoolreader.
 *
 * @param canvas The canvas to be used for rendering.
 * @param model The model passed to the callback.
 */
#define CELL_WIDTH       4
#define LINE_HEIGHT      6
#define HEXDUMP_OFFSET_W 1
#define HEXDUMP_OFFSET_H 13

static uint8_t input_read_offset = 0;

static void hex_dump_grid_transform(uint8_t x, uint8_t y, uint8_t* xt, uint8_t* yt) {
    uint8_t space = 18;
    if(x < 18) {
        space = (x / 2) * 2;
    }
    *xt = HEXDUMP_OFFSET_W + x * CELL_WIDTH + space;
    *yt = HEXDUMP_OFFSET_H + y * LINE_HEIGHT;
}
// Fonction pour dessiner un caractère individuel sur une cellule de la grille
void hex_dump_grid_draw(Canvas* canvas, int cell_x, int cell_y, char c) {
    char str[2] = {c, '\0'}; // Convertir le caractère en chaîne de 1 caractère
    uint8_t x = 0;
    uint8_t y = 0;
    hex_dump_grid_transform(cell_x, cell_y, &x, &y);
    // If the octet is on Offset side.
    if(cell_x <= 1) {
        canvas_set_color(canvas, ColorWhite);
    }
    // If the octet is on the Data side.
    else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str_aligned(canvas, x, y, AlignLeft, AlignTop, str);
}

void hex_dump_grid_draw_selected(Canvas* canvas, uint8_t offset) {
    canvas_set_color(canvas, ColorXOR);
    uint8_t xb = ((offset % 8) + 1) * 2;
    uint8_t yb = offset / 8;
    uint8_t xa = (offset % 8) + 18;
    uint8_t ya = yb;
    uint8_t xt = 0;
    uint8_t yt = 0;

    hex_dump_grid_transform(xb, yb, &xt, &yt);
    canvas_draw_box(canvas, xt - 1, yt - 1, (CELL_WIDTH * 2) + 1, LINE_HEIGHT + 1);
    hex_dump_grid_transform(xa, ya, &xt, &yt);
    canvas_draw_box(canvas, xt - 1, yt - 1, (CELL_WIDTH) + 1, LINE_HEIGHT + 1);
}

void app_i2ctoolreader_render_callback(Canvas* canvas, void* ctx) {
    furi_assert(canvas);
    furi_assert(ctx);

    i2CToolReaderModel* model = (i2CToolReaderModel*)ctx;

    canvas_clear(canvas);
    // char str[32] = {};
    // snprintf(str, 32, "0x%.02X", model->device->addr);
    // canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, str);
    size_t data_len = model->data_len;
    uint8_t* data = model->data;

    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);
    uint8_t bytes_per_line = 8;
    size_t offset = 0;
    int line = 0; // L'index de la ligne dans la grille
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(
        canvas,
        0,
        HEXDUMP_OFFSET_H,
        CELL_WIDTH * 2 + 2,
        LINE_HEIGHT * ((data_len / bytes_per_line) + 1) + 2,
        2);
    // Parcourir les données par blocs de `bytes_per_line` octets
    while(offset < data_len) {
        int cell_x = 0;
        // Afficher l'offset (sur 2 caractères hexadécimaux)
        char offset_str[3];
        snprintf(offset_str, sizeof(offset_str), "%02X", offset);
        hex_dump_grid_draw(canvas, cell_x++, line, offset_str[0]);
        hex_dump_grid_draw(canvas, cell_x++, line, offset_str[1]);

        // Afficher les octets sous forme hexadécimale
        for(size_t i = 0; i < bytes_per_line; i++) {
            if(offset + i < data_len) {
                char byte_str[3];
                snprintf(byte_str, sizeof(byte_str), "%02X", data[offset + i]);
                hex_dump_grid_draw(canvas, cell_x++, line, byte_str[0]);
                hex_dump_grid_draw(canvas, cell_x++, line, byte_str[1]);
            } else {
                // Si moins d'octets, combler les cellules avec des espaces
                cell_x += 2;
            }
        }

        // Ajouter un espace avant la partie ASCII
        // cell_x++;

        // Afficher la représentation ASCII des octets
        for(size_t i = 0; i < bytes_per_line; i++) {
            if(offset + i < data_len) {
                unsigned char byte = data[offset + i];
                char ascii_char = isprint(byte) ? byte : '.';
                hex_dump_grid_draw(canvas, cell_x++, line, ascii_char);
            } else {
                cell_x++;
            }
        }
        // Passer à la ligne suivante
        offset += bytes_per_line;
        line++;
    }

    hex_dump_grid_draw_selected(canvas, model->data_offset);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Hex editor");
    canvas_set_font(canvas, FontSecondary);
    elements_button_up(canvas, "+");
    elements_button_down(canvas, "-");
    elements_button_center(canvas, "Write");
    elements_button_left(canvas, "Offset-");
    elements_button_right(canvas, "Offset+");
}

/**
 * @brief Write on i2C Mems in page_size blocks
 *
 * @param addr I2C address
 * @param offset Offset address
 * @param data Data pointer
 * @param length Total length of data
 * @param page_size Size of pages
 * @return Returns true/false.
 */
bool i2cmem_write(uint8_t addr, uint16_t offset, uint8_t* data, uint8_t length, uint8_t page_size) {
    furi_hal_i2c_acquire(I2C_BUS);
    bool res = true;

    FURI_LOG_I(
        TAG, "Writing %d bytes starting at 0x%04X with %d-byte pages", length, offset, page_size);

    while(length > 0) {
        uint16_t page_start = (offset / page_size) * page_size; // Start of the current page
        uint8_t page_offset = offset % page_size; // Offset within the current page
        uint8_t write_length = (page_size - page_offset < length) ? (page_size - page_offset) :
                                                                    length;

        FURI_LOG_I(
            TAG,
            "Writing %d bytes at 0x%04X (Page start: 0x%04X, Page offset: %d)",
            write_length,
            offset,
            page_start,
            page_offset);

        if(!furi_hal_i2c_write_mem(I2C_BUS, addr, offset, data, write_length, 100)) {
            FURI_LOG_E(TAG, "Write failed at 0x%04X", offset);
            res = false;
        }

        data += write_length; // Advance data pointer
        offset += write_length; // Advance offset
        length -= write_length; // Reduce remaining bytes
    }

    furi_hal_i2c_release(I2C_BUS);
    return res;
}

void app_i2ctoolreader_write_success_popup_callback(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, I2CReaderEventExitPopup);
}

/**
 * @brief Callback function to handle input events for the app i2ctoolreader.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_i2ctoolreader_input_callback(InputEvent* input_event, void* ctx) {
    FURI_LOG_I(TAG, "app_i2ctoolreader_input_callback");
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    i2CToolReaderModel* model = view_get_model(appI2CToolReader->view);
    UNUSED(ctx);
    bool consumed = false;
    uint8_t increment = 1;
    if((input_event->type == InputTypeShort) || (input_event->type == InputTypeRepeat)) {
        switch(input_event->key) {
        case InputKeyUp:
            if(input_event->type == InputTypeRepeat) {
                increment = 0x10;
            }
            if(model->data != NULL) {
                model->data[model->data_offset] += increment;
                view_commit_model(appI2CToolReader->view, true);
            }
            break;
        case InputKeyDown:
            if(input_event->type == InputTypeRepeat) {
                increment = 0x10;
            }
            if(model->data != NULL) {
                model->data[model->data_offset] -= increment;
                view_commit_model(appI2CToolReader->view, true);
            }
            break;

        case InputKeyLeft: // Modify dataOffset
            if(input_event->type == InputTypeRepeat) {
                increment = 4;
            }
            if(model->data_offset == 0) {
                model->data_offset = model->data_len - 1;
            } else {
                model->data_offset -= increment;
            }
            view_commit_model(appI2CToolReader->view, true);
            break;

        case InputKeyRight: // Modify dataOffset
            if(input_event->type == InputTypeRepeat) {
                increment = 4;
            }
            if(model->data_offset < model->data_len - 1) {
                model->data_offset += increment;
                if(model->data_offset >= model->data_len - 1) {
                    model->data_offset = model->data_len - 1;
                }
            } else {
                model->data_offset = 0;
            }
            view_commit_model(appI2CToolReader->view, true);
            break;
        case InputKeyOk:
            bool res = false;

            res = i2cmem_write(
                model->device->addr << 1, model->read_offset, model->data, model->data_len, 7);
            if(res == false) {
                FURI_LOG_E(TAG, "Could not write mem");
                popup_set_header(appI2CToolReader->popup, "Error!", 64, 11, AlignCenter, AlignTop);
                popup_set_text(
                    appI2CToolReader->popup,
                    "Couldn't write data to chip",
                    64,
                    22,
                    AlignCenter,
                    AlignTop);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolWriterPopup);
            } else {
                popup_set_header(
                    appI2CToolReader->popup, "Success!", 64, 11, AlignCenter, AlignTop);
                popup_set_text(
                    appI2CToolReader->popup, "Chip programmed", 64, 22, AlignCenter, AlignTop);
                view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolWriterPopup);
            }
            break;
        default:
            FURI_LOG_I(TAG, "Resume to not handled");
            break;
        }
    }

    return consumed;
}

static void app_i2ctoolreader_input_readOffset_callback(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    i2CToolReaderModel* model = view_get_model(appI2CToolReader->view);

    model->read_offset = input_read_offset;
    view_commit_model(appI2CToolReader->view, false);
    view_dispatcher_send_custom_event(app->view_dispatcher, I2CReaderEventByteInput);
}

static void app_i2ctoolreader_input_readLen_callback(void* ctx, int32_t number) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    i2CToolReaderModel* model = view_get_model(appI2CToolReader->view);

    model->read_len = number;
    view_commit_model(appI2CToolReader->view, false);
    view_dispatcher_send_custom_event(app->view_dispatcher, I2CReaderEventNumberInput);
}

/**
 * @brief Allocate memory and initialize the app i2ctoolreader.
 *
 * @return Returns a pointer to the allocated AppI2CToolReader.
 */
AppI2CToolReader* app_i2ctoolreader_alloc(void* ctx) {
    FURI_LOG_I(TAG, "app_i2ctoolreader_alloc");
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = malloc(sizeof(AppI2CToolReader));
    appI2CToolReader->view = view_alloc();
    appI2CToolReader->popup = popup_alloc();
    popup_set_timeout(appI2CToolReader->popup, 1000);
    popup_enable_timeout(appI2CToolReader->popup);
    popup_set_context(appI2CToolReader->popup, app);
    popup_set_callback(appI2CToolReader->popup, app_i2ctoolreader_write_success_popup_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, AppViewI2CToolWriterPopup, popup_get_view(appI2CToolReader->popup));

    view_allocate_model(appI2CToolReader->view, ViewModelTypeLockFree, sizeof(i2CToolReaderModel));

    view_set_context(appI2CToolReader->view, app);
    view_set_draw_callback(appI2CToolReader->view, app_i2ctoolreader_render_callback);
    view_set_input_callback(appI2CToolReader->view, app_i2ctoolreader_input_callback);

    appI2CToolReader->InputReadOffset = byte_input_alloc();
    byte_input_set_header_text(appI2CToolReader->InputReadOffset, "Register:");
    byte_input_set_result_callback(
        appI2CToolReader->InputReadOffset,
        app_i2ctoolreader_input_readOffset_callback,
        NULL,
        app,
        &input_read_offset,
        1);

    appI2CToolReader->InputReadLen = number_input_alloc();
    number_input_set_header_text(appI2CToolReader->InputReadLen, "Data len: (max: 48)");
    number_input_set_result_callback(
        appI2CToolReader->InputReadLen, app_i2ctoolreader_input_readLen_callback, app, 1, 1, 48);

    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewI2CToolReaderByteInput,
        byte_input_get_view(appI2CToolReader->InputReadOffset));
    view_dispatcher_add_view(
        app->view_dispatcher,
        AppViewI2CToolReaderNumberInput,
        number_input_get_view(appI2CToolReader->InputReadLen));

    return appI2CToolReader;
}

/**
 * @brief Get the view associated with the app i2ctoolreader.
 *
 * @param appI2CToolReader The AppI2CToolReader for which the view is to be
 * fetched.
 * @return Returns a pointer to the View.
 */
View* app_i2ctoolreader_get_view(AppI2CToolReader* appI2CToolReader) {
    FURI_LOG_I(TAG, "app_i2ctoolreader_get_view");
    furi_assert(appI2CToolReader);
    return appI2CToolReader->view;
}

/**
 * @brief Callback when the app i2ctoolreader scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_i2ctoolreader_on_enter(void* ctx) {
    FURI_LOG_I(TAG, "app_scene_i2ctoolreader_on_enter");
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    with_view_model(
        appI2CToolReader->view,
        i2CToolReaderModel * model,
        {
            model->device = app->i2cToolDevice;
            model->data = NULL;
            model->data_offset = 0;
            model->data_len = 0;
        },
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReaderByteInput);
}

/**
 * @brief Handle scene manager events for the app i2ctoolreader scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_i2ctoolreader_on_event(void* ctx, SceneManagerEvent event) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) { // Back button pressed
        switch(event.event) {
        case I2CReaderEventExitPopup:
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReader);
            break;
        case I2CReaderEventByteInput:
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReaderNumberInput);
            break;
        case I2CReaderEventNumberInput:
            with_view_model(
                appI2CToolReader->view,
                i2CToolReaderModel * model,
                {
                    model->data = calloc(model->read_len, sizeof(uint8_t));
                    model->data_len = model->read_len;
                    model->data_offset = 0;
                    furi_hal_i2c_acquire(I2C_BUS);
                    bool res = false;
                    // Read the first 5 bytes from the EEPROM
                    FURI_LOG_I(TAG, "---- WRITE I2C: ----");
                    FURI_LOG_I(TAG, "- ADDR: %02x", model->device->addr << 1);
                    FURI_LOG_I(TAG, "- OFFSET: %02x", model->read_offset);
                    FURI_LOG_I(TAG, "- LEN: %02x", model->data_len);
                    res = furi_hal_i2c_read_mem(
                        I2C_BUS,
                        model->device->addr << 1,
                        model->read_offset,
                        model->data,
                        model->data_len,
                        I2C_MEM_I2C_TIMEOUT
                    );
                    furi_hal_i2c_release(I2C_BUS);
                    if(res == false) {
                        FURI_LOG_I(TAG, "Could not read mem");
                    }
                },
                false);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolReader);
            break;
        }
        return true;
    }
    return consumed;
}

/**
 * @brief Callback when the app i2ctoolreader scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_i2ctoolreader_on_exit(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;
    with_view_model(
        appI2CToolReader->view,
        i2CToolReaderModel * model,
        {
            free(model->data);
            model->data = NULL;
            model->data_offset = 0;
            model->data_len = 0;
        },
        false);
}

/**
 * @brief Free the memory occupied by the app i2ctoolreader.
 *
 * @param appI2CToolReader The AppI2CToolReader to be freed.
 */
void app_i2ctoolreader_free(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppI2CToolReader* appI2CToolReader = app->sceneI2CToolReader;

    view_dispatcher_remove_view(app->view_dispatcher, AppViewI2CToolWriterPopup);
    popup_free(appI2CToolReader->popup);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewI2CToolReaderByteInput);
    byte_input_free(appI2CToolReader->InputReadOffset);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewI2CToolReaderNumberInput);
    number_input_free(appI2CToolReader->InputReadLen);

    view_free_model(appI2CToolReader->view);
    view_free(appI2CToolReader->view);
    free(appI2CToolReader);
}
