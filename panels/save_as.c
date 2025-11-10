#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/file_utils.h"
#include "../utils/draw.h"

typedef enum {
    Save_START,
    Save_PNG = Save_START,
    Save_C,
#ifdef ENABLE_XBM_SAVE
    Save_XBM,
#endif
    Save_BMX, // used for Asset Packs
    Save_COUNT,
    Save_NONE
} SaveMode;

const char* SaveModeStr[Save_COUNT] = {
    "PNG",
    ".C",
#ifdef ENABLE_XBM_SAVE
    "XBM",
#endif
    "BMX",
};
const SaveMode Save_UP[Save_COUNT] = {
    Save_NONE,
    Save_PNG,
    Save_C,
#ifdef ENABLE_XBM_SAVE
    Save_XBM,
#endif
};
const SaveMode Save_DOWN[Save_COUNT] = {
    Save_C,
#ifdef ENABLE_XBM_SAVE
    Save_XBM,
#endif
    Save_BMX,
    Save_NONE};

static struct {
    SaveMode save_mode;
} saveModel = {.save_mode = Save_PNG};

void save_as_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    int pad = 2; // outside padding between frame and item
    int elem_h = 7;
    int elem_w = 22;
    int elem_pad = 1; // the space between

    // draw an empty panel frame
    int rw = elem_w + (pad * 2) + (elem_pad * 2);
    int rh = (elem_h + (elem_pad * 2)) * Save_COUNT + (pad * 2);
    int x = (canvas_width(canvas) - rw) / 2;
    int y = (canvas_height(canvas) - rh) / 2;
    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    // draw save menu items
    for(SaveMode mode = Save_START; mode < Save_COUNT; mode++) {
        canvas_draw_str_aligned(
            canvas,
            x + pad + elem_pad + 1,
            y + pad + elem_pad + mode * (elem_h + elem_pad * 2),
            AlignLeft,
            AlignTop,
            SaveModeStr[mode]);
    }
    // hightlight the selected item
    canvas_set_color(canvas, ColorXOR);
    canvas_draw_rbox(
        canvas,
        x + pad,
        y + pad + (elem_h + (elem_pad * 2)) * saveModel.save_mode,
        elem_w + elem_pad * 2,
        elem_h + elem_pad * 2,
        0);
    canvas_set_color(canvas, ColorBlack);
}

bool save_as_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            SaveMode next_save = Save_UP[saveModel.save_mode];
            if(next_save != Save_NONE) {
                saveModel.save_mode = next_save;
            }
            break;
        }
        case InputKeyDown: {
            SaveMode next_save = Save_DOWN[saveModel.save_mode];
            if(next_save != Save_NONE) {
                saveModel.save_mode = next_save;
            }
            break;
        }
        case InputKeyBack: {
            consumed = false; // send control back to File panel
            app->panel = Panel_File;
            break;
        }
        case InputKeyOk: {
            bool save_successful = false;
            switch(saveModel.save_mode) {
            case Save_PNG: {
                save_successful = png_file_save(app->icon);
                break;
            }
            case Save_BMX: {
                save_successful = bmx_file_save(app->icon);
                break;
            }
            case Save_C: {
                save_successful = c_file_save(app->icon);
                break;
            }
            case Save_XBM: {
                save_successful = xbm_file_save(app->icon);
                break;
            }
            default:
                break;
            }
            if(save_successful) {
                dialog_info_dialog(app, "File saved!", Panel_File);
                app->dirty = false;
            } else {
                dialog_info_dialog(app, "Failed to save!", Panel_File);
            }
            consumed = false;
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}
