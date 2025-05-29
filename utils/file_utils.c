#include <furi.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include "png_write.h"
#include "pngle.h"

#include "file_utils.h"

uint8_t* convert_bytes_to_xbm_bits(IEIcon* icon, size_t* size) {
    // FURI_LOG_I("XBM", "pixels: %d", icon->height * icon->width);
    *size = ceil(icon->width / 8.0) * icon->height;
    // FURI_LOG_I("XBM", "packed bytes: %d", *size);
    uint8_t* c = malloc(*size);
    memset(c, 0, *size);
    size_t byte = 0;
    for(size_t y = 0; y < icon->height; ++y) {
        for(size_t x = 0; x < icon->width; ++x) {
            size_t b = y * icon->width + x;
            c[byte] |= icon->data[b] << (x % 8);
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
    // for(size_t i = 0; i < size; ++i) {
    //     furi_string_cat_printf(tmp, " %02x", data[i]);
    //     if(i % 8 == 0) {
    //         furi_string_cat_str(tmp, " ");
    //     }
    //     if(i % 16 == 0) {
    //         FURI_LOG_I("XBM", furi_string_get_cstr(tmp));
    //         furi_string_set_str(tmp, "");
    //     }
    // }
    if(furi_string_size(tmp)) {
        FURI_LOG_I("XBM", furi_string_get_cstr(tmp));
    }
    furi_string_free(tmp);
}

// Saves the icon in XBM format that is directly usable by Flipper
// This differs from XBM in that all Flipper icons are multi-frame and thus we're
// only saving frame 0
// TODO: Make this support multi-frame icons/images
bool c_file_save(IEIcon* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FuriString* filename =
        furi_string_alloc_printf("/data/%s.c", furi_string_get_cstr(icon->name));

    File* c_file = storage_file_alloc(storage);
    bool success = false;
    if(storage_file_open(c_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* name = furi_string_get_cstr(icon->name);

        FuriString* tmp = furi_string_alloc();

        // frame 0 data start
        furi_string_printf(tmp, "const uint8_t _I_%s_0[] = {\n", name);
        FURI_LOG_I("C", "%s", furi_string_get_cstr(tmp));
        storage_file_write(c_file, furi_string_get_cstr(tmp), furi_string_size(tmp));
        // raw data
        furi_string_set_str(tmp, "");
        size_t size;
        uint8_t* data = convert_bytes_to_xbm_bits(icon, &size);
        for(size_t i = 0; i < size; ++i) {
            furi_string_cat_printf(tmp, " 0x%02x", data[i]);
            if(i < size - 1) {
                furi_string_cat_str(tmp, ",");
            }
        }
        FURI_LOG_I("C", "%s", furi_string_get_cstr(tmp));
        storage_file_write(c_file, furi_string_get_cstr(tmp), furi_string_size(tmp));
        free(data);
        // frame 0 data end
        furi_string_printf(tmp, "};\n");
        FURI_LOG_I("C", "%s", furi_string_get_cstr(tmp));
        storage_file_write(c_file, furi_string_get_cstr(tmp), furi_string_size(tmp));
        // frame list
        furi_string_printf(tmp, "const uint8_t* const _I_%s[] = {_I_%s_0};\n", name, name);
        FURI_LOG_I("C", "%s", furi_string_get_cstr(tmp));
        storage_file_write(c_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

        // main definition
        furi_string_printf(
            tmp,
            "const Icon I_%s = {.width=%d,.height=%d,.frame_count=1,.frame_rate=0,.frames=_I_%s};\n",
            name,
            icon->width,
            icon->height,
            name);
        FURI_LOG_I("C", "%s", furi_string_get_cstr(tmp));
        storage_file_write(c_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

        furi_string_free(tmp);

        storage_file_close(c_file);
        storage_file_free(c_file);

        success = true;
    } else {
        FURI_LOG_E("IE", "Couldn't save xbm file");
    }

    furi_string_free(filename);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Saves the icon in a "pure" XBM format
bool xbm_file_save(IEIcon* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FuriString* filename =
        furi_string_alloc_printf("/data/%s.xbm", furi_string_get_cstr(icon->name));

    File* xbm_file = storage_file_alloc(storage);
    bool success = false;
    if(storage_file_open(
           xbm_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* name = furi_string_get_cstr(icon->name);

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
        uint8_t* data = convert_bytes_to_xbm_bits(icon, &size);
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

        success = true;
    } else {
        FURI_LOG_E("IE", "Couldn't save xbm file");
    }

    furi_string_free(filename);
    furi_record_close(RECORD_STORAGE);
    return success;
}

bool png_file_save(IEIcon* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FuriString* filename =
        furi_string_alloc_printf("/data/%s.png", furi_string_get_cstr(icon->name));
    FURI_LOG_I("PNG", "Saving %s", furi_string_get_cstr(filename));
    File* png_file = storage_file_alloc(storage);
    bool success = false;
    if(storage_file_open(
           png_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        image_t* image = image_new(icon->width, icon->height);
        for(size_t i = 0; i < icon->width * icon->height; i++) {
            bool bit_on = icon->data[i];
            if(bit_on) {
                image->pixels[i] = (rgba_t){.r = 0, .g = 0, .b = 0, .a = 255};
            } else {
                image->pixels[i] = (rgba_t){.r = 255, .g = 255, .b = 255, .a = 0};
            }
        }
        image->file = png_file;
        image_save(image);
        image_free(image);

        success = true;
    } else {
        FURI_LOG_E("IE", "Couldn't save png file");
    }
    storage_file_close(png_file);
    storage_file_free(png_file);

    furi_string_free(filename);
    furi_record_close(RECORD_STORAGE);
    return success;
}

void png_open_on_draw(
    pngle_t* pngle,
    uint32_t x,
    uint32_t y,
    uint32_t w,
    uint32_t h,
    uint8_t rgba[4]) {
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
        icon->data[y * pngle_get_width(pngle) + x] = 0x1;
    } else {
        icon->data[y * pngle_get_width(pngle) + x] = 0x0;
    }
}
IEIcon* png_file_open(const char* icon_path) {
    FURI_LOG_I("PNG", "png_file_open: %s", icon_path);
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* png_file = storage_file_alloc(storage);

    IEIcon* icon = icon_alloc();
    bool success = false;
    do {
        if(!storage_file_open(png_file, icon_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E("PNG", "Failed to open file!");
            FURI_LOG_E("PNG", "Error: %s", storage_file_get_error_desc(png_file));
            break;
        }

        pngle_t* png = pngle_new();
        pngle_set_draw_callback(png, png_open_on_draw);
        pngle_set_user_data(png, icon);

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
        icon_free(icon);
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
