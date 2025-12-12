#include "ami_tool_i.h"
#include <stdlib.h>
#include <string.h>

#define AMI_TOOL_GENERATE_READ_BUFFER 64

typedef enum {
    AmiToolGenerateRootMenuIndexByName,
    AmiToolGenerateRootMenuIndexByGames,
} AmiToolGenerateRootMenuIndex;

typedef bool (*AmiToolGenerateLineCallback)(void* context, size_t index, FuriString* line);

static bool ami_tool_scene_generate_iterate_lines(
    AmiToolApp* app,
    const char* path,
    bool skip_header,
    AmiToolGenerateLineCallback callback,
    void* context);
static bool ami_tool_scene_generate_iterate_games(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    AmiToolGenerateLineCallback callback,
    void* context);
static bool ami_tool_scene_generate_find_mapping_for_game(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    const FuriString* game_name,
    FuriString* out_ids);
static bool ami_tool_scene_generate_prepare_amiibo_entries(
    AmiToolApp* app,
    const FuriString* game_name);
static void ami_tool_scene_generate_show_amiibo_menu(AmiToolApp* app, size_t game_index);
static void ami_tool_scene_generate_show_amiibo_placeholder(AmiToolApp* app, size_t amiibo_index);
static void ami_tool_scene_generate_clear_selected_game(AmiToolApp* app);

static void ami_tool_scene_generate_clear_selected_game(AmiToolApp* app) {
    if(app->generate_selected_game) {
        furi_string_reset(app->generate_selected_game);
    }
}

void ami_tool_generate_clear_amiibo_cache(AmiToolApp* app) {
    if(!app) return;

    if(app->generate_amiibo_names) {
        for(size_t i = 0; i < app->generate_amiibo_count; i++) {
            if(app->generate_amiibo_names[i]) {
                furi_string_free(app->generate_amiibo_names[i]);
            }
        }
        free(app->generate_amiibo_names);
        app->generate_amiibo_names = NULL;
    }

    if(app->generate_amiibo_ids) {
        for(size_t i = 0; i < app->generate_amiibo_count; i++) {
            if(app->generate_amiibo_ids[i]) {
                furi_string_free(app->generate_amiibo_ids[i]);
            }
        }
        free(app->generate_amiibo_ids);
        app->generate_amiibo_ids = NULL;
    }

    app->generate_amiibo_count = 0;
}

static void ami_tool_scene_generate_submenu_callback(void* context, uint32_t index);
static void ami_tool_scene_generate_show_root_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_platform_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_games_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_name_placeholder(AmiToolApp* app);
static void ami_tool_scene_generate_commit_text_view(
    AmiToolApp* app,
    AmiToolGenerateState state,
    AmiToolGenerateState return_state);
static bool ami_tool_scene_generate_get_game_name(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    size_t target_index,
    FuriString* out);
static const char* ami_tool_scene_generate_platform_label(AmiToolGeneratePlatform platform);
static const char* ami_tool_scene_generate_platform_mapping_path(AmiToolGeneratePlatform platform);
static void ami_tool_scene_generate_return_to_state(AmiToolApp* app, AmiToolGenerateState state);

typedef struct {
    AmiToolApp* app;
    size_t count;
} AmiToolGenerateListBuildContext;

static bool ami_tool_scene_generate_add_game_callback(
    void* context,
    size_t index,
    FuriString* name) {
    AmiToolGenerateListBuildContext* ctx = context;
    submenu_add_item(
        ctx->app->submenu,
        furi_string_get_cstr(name),
        index,
        ami_tool_scene_generate_submenu_callback,
        ctx->app);
    ctx->count = index + 1;
    return true;
}

typedef struct {
    size_t target_index;
    FuriString* result;
    bool found;
} AmiToolGenerateFindGameContext;

static bool ami_tool_scene_generate_find_game_callback(
    void* context,
    size_t index,
    FuriString* name) {
    AmiToolGenerateFindGameContext* find_ctx = context;
    if(index == find_ctx->target_index) {
        furi_string_set(find_ctx->result, name);
        find_ctx->found = true;
        return false;
    }
    return true;
}

static void ami_tool_scene_generate_show_root_menu(AmiToolApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Generate Amiibo");
    submenu_add_item(
        app->submenu,
        "Select by Name",
        AmiToolGenerateRootMenuIndexByName,
        ami_tool_scene_generate_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Select by Games",
        AmiToolGenerateRootMenuIndexByGames,
        ami_tool_scene_generate_submenu_callback,
        app);
    ami_tool_generate_clear_amiibo_cache(app);
    ami_tool_scene_generate_clear_selected_game(app);
    app->generate_state = AmiToolGenerateStateRootMenu;
    app->generate_return_state = AmiToolGenerateStateRootMenu;
    app->generate_game_count = 0;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

static void ami_tool_scene_generate_show_platform_menu(AmiToolApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Choose Platform");
    for(size_t platform = 0; platform < AmiToolGeneratePlatformCount; platform++) {
        submenu_add_item(
            app->submenu,
            ami_tool_scene_generate_platform_label((AmiToolGeneratePlatform)platform),
            platform,
            ami_tool_scene_generate_submenu_callback,
            app);
    }
    app->generate_state = AmiToolGenerateStatePlatformMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

static void ami_tool_scene_generate_show_games_menu(AmiToolApp* app) {
    submenu_reset(app->submenu);
    ami_tool_generate_clear_amiibo_cache(app);
    ami_tool_scene_generate_clear_selected_game(app);
    furi_string_printf(
        app->text_box_store, "%s Games", ami_tool_scene_generate_platform_label(app->generate_platform));
    submenu_set_header(app->submenu, furi_string_get_cstr(app->text_box_store));

    AmiToolGenerateListBuildContext ctx = {
        .app = app,
        .count = 0,
    };

    bool file_ok = ami_tool_scene_generate_iterate_games(
        app, app->generate_platform, ami_tool_scene_generate_add_game_callback, &ctx);

    if(!file_ok) {
        furi_string_printf(
            app->text_box_store,
            "Unable to read the %s game list.\n\nEnsure the assets folder is installed.",
            ami_tool_scene_generate_platform_label(app->generate_platform));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStatePlatformMenu);
        return;
    }

    app->generate_game_count = ctx.count;
    if(app->generate_game_count == 0) {
        furi_string_printf(
            app->text_box_store,
            "No games found for %s.\n\nUpdate or regenerate your assets.",
            ami_tool_scene_generate_platform_label(app->generate_platform));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStatePlatformMenu);
        return;
    }

    app->generate_state = AmiToolGenerateStateGameList;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

static void ami_tool_scene_generate_show_name_placeholder(AmiToolApp* app) {
    furi_string_printf(
        app->text_box_store,
        "Generate by Name\n\nSelecting an Amiibo by name will be available in a future update.");
    ami_tool_scene_generate_commit_text_view(
        app, AmiToolGenerateStateByNamePlaceholder, AmiToolGenerateStateRootMenu);
}

static void ami_tool_scene_generate_show_amiibo_menu(AmiToolApp* app, size_t game_index) {
    FuriString* game_name = furi_string_alloc();
    bool found = ami_tool_scene_generate_get_game_name(
        app, app->generate_platform, game_index, game_name);

    if(!found) {
        furi_string_printf(
            app->text_box_store,
            "Unable to find the selected game.\n\nReturn to the list and try again.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateGameList);
        furi_string_free(game_name);
        return;
    }

    bool ready = ami_tool_scene_generate_prepare_amiibo_entries(app, game_name);
    if(!ready || app->generate_amiibo_count == 0) {
        furi_string_free(game_name);
        return;
    }

    furi_string_set(app->generate_selected_game, game_name);

    submenu_reset(app->submenu);
    furi_string_printf(
        app->text_box_store,
        "%s Amiibo",
        furi_string_get_cstr(app->generate_selected_game));
    submenu_set_header(app->submenu, furi_string_get_cstr(app->text_box_store));

    for(size_t i = 0; i < app->generate_amiibo_count; i++) {
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(app->generate_amiibo_names[i]),
            i,
            ami_tool_scene_generate_submenu_callback,
            app);
    }

    app->generate_state = AmiToolGenerateStateAmiiboList;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);

    furi_string_free(game_name);
}

static size_t ami_tool_scene_generate_count_tokens(const char* data) {
    if(!data || data[0] == '\0') {
        return 0;
    }

    size_t count = 0;
    const char* cursor = data;

    while(*cursor) {
        while(*cursor == '|') {
            cursor++;
        }
        if(*cursor == '\0') {
            break;
        }
        count++;
        while(*cursor && *cursor != '|') {
            cursor++;
        }
    }

    return count;
}

static bool ami_tool_scene_generate_populate_id_cache(AmiToolApp* app, const char* ids_line) {
    size_t token_count = ami_tool_scene_generate_count_tokens(ids_line);
    if(token_count == 0) {
        return false;
    }

    ami_tool_generate_clear_amiibo_cache(app);

    app->generate_amiibo_names = malloc(token_count * sizeof(FuriString*));
    app->generate_amiibo_ids = malloc(token_count * sizeof(FuriString*));
    if(!app->generate_amiibo_names || !app->generate_amiibo_ids) {
        ami_tool_generate_clear_amiibo_cache(app);
        return false;
    }

    for(size_t i = 0; i < token_count; i++) {
        app->generate_amiibo_names[i] = furi_string_alloc();
        app->generate_amiibo_ids[i] = furi_string_alloc();
    }

    size_t index = 0;
    size_t token_start = 0;
    size_t line_len = strlen(ids_line);

    for(size_t pos = 0; pos <= line_len; pos++) {
        if(ids_line[pos] == '|' || ids_line[pos] == '\0') {
            size_t token_len = pos - token_start;
            if(token_len > 0 && index < token_count) {
                furi_string_set_strn(
                    app->generate_amiibo_ids[index], ids_line + token_start, token_len);
                furi_string_trim(app->generate_amiibo_ids[index]);
                furi_string_set(
                    app->generate_amiibo_names[index],
                    furi_string_get_cstr(app->generate_amiibo_ids[index]));
                index++;
            }
            token_start = pos + 1;
        }
    }

    app->generate_amiibo_count = index;
    if(index == 0) {
        ami_tool_generate_clear_amiibo_cache(app);
        return false;
    }

    return true;
}

typedef struct {
    const FuriString* game_name;
    FuriString* result_ids;
    bool found;
} AmiToolGenerateMappingContext;

static bool ami_tool_scene_generate_mapping_callback(
    void* context,
    size_t index,
    FuriString* line) {
    UNUSED(index);
    AmiToolGenerateMappingContext* ctx = context;
    const char* raw = furi_string_get_cstr(line);
    const char* target = furi_string_get_cstr(ctx->game_name);
    size_t target_len = furi_string_size(ctx->game_name);

    if(strncmp(raw, target, target_len) == 0 && raw[target_len] == ':') {
        const char* value = raw + target_len + 1;
        while(*value == ' ' || *value == '\t') {
            value++;
        }
        furi_string_set(ctx->result_ids, value);
        ctx->found = true;
        return false;
    }

    return true;
}

static bool ami_tool_scene_generate_find_mapping_for_game(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    const FuriString* game_name,
    FuriString* out_ids) {
    const char* path = ami_tool_scene_generate_platform_mapping_path(platform);
    if(!path) {
        return false;
    }

    AmiToolGenerateMappingContext context = {
        .game_name = game_name,
        .result_ids = out_ids,
        .found = false,
    };

    furi_string_reset(out_ids);
    bool ok = ami_tool_scene_generate_iterate_lines(
        app, path, true, ami_tool_scene_generate_mapping_callback, &context);
    return ok && context.found;
}

typedef struct {
    AmiToolApp* app;
    size_t remaining;
} AmiToolGenerateAmiiboLookupContext;

static bool ami_tool_scene_generate_amiibo_lookup_callback(
    void* context,
    size_t index,
    FuriString* line) {
    UNUSED(index);
    AmiToolGenerateAmiiboLookupContext* lookup = context;
    const char* raw = furi_string_get_cstr(line);
    const char* colon = strchr(raw, ':');
    if(!colon) {
        return true;
    }

    size_t id_len = colon - raw;
    const char* value = colon + 1;
    while(*value == ' ' || *value == '\t') {
        value++;
    }
    const char* pipe = strchr(value, '|');
    size_t name_len = pipe ? (size_t)(pipe - value) : strlen(value);

    for(size_t i = 0; i < lookup->app->generate_amiibo_count; i++) {
        if(furi_string_size(lookup->app->generate_amiibo_ids[i]) == id_len &&
           strncmp(
               raw,
               furi_string_get_cstr(lookup->app->generate_amiibo_ids[i]),
               id_len) == 0) {
            furi_string_set_strn(
                lookup->app->generate_amiibo_names[i], value, name_len);
            lookup->remaining--;
            if(lookup->remaining == 0) {
                return false;
            }
            break;
        }
    }

    return true;
}

static bool ami_tool_scene_generate_resolve_amiibo_names(AmiToolApp* app) {
    if(app->generate_amiibo_count == 0) {
        return false;
    }

    AmiToolGenerateAmiiboLookupContext context = {
        .app = app,
        .remaining = app->generate_amiibo_count,
    };

    bool ok = ami_tool_scene_generate_iterate_lines(
        app,
        APP_ASSETS_PATH("amiibo.dat"),
        true,
        ami_tool_scene_generate_amiibo_lookup_callback,
        &context);
    return ok;
}

static bool ami_tool_scene_generate_prepare_amiibo_entries(
    AmiToolApp* app,
    const FuriString* game_name) {
    FuriString* ids = furi_string_alloc();
    bool found = ami_tool_scene_generate_find_mapping_for_game(
        app, app->generate_platform, game_name, ids);

    if(!found) {
        furi_string_printf(
            app->text_box_store,
            "No Amiibo mapping found for:\n%s\n\nUpdate your assets.",
            furi_string_get_cstr(game_name));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateGameList);
        furi_string_free(ids);
        return false;
    }

    if(!ami_tool_scene_generate_populate_id_cache(app, furi_string_get_cstr(ids))) {
        furi_string_printf(
            app->text_box_store,
            "Unable to parse Amiibo mapping for:\n%s",
            furi_string_get_cstr(game_name));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateGameList);
        furi_string_free(ids);
        return false;
    }

    ami_tool_scene_generate_resolve_amiibo_names(app);

    if(app->generate_amiibo_count == 0) {
        furi_string_printf(
            app->text_box_store,
            "No Amiibo IDs available for:\n%s",
            furi_string_get_cstr(game_name));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateGameList);
        furi_string_free(ids);
        return false;
    }

    furi_string_free(ids);
    return true;
}

static void ami_tool_scene_generate_show_amiibo_placeholder(AmiToolApp* app, size_t amiibo_index) {
    if(amiibo_index >= app->generate_amiibo_count) {
        furi_string_printf(
            app->text_box_store,
            "Unknown Amiibo selection.\n\nReturn to the previous menu.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateAmiiboList);
        return;
    }

    const char* id = furi_string_get_cstr(app->generate_amiibo_ids[amiibo_index]);
    ami_tool_generate_random_uid(app);
    ami_tool_info_show_page(app, id, false);
    app->generate_state = AmiToolGenerateStateAmiiboPlaceholder;
    app->generate_return_state = AmiToolGenerateStateAmiiboList;
}

static void ami_tool_scene_generate_commit_text_view(
    AmiToolApp* app,
    AmiToolGenerateState state,
    AmiToolGenerateState return_state) {
    app->generate_state = state;
    app->generate_return_state = return_state;
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
}

static bool ami_tool_scene_generate_iterate_lines(
    AmiToolApp* app,
    const char* path,
    bool skip_header,
    AmiToolGenerateLineCallback callback,
    void* context) {
    if(!app->storage || !path || !callback) {
        return false;
    }

    File* file = storage_file_alloc(app->storage);
    if(!file) {
        return false;
    }

    FuriString* line = furi_string_alloc();
    bool success = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bool in_data_section = !skip_header;
        size_t line_index = 0;
        bool keep_reading = true;
        uint8_t buffer[AMI_TOOL_GENERATE_READ_BUFFER];

        while(keep_reading) {
            size_t read = storage_file_read(file, buffer, sizeof(buffer));
            if(read == 0) break;

            for(size_t i = 0; i < read; i++) {
                char ch = (char)buffer[i];
                if(ch == '\r') {
                    continue;
                }
                if(ch == '\n') {
                    if(in_data_section) {
                        if(!furi_string_empty(line)) {
                            keep_reading = callback(context, line_index, line);
                            line_index++;
                        }
                    } else if(furi_string_empty(line)) {
                        in_data_section = true;
                    }
                    furi_string_reset(line);
                    if(!keep_reading) break;
                } else {
                    furi_string_push_back(line, ch);
                }
            }

            if(!keep_reading) break;
        }

        if(keep_reading && in_data_section && !furi_string_empty(line)) {
            callback(context, line_index, line);
        }

        success = true;
        storage_file_close(file);
    }

    furi_string_free(line);
    storage_file_free(file);
    return success;
}

static bool ami_tool_scene_generate_iterate_games(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    AmiToolGenerateLineCallback callback,
    void* context) {
    const char* path = NULL;
    switch(platform) {
    case AmiToolGeneratePlatform3DS:
        path = APP_ASSETS_PATH("game_3ds.dat");
        break;
    case AmiToolGeneratePlatformWiiU:
        path = APP_ASSETS_PATH("game_wii_u.dat");
        break;
    case AmiToolGeneratePlatformSwitch:
        path = APP_ASSETS_PATH("game_switch.dat");
        break;
    case AmiToolGeneratePlatformSwitch2:
        path = APP_ASSETS_PATH("game_switch2.dat");
        break;
    default:
        break;
    }

    if(!path) {
        return false;
    }

    return ami_tool_scene_generate_iterate_lines(app, path, true, callback, context);
}

static bool ami_tool_scene_generate_get_game_name(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    size_t target_index,
    FuriString* out) {
    AmiToolGenerateFindGameContext context = {
        .target_index = target_index,
        .result = out,
        .found = false,
    };

    furi_string_reset(out);
    bool file_ok = ami_tool_scene_generate_iterate_games(
        app, platform, ami_tool_scene_generate_find_game_callback, &context);

    return file_ok && context.found;
}

static const char* ami_tool_scene_generate_platform_label(AmiToolGeneratePlatform platform) {
    switch(platform) {
    case AmiToolGeneratePlatform3DS:
        return "3DS";
    case AmiToolGeneratePlatformWiiU:
        return "Wii U";
    case AmiToolGeneratePlatformSwitch:
        return "Switch";
    case AmiToolGeneratePlatformSwitch2:
        return "Switch 2";
    default:
        return "Unknown";
    }
}

static const char* ami_tool_scene_generate_platform_mapping_path(AmiToolGeneratePlatform platform) {
    switch(platform) {
    case AmiToolGeneratePlatform3DS:
        return APP_ASSETS_PATH("game_3ds_mapping.dat");
    case AmiToolGeneratePlatformWiiU:
        return APP_ASSETS_PATH("game_wii_u_mapping.dat");
    case AmiToolGeneratePlatformSwitch:
        return APP_ASSETS_PATH("game_switch_mapping.dat");
    case AmiToolGeneratePlatformSwitch2:
        return APP_ASSETS_PATH("game_switch2_mapping.dat");
    default:
        return NULL;
    }
}

static void ami_tool_scene_generate_submenu_callback(void* context, uint32_t index) {
    AmiToolApp* app = context;

    switch(app->generate_state) {
    case AmiToolGenerateStateRootMenu:
        if(index == AmiToolGenerateRootMenuIndexByName) {
            ami_tool_scene_generate_show_name_placeholder(app);
        } else if(index == AmiToolGenerateRootMenuIndexByGames) {
            ami_tool_scene_generate_show_platform_menu(app);
        }
        break;
    case AmiToolGenerateStatePlatformMenu:
        if(index < AmiToolGeneratePlatformCount) {
            app->generate_platform = (AmiToolGeneratePlatform)index;
            ami_tool_scene_generate_show_games_menu(app);
        }
        break;
    case AmiToolGenerateStateGameList:
        if(index < app->generate_game_count) {
            ami_tool_scene_generate_show_amiibo_menu(app, index);
        }
        break;
    case AmiToolGenerateStateAmiiboList:
        if(index < app->generate_amiibo_count) {
            ami_tool_scene_generate_show_amiibo_placeholder(app, index);
        }
        break;
    default:
        break;
    }
}

static void ami_tool_scene_generate_return_to_state(AmiToolApp* app, AmiToolGenerateState state) {
    switch(state) {
    case AmiToolGenerateStateRootMenu:
        ami_tool_scene_generate_show_root_menu(app);
        break;
    case AmiToolGenerateStatePlatformMenu:
        ami_tool_scene_generate_show_platform_menu(app);
        break;
    case AmiToolGenerateStateGameList:
        ami_tool_scene_generate_show_games_menu(app);
        break;
    case AmiToolGenerateStateAmiiboList:
        if(app->generate_game_count > 0) {
            ami_tool_scene_generate_show_games_menu(app);
        } else {
            ami_tool_scene_generate_show_platform_menu(app);
        }
        break;
    default:
        ami_tool_scene_generate_show_root_menu(app);
        break;
    }
}

void ami_tool_scene_generate_on_enter(void* context) {
    AmiToolApp* app = context;
    ami_tool_scene_generate_show_root_menu(app);
}

bool ami_tool_scene_generate_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        switch(app->generate_state) {
        case AmiToolGenerateStateRootMenu:
            return false;
        case AmiToolGenerateStatePlatformMenu:
            ami_tool_scene_generate_show_root_menu(app);
            return true;
        case AmiToolGenerateStateGameList:
            ami_tool_scene_generate_show_platform_menu(app);
            return true;
        case AmiToolGenerateStateAmiiboList:
            ami_tool_scene_generate_show_games_menu(app);
            return true;
        case AmiToolGenerateStateByNamePlaceholder:
            ami_tool_scene_generate_show_root_menu(app);
            return true;
        case AmiToolGenerateStateAmiiboPlaceholder:
        case AmiToolGenerateStateMessage:
            ami_tool_scene_generate_return_to_state(app, app->generate_return_state);
            return true;
        default:
            break;
        }
    }

    return false;
}

void ami_tool_scene_generate_on_exit(void* context) {
    AmiToolApp* app = context;
    app->generate_state = AmiToolGenerateStateRootMenu;
    app->generate_return_state = AmiToolGenerateStateRootMenu;
    app->generate_game_count = 0;
    ami_tool_generate_clear_amiibo_cache(app);
    ami_tool_scene_generate_clear_selected_game(app);
}
