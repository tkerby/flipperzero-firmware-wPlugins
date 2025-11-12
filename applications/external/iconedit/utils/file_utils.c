#include <furi.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include "png_write.h"
#include "pngle.h"

#include "file_utils.h"

// Determines filename format string for single vs multiple frames
const char* get_filename_format_str(IEIcon* icon) {
    const char* const single_fn_fmt = "/data/%s";
    const char* const multi_fn_fmt = "/data/%s_%0*d";
    bool multi_frame = icon->frame_count > 1;
    return multi_frame ? multi_fn_fmt : single_fn_fmt;
}

// Returns the num digits of the frame count, used for printf fmt string
int get_frame_num_fmt_width(IEIcon* icon) {
    return (int)floor(log10(icon->frame_count)) + 1;
}

// *******************************************************************************
// * XBM
// *******************************************************************************
// creates a packed XBM binary buffer
uint8_t* convert_bytes_to_xbm_bits(IEIcon* icon, size_t* size, size_t frame) {
    *size = ceil(icon->width / 8.0) * icon->height;
    uint8_t* c = malloc(*size);
    memset(c, 0, *size);
    size_t byte = 0;
    Frame* f = ie_icon_get_frame(icon, frame);
    for(size_t y = 0; y < icon->height; ++y) {
        for(size_t x = 0; x < icon->width; ++x) {
            size_t b = y * icon->width + x;
            c[byte] |= f->data[b] << (x % 8);
            if(x != 0 && (x + 1) % 8 == 0) {
                byte++;
            }
        }
        byte++;
    }
    return c;
}

// Write the XBM bytes to the log for debugging
void log_xbm_data(uint8_t* data, size_t size) {
    FuriString* tmp = furi_string_alloc_set_str("");
    size_t i = 0;
    while(i < size) {
        furi_string_cat_printf(tmp, " %02x", data[i]);
        ++i;
        if(i % 8 == 0) {
            furi_string_cat_str(tmp, " ");
        }
        if(i % 16 == 0) {
            FURI_LOG_I("XBM", furi_string_get_cstr(tmp));
            furi_string_set_str(tmp, "");
        }
    }
    if(furi_string_size(tmp)) {
        FURI_LOG_I("XBM", furi_string_get_cstr(tmp));
    }
    furi_string_free(tmp);
}

// Saves the icon in a "pure" XBM format (Not really sure if this useful :|)
bool xbm_file_save(IEIcon* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    const char* fn_fmt = get_filename_format_str(icon);
    int pad_width = get_frame_num_fmt_width(icon);

    bool success = true;
    for(size_t f = 0; f < icon->frame_count; f++) {
        FuriString* filename =
            furi_string_alloc_printf(fn_fmt, furi_string_get_cstr(icon->name), pad_width, f);
        FuriString* base_name = furi_string_alloc_set(filename);
        furi_string_cat_str(filename, ".xbm");

        File* xbm_file = storage_file_alloc(storage);
        if(storage_file_open(
               xbm_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* name = furi_string_get_cstr(base_name);

            FuriString* tmp = furi_string_alloc();
            // dimensions
            furi_string_printf(tmp, "#define %s_width %d\n", name, icon->width);
            FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));
            furi_string_printf(tmp, "#define %s_height %d\n", name, icon->height);
            FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            // prefix
            furi_string_printf(tmp, "static unsigned char %s_bits[] = {\n", name);
            FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            // raw data
            furi_string_set_str(tmp, "");
            size_t size;
            uint8_t* data = convert_bytes_to_xbm_bits(icon, &size, f);
            for(size_t i = 0; i < size; ++i) {
                furi_string_cat_printf(tmp, "0x%02x", data[i]);
                if(i < size - 1) {
                    furi_string_cat_str(tmp, ",");
                }
            }
            FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            // suffix
            furi_string_set_str(tmp, " };");
            FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            free(data);

            furi_string_free(tmp);

            storage_file_close(xbm_file);
            storage_file_free(xbm_file);

        } else {
            success = false;
            FURI_LOG_E(
                "IE", "Failed to open file for saving: %s", storage_file_get_error_desc(xbm_file));
        }

        furi_string_free(filename);
    }
    furi_record_close(RECORD_STORAGE);
    return success;
}

// *******************************************************************************
// * .C source code
// *******************************************************************************

// Generates the single .C file (whether single frame or multi frame) as one
// Furi string. Used by both C file saving and USB send.
FuriString* c_file_generate(IEIcon* icon) {
    const char* name = furi_string_get_cstr(icon->name);

    FuriString* tmp = furi_string_alloc();

    int pad_width = get_frame_num_fmt_width(icon);

    // Include the Icon header
    furi_string_cat_printf(tmp, "#include <gui/icon_i.h>\n");

    // Generate each frame variable
    for(size_t f = 0; f < icon->frame_count; f++) {
        // frame 0 data start
        furi_string_cat_printf(tmp, "const uint8_t _I_%s_%0*d[] = {", name, pad_width, f);
        // raw data
        size_t size;
        uint8_t* data = convert_bytes_to_xbm_bits(icon, &size, f);
        for(size_t i = 0; i < size; ++i) {
            furi_string_cat_printf(tmp, " 0x%02x", data[i]);
            if(i < size - 1) {
                furi_string_cat_str(tmp, ",");
            }
        }
        free(data);
        furi_string_cat_printf(tmp, "};\n");
    }

    // Generate the frame list
    furi_string_cat_printf(tmp, "const uint8_t* const _I_%s[] = {", name);
    for(size_t f = 0; f < icon->frame_count; f++) {
        furi_string_cat_printf(tmp, "_I_%s_%0*d", name, pad_width, f);
        if(f < icon->frame_count - 1) {
            furi_string_cat_printf(tmp, ",");
        }
    }
    furi_string_cat_printf(tmp, "};\n");

    // Generate the main icon definition
    furi_string_cat_printf(
        tmp,
        "const Icon I_%s = {.width=%d,.height=%d,.frame_count=%d,.frame_rate=%d,.frames=_I_%s};\n",
        name,
        icon->width,
        icon->height,
        icon->frame_count,
        icon->frame_rate,
        name);
    // FURI_LOG_I("C", "\n\n%s\n", furi_string_get_cstr(tmp));
    return tmp;
}

// Saves the icon in XBM format, as a .C file, that is directly usable by
// Flipper's canvas_draw_icon This differs from true XBM in that all Flipper
// icons are multi-frame capable
bool c_file_save(IEIcon* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    const char* fn_fmt = "/data/%s.c";
    bool success = true;
    FuriString* filename = furi_string_alloc_printf(fn_fmt, furi_string_get_cstr(icon->name));

    File* c_file = storage_file_alloc(storage);
    if(storage_file_open(c_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* tmp = c_file_generate(icon);
        storage_file_write(c_file, furi_string_get_cstr(tmp), furi_string_size(tmp));
        FURI_LOG_I("C", "%s", furi_string_get_cstr(tmp));
        furi_string_free(tmp);

        storage_file_close(c_file);
        storage_file_free(c_file);

    } else {
        success = false;
        FURI_LOG_E("IE", "Couldn't save C file");
    }

    furi_string_free(filename);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// *******************************************************************************
// * PNG
// *******************************************************************************

// Generates PNG byte buffer of current frame with zero compression
bool png_file_generate_binary(IEIcon* icon, uint8_t** png_file_data, size_t* png_file_size) {
    bool success = true;
    Frame* frame = ie_icon_get_current_frame(icon);
    image_t* image = image_new(icon->width, icon->height);
    for(size_t i = 0; i < icon->width * icon->height; i++) {
        bool bit_on = frame->data[i];
        if(bit_on) {
            image->pixels[i] = (rgba_t){.r = 0, .g = 0, .b = 0, .a = 255};
        } else {
            image->pixels[i] = (rgba_t){.r = 255, .g = 255, .b = 255, .a = 0};
        }
    }
    image->save_to_bytes = true; // tell png_write to save as byte buffer
    *png_file_data = malloc(1024 * sizeof(uint8_t));
    image->bytes = *png_file_data;
    image->bytes_len = 1024;
    image_save(image);
    *png_file_data = image->bytes; // need to reassign in case we realloc'd
    *png_file_size = image->size;

    image_free(image);
    return success;
}

// Saves to one or more PNG files with zero compression
bool png_file_save(IEIcon* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    const char* fn_fmt = get_filename_format_str(icon);
    int pad_width = get_frame_num_fmt_width(icon);

    bool success = true;
    for(size_t f = 0; f < icon->frame_count; f++) {
        FuriString* filename =
            furi_string_alloc_printf(fn_fmt, furi_string_get_cstr(icon->name), pad_width, f);
        furi_string_cat_str(filename, ".png");
        FURI_LOG_I("PNG", "Saving %s", furi_string_get_cstr(filename));
        File* png_file = storage_file_alloc(storage);
        Frame* frame = ie_icon_get_frame(icon, f);
        if(storage_file_open(
               png_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            image_t* image = image_new(icon->width, icon->height);
            for(size_t i = 0; i < icon->width * icon->height; i++) {
                bool bit_on = frame->data[i];
                if(bit_on) {
                    image->pixels[i] = (rgba_t){.r = 0, .g = 0, .b = 0, .a = 255};
                } else {
                    image->pixels[i] = (rgba_t){.r = 255, .g = 255, .b = 255, .a = 0};
                }
            }
            image->file = png_file;
            image_save(image);
            image_free(image);

        } else {
            success = false;
            FURI_LOG_E("IE", "Couldn't save png file");
        }
        storage_file_close(png_file);
        storage_file_free(png_file);
        furi_string_free(filename);
    }
    furi_record_close(RECORD_STORAGE);
    return success;
}

void png_init_cb(pngle_t* pngle, uint32_t width, uint32_t height) {
    IEIcon* icon = pngle_get_user_data(pngle);
    assert(icon->data == NULL);
    assert(width > 0);
    assert(height > 0);
    ie_icon_reset(icon, width, height);
}

void png_draw_cb(pngle_t* pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
    UNUSED(pngle);
    UNUSED(w);
    UNUSED(h);
    IEIcon* icon = pngle_get_user_data(pngle);
    // FURI_LOG_W(
    //     "png",
    //     "on_draw: w,h = %ld x %ld, x,y = %ld, %ld = %d %d %d",
    //     pngle_get_width(pngle),
    //     pngle_get_height(pngle),
    //     x,
    //     y,
    //     rgba[0],
    //     rgba[1],
    //     rgba[2]);
    if(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0) {
        // FURI_LOG_W("png", "setting pixel: %ld, %ld", x, y);
        icon->frames->data[y * pngle_get_width(pngle) + x] = 0x1;
    } else {
        icon->frames->data[y * pngle_get_width(pngle) + x] = 0x0;
    }
}
IEIcon* png_file_open(const char* icon_path) {
    FURI_LOG_I("PNG", "png_file_open: %s", icon_path);
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* png_file = storage_file_alloc(storage);

    IEIcon* icon = ie_icon_alloc(false);
    bool success = false;
    do {
        if(!storage_file_open(png_file, icon_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E("PNG", "Failed to open file!");
            FURI_LOG_E("PNG", "Error: %s", storage_file_get_error_desc(png_file));
            break;
        }

        pngle_t* png = pngle_new();
        pngle_set_user_data(png, icon);
        pngle_set_init_callback(png, png_init_cb);
        pngle_set_draw_callback(png, png_draw_cb);

        char buffer[64] = {0};
        size_t remain = 0;
        size_t len;
        while((len = storage_file_read(png_file, buffer + remain, sizeof(buffer) - remain)) > 0) {
            int fed = pngle_feed(png, buffer, remain + len);
            if(fed < 0) {
                FURI_LOG_E("PNG", "pngle error: %s", pngle_error(png));
            }
            remain = remain + len - fed;
            if(remain > 0) {
                memmove(buffer, buffer + fed, remain);
            }
        }

        pngle_destroy(png);
        success = true;
    } while(false);

    if(!success) {
        FURI_LOG_E("PNG", "Couldn't open png file");
        ie_icon_free(icon);
        icon = NULL;
    } else {
        path_extract_filename_no_ext(icon_path, icon->name);

        FURI_LOG_I("PNG", "Opened png file?");
    }

    storage_file_close(png_file);
    storage_file_free(png_file);

    furi_record_close(RECORD_STORAGE);
    return icon;
}

// *******************************************************************************
// * Generator methods
// *******************************************************************************

FuriString* generate_text_from_binary(uint8_t* data, size_t size) {
    const int bytes_per_line = 64;
    FuriString* text = furi_string_alloc();
    for(size_t i = 0; i < size; ++i) {
        furi_string_cat_printf(text, "%02x", data[i]);
        if((i + 1) % bytes_per_line == 0) {
            furi_string_cat_str(text, "\n");
        }
    }
    if(size % bytes_per_line != 0) {
        furi_string_cat_str(text, "\n");
    }
    return text;
}

#define TEXT_FILE_GENERATOR(TYPE)                                 \
    FuriString* TYPE##_file_generate(IEIcon* icon) {              \
        uint8_t* data;                                            \
        size_t size;                                              \
        TYPE##_file_generate_binary(icon, &data, &size);          \
        FuriString* text = generate_text_from_binary(data, size); \
        free(data);                                               \
        return text;                                              \
    }

TEXT_FILE_GENERATOR(png)
