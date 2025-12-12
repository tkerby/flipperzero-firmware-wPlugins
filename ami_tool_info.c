#include "ami_tool_i.h"
#include <ctype.h>
#include <stdlib.h>

#define AMI_TOOL_INFO_READ_BUFFER 96

static bool ami_tool_info_lookup_entry(
    AmiToolApp* app,
    const char* id_hex,
    FuriString* result,
    bool* asset_error) {
    furi_assert(app);
    furi_assert(result);
    if(asset_error) {
        *asset_error = false;
    }

    if(!app->storage || !id_hex || id_hex[0] == '\0') {
        if(asset_error && !app->storage) {
            *asset_error = true;
        }
        return false;
    }

    const size_t id_len = strlen(id_hex);
    if(id_len == 0) {
        return false;
    }

    char* id_lower = malloc(id_len + 1);
    if(!id_lower) {
        return false;
    }
    for(size_t i = 0; i < id_len; i++) {
        id_lower[i] = (char)tolower((unsigned char)id_hex[i]);
    }
    id_lower[id_len] = '\0';

    bool found = false;
    File* file = storage_file_alloc(app->storage);
    if(!file) {
        if(asset_error) {
            *asset_error = true;
        }
        free(id_lower);
        return false;
    }

    FuriString* line = furi_string_alloc();

    if(storage_file_open(file, APP_ASSETS_PATH("amiibo.dat"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        bool in_data_section = false;
        uint8_t buffer[AMI_TOOL_INFO_READ_BUFFER];

        while(true) {
            size_t read = storage_file_read(file, buffer, sizeof(buffer));
            if(read == 0) break;

            for(size_t i = 0; i < read; i++) {
                char ch = (char)buffer[i];
                if(ch == '\r') continue;

                if(ch == '\n') {
                    if(in_data_section && !furi_string_empty(line)) {
                        const char* raw = furi_string_get_cstr(line);
                        if(strncmp(raw, id_lower, id_len) == 0 && raw[id_len] == ':') {
                            furi_string_set(result, line);
                            found = true;
                            break;
                        }
                    } else if(furi_string_empty(line)) {
                        in_data_section = true;
                    }
                    furi_string_reset(line);
                } else {
                    furi_string_push_back(line, ch);
                }
            }

            if(found) break;
        }

        if(!found && in_data_section && !furi_string_empty(line)) {
            const char* raw = furi_string_get_cstr(line);
            if(strncmp(raw, id_lower, id_len) == 0 && raw[id_len] == ':') {
                furi_string_set(result, line);
                found = true;
            }
        }

        storage_file_close(file);
    } else {
        if(asset_error) {
            *asset_error = true;
        }
    }

    furi_string_free(line);
    storage_file_free(file);
    free(id_lower);
    return found;
}

static void ami_tool_info_append_uid(AmiToolApp* app, FuriString* target) {
    if(app->read_result.uid_len == 0) {
        return;
    }

    furi_string_cat(target, "UID:");
    for(size_t i = 0; i < app->read_result.uid_len; i++) {
        furi_string_cat_printf(target, " %02X", app->read_result.uid[i]);
    }
    furi_string_push_back(target, '\n');
}

void ami_tool_info_show_page(AmiToolApp* app, const char* id_hex, bool from_read) {
    furi_assert(app);

    FuriString* entry = furi_string_alloc();
    bool asset_error = false;
    bool found = ami_tool_info_lookup_entry(app, id_hex, entry, &asset_error);

    furi_string_reset(app->text_box_store);

    if(found) {
        furi_string_printf(
            app->text_box_store,
            "Amiibo identified:\n%s\n\nPress Back to continue.",
            furi_string_get_cstr(entry));
    } else {
        if(from_read) {
            furi_string_cat(app->text_box_store, "Valid NTAG215 detected.\n");
            if(id_hex && id_hex[0] != '\0') {
                furi_string_cat_printf(app->text_box_store, "ID: %s\n", id_hex);
            }
            ami_tool_info_append_uid(app, app->text_box_store);
            if(asset_error) {
                furi_string_cat(
                    app->text_box_store,
                    "\nUnable to open amiibo.dat.\nInstall or update the assets package.");
            } else {
                furi_string_cat(
                    app->text_box_store,
                    "\nNo matching entry found in amiibo.dat.\nTag is unlikely to be an official Amiibo.");
            }
        } else {
            if(asset_error) {
                furi_string_cat(
                    app->text_box_store,
                    "Unable to open amiibo.dat.\nInstall or update the assets package.");
            } else if(id_hex && id_hex[0] != '\0') {
                furi_string_cat_printf(
                    app->text_box_store,
                    "Selected Amiibo ID:\n%s\n\nNo matching entry found in amiibo.dat.",
                    id_hex);
            } else {
                furi_string_cat(
                    app->text_box_store,
                    "No Amiibo ID provided.\nUnable to display details.");
            }
        }
        furi_string_cat(app->text_box_store, "\n\nPress Back to continue.");
    }

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);

    furi_string_free(entry);
}
