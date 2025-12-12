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
    if(!target || !app || !app->last_uid_valid || app->last_uid_len == 0) {
        return;
    }

    furi_string_cat(target, "UID: ");
    for(size_t i = 0; i < app->last_uid_len; i++) {
        furi_string_cat_printf(target, "%02X", app->last_uid[i]);
        if(i + 1 < app->last_uid_len) {
            furi_string_push_back(target, ' ');
        }
    }
    furi_string_push_back(target, '\n');
}

static void ami_tool_info_append_field(FuriString* buffer, const char* label, const char* value) {
    if(!label || !value) {
        return;
    }
    furi_string_cat_printf(buffer, "%s: %s\n", label, value);
}

static void ami_tool_info_append_releases(FuriString* buffer, const char* releases) {
    if(!buffer || !releases || releases[0] == '\0') {
        return;
    }

    furi_string_cat(buffer, "Released on:\n");
    size_t len = strlen(releases);
    char* temp = malloc(len + 1);
    if(!temp) {
        furi_string_cat_printf(buffer, "  %s\n", releases);
        return;
    }
    memcpy(temp, releases, len + 1);

    char* cursor = temp;
    while(*cursor) {
        char* entry = cursor;
        char* comma = strchr(cursor, ',');
        if(comma) {
            *comma = '\0';
            cursor = comma + 1;
        } else {
            cursor += strlen(cursor);
        }

        if(entry[0] == '\0') continue;
        char* sep = strchr(entry, '.');
        if(sep) {
            *sep = '\0';
            const char* region = entry;
            const char* date = sep + 1;
            if(*date == '\0') date = "unknown";
            furi_string_cat_printf(buffer, "  - %s: %s\n", region, date);
        } else {
            furi_string_cat_printf(buffer, "  - %s\n", entry);
        }
    }

    free(temp);
}

static void ami_tool_info_format_entry(
    AmiToolApp* app,
    const char* id_hex,
    const char* entry_line,
    bool from_read) {
    furi_assert(app);
    UNUSED(from_read);
    furi_string_reset(app->text_box_store);

    if(!entry_line || entry_line[0] == '\0') {
        return;
    }

    const char* data = strchr(entry_line, ':');
    if(!data) {
        furi_string_set(app->text_box_store, entry_line);
        return;
    }
    data++; // skip ':'
    while(*data == ' ') data++;

    size_t data_len = strlen(data);
    char* temp = malloc(data_len + 1);
    if(!temp) {
        furi_string_set(app->text_box_store, data);
        return;
    }
    memcpy(temp, data, data_len + 1);

    const char* fields[6] = {0};
    size_t field_count = 0;
    char* cursor = temp;
    if(*cursor != '\0') {
        fields[field_count++] = cursor;
    }

    while(*cursor && field_count < 6) {
        if(*cursor == '|') {
            *cursor = '\0';
            cursor++;
            if(*cursor == '\0') break;
            fields[field_count++] = cursor;
        } else {
            cursor++;
        }
    }

    const char* name = (field_count > 0 && fields[0]) ? fields[0] : "Unknown";
    const char* character = (field_count > 1 && fields[1]) ? fields[1] : "Unknown";
    const char* series = (field_count > 2 && fields[2]) ? fields[2] : "Unknown";
    const char* game_series = (field_count > 3 && fields[3]) ? fields[3] : "Unknown";
    const char* type = (field_count > 4 && fields[4]) ? fields[4] : "Unknown";
    const char* releases = (field_count > 5 && fields[5]) ? fields[5] : "N/A";

    furi_string_cat_printf(app->text_box_store, "\e#%s\n\n", name);
    ami_tool_info_append_uid(app, app->text_box_store);
    if(id_hex && id_hex[0] != '\0') {
        ami_tool_info_append_field(app->text_box_store, "ID", id_hex);
    }
    ami_tool_info_append_field(app->text_box_store, "Character", character);
    ami_tool_info_append_field(app->text_box_store, "Amiibo Series", series);
    ami_tool_info_append_field(app->text_box_store, "Game Series", game_series);
    ami_tool_info_append_field(app->text_box_store, "Type", type);
    ami_tool_info_append_releases(app->text_box_store, releases);

    free(temp);
}

static void ami_tool_info_show_text_info(AmiToolApp* app, const char* message) {
    furi_string_set(app->text_box_store, message);
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
}

void ami_tool_info_show_page(AmiToolApp* app, const char* id_hex, bool from_read) {
    furi_assert(app);

    if(!app->tag_data || !app->tag_data_valid) {
        ami_tool_info_show_text_info(
            app,
            "No Amiibo dump available.\nRead a tag or generate one, then try again.");
        return;
    }

    FuriString* entry = furi_string_alloc();
    bool asset_error = false;
    bool found = ami_tool_info_lookup_entry(app, id_hex, entry, &asset_error);

    if(found) {
        ami_tool_info_format_entry(
            app, id_hex ? id_hex : "", furi_string_get_cstr(entry), from_read);
        widget_reset(app->info_widget);
        widget_add_text_scroll_element(
            app->info_widget, 2, 0, 124, 60, furi_string_get_cstr(app->text_box_store));
        view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewInfo);
    } else {
        FuriString* fallback = furi_string_alloc();
        if(from_read) {
            furi_string_cat(fallback, "Valid NTAG215 detected.\n");
            if(id_hex && id_hex[0] != '\0') {
                furi_string_cat_printf(fallback, "ID: %s\n", id_hex);
            }
            if(asset_error) {
                furi_string_cat(
                    fallback, "\nUnable to open amiibo.dat.\nInstall or update the assets.");
            } else {
                furi_string_cat(
                    fallback,
                    "\nNo matching entry found.\nTag is unlikely to be an official Amiibo.");
            }
        } else {
            if(asset_error) {
                furi_string_cat(
                    fallback,
                    "Unable to open amiibo.dat.\nInstall or update the assets.");
            } else if(id_hex && id_hex[0] != '\0') {
                furi_string_cat_printf(
                    fallback, "Selected Amiibo ID:\n%s\nNo matching entry found.", id_hex);
            } else {
                furi_string_cat(
                    fallback,
                    "No Amiibo ID provided.\nUnable to display details.");
            }
        }
        ami_tool_info_append_uid(app, fallback);
        ami_tool_info_show_text_info(app, furi_string_get_cstr(fallback));
        furi_string_free(fallback);
    }

    furi_string_free(entry);
}

void ami_tool_store_uid(AmiToolApp* app, const uint8_t* uid, size_t len) {
    if(!app) return;
    if(!uid || len == 0) {
        memset(app->last_uid, 0, sizeof(app->last_uid));
        app->last_uid_len = 0;
        app->last_uid_valid = false;
        return;
    }
    if(len > sizeof(app->last_uid)) {
        len = sizeof(app->last_uid);
    }
    memcpy(app->last_uid, uid, len);
    app->last_uid_len = len;
    app->last_uid_valid = true;
}

void ami_tool_clear_cached_tag(AmiToolApp* app) {
    if(!app) return;
    app->tag_data_valid = false;
    if(app->tag_data) {
        mf_ultralight_reset(app->tag_data);
    }
    app->tag_password_valid = false;
    memset(&app->tag_password, 0, sizeof(app->tag_password));
}
