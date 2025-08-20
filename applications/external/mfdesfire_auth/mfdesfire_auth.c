#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/submenu.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include <dialogs/dialogs.h>
#include <furi/core/string.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include <gui/icon_i.h>

#define TAG                 "MfDesfireAuthApp"
#define NFC_CARDS_PATH      "/ext/nfc"
#define INITIAL_VECTOR_SIZE 8
#define KEY_SIZE            24

#define SAVE_PATH         APP_DATA_PATH("mfdesfire_auth.save")
#define SAVE_FILE_HEADER  "Flipper DESFire Auth Settings"
#define SAVE_FILE_VERSION 1

const uint8_t icon_data[] = {0x00, 0x80, 0x00, 0x02, 0x01, 0x1E, 0x02, 0x24, 0x02, 0x44, 0x02,
                             0x44, 0x02, 0x44, 0x02, 0x44, 0x02, 0x3E, 0x01, 0x80, 0x00};

// Pointers to frame data
const uint8_t* const icon_frames_10px[] = {icon_data};

const Icon submenu_icon = {
    .width = 10,
    .height = 10,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = icon_frames_10px,
};

// Enum pro definici pohledů
typedef enum {
    MfDesfireAuthAppViewBrowser,
    MfDesfireAuthAppViewSubmenu,
    MfDesfireAuthAppViewByteInputIV,
    MfDesfireAuthAppViewByteInputKey,
    MfDesfireAuthAppViewPopupAuth,
} MfDesfireAuthAppView;

// Enum pro submenu položky
typedef enum {
    MfDesfireAuthSubmenuSetIV,
    MfDesfireAuthSubmenuSetKey,
    MfDesfireAuthSubmenuAuthenticate,
} MfDesfireAuthSubmenuIndex;

typedef enum {
    MfDesfireAuthSaveIV,
    MfDesfireAuthSaveKey,
} MfDesfireAuthSaveIndex;

/**
 * @brief Struktura pro uchování stavu aplikace.
 */
typedef struct {
    //Zaklad pro aplikaci
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    // Jednotliva views
    FileBrowser* file_browser;
    Submenu* submenu;
    ByteInput* byte_input;
    Popup* popup;

    FuriString* selected_card_path;

    uint8_t initial_vector[INITIAL_VECTOR_SIZE];
    uint8_t key[KEY_SIZE];

    MfDesfireAuthAppView current_view; // Track current view manually
} MfDesfireAuthApp;

static bool mfdesfire_auth_save_settings(MfDesfireAuthApp* app, uint32_t index) {
    bool result = false;
    // UNUSED(index);
    // MfDesfireAuthApp* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    // Extract filename from full path
    const char* filename = strrchr(furi_string_get_cstr(app->selected_card_path), '/');
    if(filename) {
        filename++; // Skip the '/' character
    } else {
        filename = furi_string_get_cstr(app->selected_card_path);
    }

    do {
        // Open existing or create a new one
        if(!flipper_format_file_open_existing(file, SAVE_PATH)){
            flipper_format_file_open_new(file, SAVE_PATH);
            // Write the header to a new file
            if(!flipper_format_write_header_cstr(file, SAVE_FILE_HEADER, SAVE_FILE_VERSION)) break;
        };

        // Prepare key variable
        FuriString* key_name = furi_string_alloc();

        // Set key for card path
        furi_string_printf(key_name, "Card_Path_%s", filename);
        if(!flipper_format_insert_or_update_string(file, furi_string_get_cstr(key_name), app->selected_card_path)) break;

        if(index == MfDesfireAuthSaveIV){
            furi_string_printf(key_name, "Initial_Vector_%s", filename);
            if(!flipper_format_insert_or_update_hex(file, furi_string_get_cstr(key_name), app->initial_vector, INITIAL_VECTOR_SIZE)) break;
        }

        if(index == MfDesfireAuthSaveKey){
            furi_string_printf(key_name, "Key_%s", filename);
            if(!flipper_format_insert_or_update_hex(file, furi_string_get_cstr(key_name), app->key, KEY_SIZE)) break;
        }

        furi_string_free(key_name);
        result = true;
    } while(false);

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

static bool mfdesfire_auth_load_settings(MfDesfireAuthApp* app) {
    bool result = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    uint32_t version;

    do {
        if(!flipper_format_file_open_existing(file, SAVE_PATH)) break;

        // Read and verify header
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(!furi_string_equal_str(temp_str, SAVE_FILE_HEADER)) break;
        if(version != SAVE_FILE_VERSION) break;

        // Extract filename from full path
        const char* filename = strrchr(furi_string_get_cstr(app->selected_card_path), '/');
        if(filename) {
            filename++; // Skip the '/' character
        } else {
            filename = furi_string_get_cstr(app->selected_card_path);
        }

        FuriString* key_name = furi_string_alloc();

        furi_string_printf(key_name, "Card_Path_%s", filename);
        if(flipper_format_read_string(file, furi_string_get_cstr(key_name), temp_str)) {
            if(furi_string_equal(temp_str, app->selected_card_path)) {
                furi_string_printf(key_name, "Initial_Vector_%s", filename);
                if(!flipper_format_read_hex(file, furi_string_get_cstr(key_name), app->initial_vector, INITIAL_VECTOR_SIZE)) break;

                furi_string_printf(key_name, "Key_%s", filename);
                if(!flipper_format_read_hex(file, furi_string_get_cstr(key_name), app->key, KEY_SIZE)) break;

                result = true;
            }
        }

        furi_string_free(key_name);
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

/**
 * @brief Callback funkce volaná při stisknutí tlačítka Zpět.
 */
static bool mfdesfire_auth_navigation_callback(void* context) {
    furi_assert(context);
    MfDesfireAuthApp* app = context;

    switch(app->current_view) {
    case MfDesfireAuthAppViewBrowser:
        // Z file browseru ukončíme aplikaci
        view_dispatcher_stop(app->view_dispatcher);
        break;

    case MfDesfireAuthAppViewSubmenu:
        // Ze submenu se vrátíme do file browseru
        app->current_view = MfDesfireAuthAppViewBrowser;
        view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewBrowser);
        break;

    case MfDesfireAuthAppViewByteInputIV:
    case MfDesfireAuthAppViewByteInputKey:
    case MfDesfireAuthAppViewPopupAuth:
        // Z byte input views se vrátíme do submenu
        app->current_view = MfDesfireAuthAppViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewSubmenu);
        // TODO udelat tady load zpatky aby to neukazovalo fake 1
        break;

        // case MfDesfireAuthAppViewPopupAuth:
        //     // Z popup se vrátíme do submenu
        //     app->current_view = MfDesfireAuthAppViewSubmenu;
        //     view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewSubmenu);
        //     break;

    default:
        // Jako fallback ukončíme aplikaci
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }

    return true;
}

/**
 * @brief Callback pro ukončení byte input.
 */
static void mfdesfire_auth_byte_input_callback(MfDesfireAuthApp* app, uint32_t index) {
    mfdesfire_auth_save_settings(app, index);


    app->current_view = MfDesfireAuthAppViewSubmenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewSubmenu);
}

static void mfdesfire_auth_byte_input_vector_callback(void* context) {
    furi_assert(context);
    MfDesfireAuthApp* app = context;

    mfdesfire_auth_byte_input_callback(app, MfDesfireAuthSaveIV);
}

static void mfdesfire_auth_byte_input_key_callback(void* context) {
    furi_assert(context);
    MfDesfireAuthApp* app = context;

    mfdesfire_auth_byte_input_callback(app, MfDesfireAuthSaveKey);
}

/**
 * @brief Callback pro submenu.
 */
static void mfdesfire_auth_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    MfDesfireAuthApp* app = context;

    switch(index) {
    case MfDesfireAuthSubmenuSetIV:
        byte_input_set_header_text(app->byte_input, "Set Initial Vector:");
        byte_input_set_result_callback( // Tohle je callback na to kdyz dam save. Kdyz klikam zpet tak se vola jiny.
                app->byte_input,
                mfdesfire_auth_byte_input_vector_callback,
                NULL,
                app,
                app->initial_vector,
                INITIAL_VECTOR_SIZE);
        app->current_view = MfDesfireAuthAppViewByteInputIV;
        view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewByteInputIV);
        break;

    case MfDesfireAuthSubmenuSetKey:
        byte_input_set_header_text(app->byte_input, "Set Key:");
        byte_input_set_result_callback(
            app->byte_input, mfdesfire_auth_byte_input_key_callback, NULL, app, app->key, KEY_SIZE);
        app->current_view = MfDesfireAuthAppViewByteInputKey;
        view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewByteInputKey);
        break;

    case MfDesfireAuthSubmenuAuthenticate:
        popup_set_header(app->popup, "Authenticating...", 64, 20, AlignCenter, AlignCenter);
        popup_set_text(app->popup, "Please wait", 64, 40, AlignCenter, AlignCenter);
        popup_set_icon(app->popup, 0, 0, NULL); // No icon
        popup_set_timeout(app->popup, 3000); // 3 seconds timeout
        popup_set_context(app->popup, app);
        popup_set_callback(app->popup, NULL); // No callback needed
        popup_enable_timeout(app->popup);
        app->current_view = MfDesfireAuthAppViewPopupAuth;
        view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewPopupAuth);
        break;

    default:
        break;
    }
}

/**
 * @brief Callback funkce volaná po výběru souboru v FileBrowseru.
 */
static void mfdesfire_auth_file_select_callback(void* context) {
    furi_assert(context);
    MfDesfireAuthApp* app = context;

    // Try to load settings for selected card
    if(!mfdesfire_auth_load_settings(app)) {
        // If no settings found, initialize with zeros
        memset(app->initial_vector, 0, INITIAL_VECTOR_SIZE);
        memset(app->key, 0, KEY_SIZE);
    }

    app->current_view = MfDesfireAuthAppViewSubmenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewSubmenu);
}

/**
 * @brief Callback pro popup timeout.
 */
static void mfdesfire_auth_popup_callback(void* context) {
    furi_assert(context);
    MfDesfireAuthApp* app = context;
    app->current_view = MfDesfireAuthAppViewSubmenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewSubmenu);
}

/**
 * @brief Alokuje a inicializuje zdroje aplikace.
 */
MfDesfireAuthApp* mfdesfire_auth_app_alloc() {
    MfDesfireAuthApp* app = malloc(sizeof(MfDesfireAuthApp));

    // Inicializace dat
    app->selected_card_path = furi_string_alloc();
    app->current_view = MfDesfireAuthAppViewBrowser; // Initialize current view
    memset(app->initial_vector, 0, INITIAL_VECTOR_SIZE);
    memset(app->key, 0, KEY_SIZE);

    // GUI komponenty
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->file_browser = file_browser_alloc(
        app->selected_card_path); // Ulozi vyslednou cestu do selected card path - je to v file_browser.c inner se o to stara
    app->submenu = submenu_alloc();
    app->byte_input = byte_input_alloc();
    app->popup = popup_alloc();

    // Nastavení FileBrowseru
    file_browser_set_callback(
        app->file_browser, mfdesfire_auth_file_select_callback, app); // Select v file browseru
    file_browser_configure(app->file_browser, ".nfc", NFC_CARDS_PATH, 1, 1, &submenu_icon, 1);
    file_browser_start(app->file_browser, furi_string_alloc_set(NFC_CARDS_PATH));

    // Nastavení Submenu
    submenu_add_item(
        app->submenu,
        "Set Initial Vector",
        MfDesfireAuthSubmenuSetIV,
        mfdesfire_auth_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "Set Key", MfDesfireAuthSubmenuSetKey, mfdesfire_auth_submenu_callback, app);
    submenu_add_item(
        app->submenu,
        "Legacy Authetication",
        MfDesfireAuthSubmenuAuthenticate,
        mfdesfire_auth_submenu_callback,
        app);

    // Nastavení Popup
    popup_set_callback(app->popup, mfdesfire_auth_popup_callback);
    popup_set_context(app->popup, app);

    // Nastavení ViewDispatcheru
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher,
        mfdesfire_auth_navigation_callback); // Tohle se zavola kdyz kratce kliknu tlacitko zpet
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Přidání views
    view_dispatcher_add_view(
        app->view_dispatcher,
        MfDesfireAuthAppViewBrowser,
        file_browser_get_view(app->file_browser));

    view_dispatcher_add_view(
        app->view_dispatcher, MfDesfireAuthAppViewSubmenu, submenu_get_view(app->submenu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        MfDesfireAuthAppViewByteInputIV,
        byte_input_get_view(app->byte_input));

    view_dispatcher_add_view(
        app->view_dispatcher,
        MfDesfireAuthAppViewByteInputKey,
        byte_input_get_view(app->byte_input));

    view_dispatcher_add_view(
        app->view_dispatcher, MfDesfireAuthAppViewPopupAuth, popup_get_view(app->popup));

    return app;
}

/**
 * @brief Uvolní všechny alokované zdroje aplikace.
 */
void mfdesfire_auth_app_free(MfDesfireAuthApp* app) {
    furi_assert(app);

    // Odstranění views
    view_dispatcher_remove_view(app->view_dispatcher, MfDesfireAuthAppViewBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, MfDesfireAuthAppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, MfDesfireAuthAppViewByteInputIV);
    view_dispatcher_remove_view(app->view_dispatcher, MfDesfireAuthAppViewByteInputKey);
    view_dispatcher_remove_view(app->view_dispatcher, MfDesfireAuthAppViewPopupAuth);

    // Uvolnění zdrojů
    view_dispatcher_free(app->view_dispatcher);
    file_browser_stop(app->file_browser);
    file_browser_free(app->file_browser);
    submenu_free(app->submenu);
    byte_input_free(app->byte_input);
    popup_free(app->popup);
    furi_string_free(app->selected_card_path);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t mfdesfire_auth_main(void* p) {
    UNUSED(p);

    MfDesfireAuthApp* app = mfdesfire_auth_app_alloc();
    app->current_view = MfDesfireAuthAppViewBrowser;
    view_dispatcher_switch_to_view(app->view_dispatcher, MfDesfireAuthAppViewBrowser);
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "Stopping app & cleaning");
    mfdesfire_auth_app_free(app);

    return 0;
}
