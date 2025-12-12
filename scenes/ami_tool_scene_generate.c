#include "ami_tool_i.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define AMI_TOOL_GENERATE_READ_BUFFER 64
#define AMI_TOOL_GENERATE_MENU_INDEX_PREV_PAGE UINT32_C(0xFFFFFFFE)
#define AMI_TOOL_GENERATE_MENU_INDEX_NEXT_PAGE UINT32_C(0xFFFFFFFD)

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
static bool ami_tool_scene_generate_show_cached_amiibo_menu(AmiToolApp* app);
static void ami_tool_scene_generate_change_page(AmiToolApp* app, int direction);
static bool ami_tool_scene_generate_prepare_name_entries(AmiToolApp* app);
static void ami_tool_scene_generate_show_name_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_amiibo_placeholder(AmiToolApp* app, size_t amiibo_index);
static void ami_tool_scene_generate_clear_selected_game(AmiToolApp* app);
static int8_t ami_tool_scene_generate_hex_value(char ch);
static bool ami_tool_scene_generate_parse_uuid(const char* hex, uint8_t* out, size_t out_len);
static bool ami_tool_scene_generate_prepare_dump(AmiToolApp* app, const char* id_hex);

static void ami_tool_scene_generate_clear_selected_game(AmiToolApp* app) {
    if(app->generate_selected_game) {
        furi_string_reset(app->generate_selected_game);
    }
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;
    app->generate_list_source = AmiToolGenerateListSourceGame;
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
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;
}

static int8_t ami_tool_scene_generate_hex_value(char ch) {
    if(ch >= '0' && ch <= '9') {
        return (int8_t)(ch - '0');
    }
    ch = (char)tolower((unsigned char)ch);
    if(ch >= 'a' && ch <= 'f') {
        return (int8_t)(10 + (ch - 'a'));
    }
    return -1;
}

static bool ami_tool_scene_generate_parse_uuid(const char* hex, uint8_t* out, size_t out_len) {
    if(!hex || !out || out_len == 0) {
        return false;
    }
    size_t required = out_len * 2;
    if(strlen(hex) < required) {
        return false;
    }
    for(size_t i = 0; i < out_len; i++) {
        char hi_char = hex[i * 2];
        char lo_char = hex[i * 2 + 1];
        if(hi_char == '\0' || lo_char == '\0') {
            return false;
        }
        int8_t hi = ami_tool_scene_generate_hex_value(hi_char);
        int8_t lo = ami_tool_scene_generate_hex_value(lo_char);
        if(hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static bool ami_tool_scene_generate_prepare_dump(AmiToolApp* app, const char* id_hex) {
    if(!app || !app->tag_data || !id_hex) {
        return false;
    }

    uint8_t uuid[8];
    if(!ami_tool_scene_generate_parse_uuid(id_hex, uuid, sizeof(uuid))) {
        return false;
    }

    ami_tool_clear_cached_tag(app);

    Ntag21xMetadataHeader header;
    RfidxStatus status = amiibo_generate(uuid, app->tag_data, &header);
    if(status != RFIDX_OK) {
        return false;
    }

    if(app->tag_data->pages_total < 2) {
        return false;
    }

    uint8_t uid[7];
    memcpy(uid, app->tag_data->page[0].data, 3);
    memcpy(uid + 3, app->tag_data->page[1].data, 4);
    ami_tool_store_uid(app, uid, sizeof(uid));

    app->tag_data_valid = true;
    return true;
}

static void ami_tool_scene_generate_submenu_callback(void* context, uint32_t index);
static void ami_tool_scene_generate_show_root_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_platform_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_games_menu(AmiToolApp* app);
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

typedef struct {
    size_t count;
} AmiToolGenerateNameCountContext;

static bool ami_tool_scene_generate_name_count_callback(
    void* context,
    size_t index,
    FuriString* line) {
    UNUSED(index);
    AmiToolGenerateNameCountContext* ctx = context;
    if(!furi_string_empty(line)) {
        ctx->count++;
    }
    return true;
}

typedef struct {
    AmiToolApp* app;
    size_t position;
    bool ok;
} AmiToolGenerateNameLoadContext;

static bool ami_tool_scene_generate_name_load_callback(
    void* context,
    size_t index,
    FuriString* line) {
    UNUSED(index);
    AmiToolGenerateNameLoadContext* ctx = context;
    if(!ctx || furi_string_empty(line)) {
        return true;
    }

    const char* raw = furi_string_get_cstr(line);
    const char* colon = strchr(raw, ':');
    if(!colon) {
        return true;
    }

    size_t name_len = (size_t)(colon - raw);
    while(name_len > 0 && raw[name_len - 1] == ' ') {
        name_len--;
    }

    const char* value = colon + 1;
    while(*value == ' ' || *value == '\t') {
        value++;
    }

    if(ctx->position >= ctx->app->generate_amiibo_count) {
        ctx->ok = false;
        return false;
    }

    furi_string_set_strn(ctx->app->generate_amiibo_names[ctx->position], raw, name_len);
    furi_string_set(ctx->app->generate_amiibo_ids[ctx->position], value);
    furi_string_trim(ctx->app->generate_amiibo_ids[ctx->position]);
    ctx->position++;
    return true;
}

static bool ami_tool_scene_generate_prepare_name_entries(AmiToolApp* app) {
    const char* path = APP_ASSETS_PATH("amiibo_name.dat");
    AmiToolGenerateNameCountContext count_ctx = {.count = 0};

    bool file_ok = ami_tool_scene_generate_iterate_lines(
        app, path, true, ami_tool_scene_generate_name_count_callback, &count_ctx);
    if(!file_ok || count_ctx.count == 0) {
        return false;
    }

    ami_tool_generate_clear_amiibo_cache(app);

    app->generate_amiibo_names = malloc(count_ctx.count * sizeof(FuriString*));
    app->generate_amiibo_ids = malloc(count_ctx.count * sizeof(FuriString*));
    if(!app->generate_amiibo_names || !app->generate_amiibo_ids) {
        ami_tool_generate_clear_amiibo_cache(app);
        return false;
    }

    for(size_t i = 0; i < count_ctx.count; i++) {
        app->generate_amiibo_names[i] = furi_string_alloc();
        app->generate_amiibo_ids[i] = furi_string_alloc();
    }

    app->generate_amiibo_count = count_ctx.count;

    AmiToolGenerateNameLoadContext load_ctx = {
        .app = app,
        .position = 0,
        .ok = true,
    };

    file_ok = ami_tool_scene_generate_iterate_lines(
        app, path, true, ami_tool_scene_generate_name_load_callback, &load_ctx);
    if(!file_ok || !load_ctx.ok || load_ctx.position == 0) {
        ami_tool_generate_clear_amiibo_cache(app);
        return false;
    }

    app->generate_amiibo_count = load_ctx.position;
    return true;
}

static void ami_tool_scene_generate_show_name_menu(AmiToolApp* app) {
    if(!ami_tool_scene_generate_prepare_name_entries(app)) {
        furi_string_set(
            app->text_box_store,
            "Unable to load amiibo_name.dat.\n\nEnsure the file is installed in assets.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateRootMenu);
        return;
    }

    furi_string_set_str(app->generate_selected_game, "Amiibo by Name");
    app->generate_list_source = AmiToolGenerateListSourceName;
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;

    if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
        furi_string_set(
            app->text_box_store,
            "No Amiibo names available.\n\nUpdate amiibo_name.dat and try again.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateRootMenu);
    }
}

static bool ami_tool_scene_generate_show_cached_amiibo_menu(AmiToolApp* app) {
    if(!app->generate_selected_game ||
       furi_string_empty(app->generate_selected_game) ||
       app->generate_amiibo_count == 0 ||
       !app->generate_amiibo_names) {
        return false;
    }

    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;

    if(app->generate_selected_index >= app->generate_amiibo_count) {
        app->generate_selected_index = app->generate_amiibo_count - 1;
    }

    app->generate_page_offset = (app->generate_selected_index / page_size) * page_size;
    size_t remaining = app->generate_amiibo_count - app->generate_page_offset;
    size_t page_entries = remaining < page_size ? remaining : page_size;
    size_t total_pages = (app->generate_amiibo_count + page_size - 1) / page_size;
    size_t current_page = (app->generate_page_offset / page_size) + 1;

    submenu_reset(app->submenu);
    furi_string_printf(
        app->text_box_store,
        "%s Amiibo (%u/%u)",
        furi_string_get_cstr(app->generate_selected_game),
        (unsigned int)current_page,
        (unsigned int)((total_pages > 0) ? total_pages : 1));
    submenu_set_header(app->submenu, furi_string_get_cstr(app->text_box_store));

    if(app->generate_page_offset > 0) {
        submenu_add_item(
            app->submenu,
            "< Previous Page",
            AMI_TOOL_GENERATE_MENU_INDEX_PREV_PAGE,
            ami_tool_scene_generate_submenu_callback,
            app);
    }

    for(size_t i = 0; i < page_entries; i++) {
        size_t entry_index = app->generate_page_offset + i;
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(app->generate_amiibo_names[entry_index]),
            entry_index,
            ami_tool_scene_generate_submenu_callback,
            app);
    }

    if((app->generate_page_offset + page_entries) < app->generate_amiibo_count) {
        submenu_add_item(
            app->submenu,
            "Next Page >",
            AMI_TOOL_GENERATE_MENU_INDEX_NEXT_PAGE,
            ami_tool_scene_generate_submenu_callback,
            app);
    }

    if(page_entries > 0) {
        if(app->generate_selected_index < app->generate_page_offset ||
           app->generate_selected_index >= app->generate_page_offset + page_entries) {
            app->generate_selected_index = app->generate_page_offset;
        }
        submenu_set_selected_item(app->submenu, app->generate_selected_index);
    }

    app->generate_state = AmiToolGenerateStateAmiiboList;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
    return true;
}

static void ami_tool_scene_generate_change_page(AmiToolApp* app, int direction) {
    if(app->generate_amiibo_count == 0) {
        return;
    }

    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;

    if(direction > 0) {
        size_t new_offset = app->generate_page_offset + page_size;
        if(new_offset >= app->generate_amiibo_count) {
            size_t last_index = app->generate_amiibo_count - 1;
            new_offset = (last_index / page_size) * page_size;
        }
        if(new_offset != app->generate_page_offset) {
            app->generate_page_offset = new_offset;
            app->generate_selected_index = new_offset;
            ami_tool_scene_generate_show_cached_amiibo_menu(app);
        }
    } else if(direction < 0) {
        size_t new_offset =
            (app->generate_page_offset >= page_size) ? app->generate_page_offset - page_size : 0;
        if(new_offset != app->generate_page_offset) {
            app->generate_page_offset = new_offset;
            app->generate_selected_index = new_offset;
            ami_tool_scene_generate_show_cached_amiibo_menu(app);
        }
    }
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
    app->generate_list_source = AmiToolGenerateListSourceGame;
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;

    if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
        furi_string_free(game_name);
        return;
    }

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

    app->generate_selected_index = amiibo_index;

    const char* id = furi_string_get_cstr(app->generate_amiibo_ids[amiibo_index]);
    if(!ami_tool_scene_generate_prepare_dump(app, id)) {
        furi_string_printf(
            app->text_box_store,
            "Unable to generate Amiibo dump.\n\nID: %s",
            id ? id : "Unknown");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateAmiiboList);
        return;
    }
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
            ami_tool_scene_generate_show_name_menu(app);
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
        if(index == AMI_TOOL_GENERATE_MENU_INDEX_PREV_PAGE) {
            ami_tool_scene_generate_change_page(app, -1);
        } else if(index == AMI_TOOL_GENERATE_MENU_INDEX_NEXT_PAGE) {
            ami_tool_scene_generate_change_page(app, 1);
        } else if(index < app->generate_amiibo_count) {
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
        if(app->generate_list_source == AmiToolGenerateListSourceName) {
            if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
                ami_tool_scene_generate_show_root_menu(app);
            }
        } else {
            if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
                if(app->generate_game_count > 0) {
                    ami_tool_scene_generate_show_games_menu(app);
                } else {
                    ami_tool_scene_generate_show_platform_menu(app);
                }
            }
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
        if(app->generate_list_source == AmiToolGenerateListSourceName) {
            ami_tool_scene_generate_show_root_menu(app);
        } else {
            ami_tool_scene_generate_show_games_menu(app);
        }
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
