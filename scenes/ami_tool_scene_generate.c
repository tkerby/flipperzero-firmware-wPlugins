#include "ami_tool_i.h"

#define AMI_TOOL_GENERATE_READ_BUFFER 64

typedef enum {
    AmiToolGenerateRootMenuIndexByName,
    AmiToolGenerateRootMenuIndexByGames,
} AmiToolGenerateRootMenuIndex;

typedef bool (*AmiToolGenerateGameCallback)(void* context, size_t index, FuriString* name);

static void ami_tool_scene_generate_submenu_callback(void* context, uint32_t index);
static void ami_tool_scene_generate_show_root_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_platform_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_games_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_name_placeholder(AmiToolApp* app);
static void ami_tool_scene_generate_show_game_placeholder(AmiToolApp* app, size_t game_index);
static void ami_tool_scene_generate_commit_text_view(
    AmiToolApp* app,
    AmiToolGenerateState state,
    AmiToolGenerateState return_state);
static bool ami_tool_scene_generate_iterate_games(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    AmiToolGenerateGameCallback callback,
    void* context);
static bool ami_tool_scene_generate_get_game_name(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    size_t target_index,
    FuriString* out);
static const char* ami_tool_scene_generate_platform_label(AmiToolGeneratePlatform platform);
static const char* ami_tool_scene_generate_platform_path(AmiToolGeneratePlatform platform);
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

static void ami_tool_scene_generate_show_game_placeholder(AmiToolApp* app, size_t game_index) {
    FuriString* game_name = furi_string_alloc();
    bool found =
        ami_tool_scene_generate_get_game_name(app, app->generate_platform, game_index, game_name);

    if(!found) {
        furi_string_printf(
            app->text_box_store,
            "Unable to find the selected game.\n\nReturn to the list and try again.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateGameList);
    } else {
        furi_string_printf(
            app->text_box_store,
            "%s\n\nGame-specific generation for %s titles is coming soon.",
            furi_string_get_cstr(game_name),
            ami_tool_scene_generate_platform_label(app->generate_platform));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateGamePlaceholder, AmiToolGenerateStateGameList);
    }

    furi_string_free(game_name);
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

static bool ami_tool_scene_generate_iterate_games(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    AmiToolGenerateGameCallback callback,
    void* context) {
    if(!app->storage || !callback) {
        return false;
    }

    const char* path = ami_tool_scene_generate_platform_path(platform);
    if(!path) {
        return false;
    }

    File* file = storage_file_alloc(app->storage);
    if(!file) {
        return false;
    }

    FuriString* line = furi_string_alloc();
    bool success = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bool in_game_section = false;
        size_t game_index = 0;
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
                    if(in_game_section) {
                        if(!furi_string_empty(line)) {
                            keep_reading = callback(context, game_index, line);
                            game_index++;
                        }
                    } else if(furi_string_empty(line)) {
                        in_game_section = true;
                    }
                    furi_string_reset(line);
                    if(!keep_reading) break;
                } else {
                    furi_string_push_back(line, ch);
                }
            }

            if(!keep_reading) break;
        }

        if(keep_reading && in_game_section && !furi_string_empty(line)) {
            callback(context, game_index, line);
        }

        success = true;
        storage_file_close(file);
    }

    furi_string_free(line);
    storage_file_free(file);
    return success;
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

static const char* ami_tool_scene_generate_platform_path(AmiToolGeneratePlatform platform) {
    switch(platform) {
    case AmiToolGeneratePlatform3DS:
        return APP_ASSETS_PATH("game_3ds.dat");
    case AmiToolGeneratePlatformWiiU:
        return APP_ASSETS_PATH("game_wii_u.dat");
    case AmiToolGeneratePlatformSwitch:
        return APP_ASSETS_PATH("game_switch.dat");
    case AmiToolGeneratePlatformSwitch2:
        return APP_ASSETS_PATH("game_switch2.dat");
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
            ami_tool_scene_generate_show_game_placeholder(app, index);
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
        case AmiToolGenerateStateByNamePlaceholder:
            ami_tool_scene_generate_show_root_menu(app);
            return true;
        case AmiToolGenerateStateGamePlaceholder:
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
}
