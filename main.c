#include <ctype.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <lib/toolbox/value_index.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ciphers/atbash.h"
#include "ciphers/baconian.h"
#include "ciphers/playfair.h"
#include "ciphers/railfence.h"
#include "ciphers/vigenere.h"

#include "hashes/blake2.h"
#include "hashes/md5.h"
#include "hashes/sha1.h"
#include "hashes/sha256.h"

// Scene declarations
typedef enum {
// Main menu
    FlipCryptMainMenuScene,
// Atbash scenes
    FlipCryptAtbashSubmenuScene,
    FlipCryptAtbashInputScene,
    FlipCryptAtbashOutputScene,
    FlipCryptAtbashDecryptInputScene,
    FlipCryptAtbashDecryptOutputScene,
    FlipCryptAtbashLearnScene,
// Baconian scenes
    FlipCryptBaconianSubmenuScene,
    FlipCryptBaconianInputScene,
    FlipCryptBaconianOutputScene,
    FlipCryptBaconianDecryptInputScene,
    FlipCryptBaconianDecryptOutputScene,
    FlipCryptBaconianLearnScene,
// Playfair scenes
    FlipCryptPlayfairSubmenuScene,
    FlipCryptPlayfairInputScene,
    FlipCryptPlayfairKeywordInputScene,
    FlipCryptPlayfairDecryptKeywordInputScene,
    FlipCryptPlayfairOutputScene,
    FlipCryptPlayfairDecryptInputScene,
    FlipCryptPlayfairDecryptOutputScene,
    FlipCryptPlayfairLearnScene,
// Railfence scenes
    FlipCryptRailfenceSubmenuScene,
    FlipCryptRailfenceInputScene,
    FlipCryptRailfenceOutputScene,
    FlipCryptRailfenceDecryptInputScene,
    FlipCryptRailfenceDecryptOutputScene,
    FlipCryptRailfenceLearnScene,
// Vigenere Cipher
    FlipCryptVigenereSubmenuScene,
    FlipCryptVigenereInputScene,
    FlipCryptVigenereKeywordInputScene,
    FlipCryptVigenereDecryptKeywordInputScene,
    FlipCryptVigenereOutputScene,
    FlipCryptVigenereDecryptInputScene,
    FlipCryptVigenereDecryptOutputScene,
    FlipCryptVigenereLearnScene,
// BLAKE2 Hash
    FlipCryptBlake2SubmenuScene,
    FlipCryptBlake2InputScene,
    FlipCryptBlake2OutputScene,
    FlipCryptBlake2LearnScene,
// MD5 Hash
    FlipCryptMD5SubmenuScene,
    FlipCryptMD5InputScene,
    FlipCryptMD5OutputScene,
    FlipCryptMD5LearnScene,
// SHA1 Hash
    FlipCryptSHA1SubmenuScene,
    FlipCryptSHA1InputScene,
    FlipCryptSHA1OutputScene,
    FlipCryptSHA1LearnScene,
// SHA256 Hash
    FlipCryptSHA256SubmenuScene,
    FlipCryptSHA256InputScene,
    FlipCryptSHA256OutputScene,
    FlipCryptSHA256LearnScene,
// Other
    FlipCryptAboutScene,
    FlipCryptSceneCount,
} FlipCryptScene;

// View type declarations
typedef enum {
    FlipCryptSubmenuView,
    FlipCryptWidgetView,
    FlipCryptTextInputView,
} FlipCryptView;

// View and variable inits
typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    char* atbash_input;
    char* atbash_decrypt_input;
    char* baconian_input;
    char* baconian_decrypt_input;
    char* playfair_input;
    char* playfair_keyword_input;
    char* playfair_decrypt_input;
    char* railfence_input;
    char* railfence_decrypt_input;
    char* vigenere_input;
    char* vigenere_keyword_input;
    char* vigenere_decrypt_input;
    char* blake2_input;
    char* md5_input;
    char* sha1_input;
    char* sha256_input;
    uint8_t atbash_input_size;
    uint8_t baconian_input_size;
    uint8_t playfair_input_size;
    uint8_t playfair_keyword_input_size;
    uint8_t railfence_input_size;
    uint8_t vigenere_input_size;
    uint8_t vigenere_keyword_input_size;
    uint8_t blake2_input_size;
    uint8_t md5_input_size;
    uint8_t sha1_input_size;
    uint8_t sha256_input_size;
    uint8_t atbash_decrypt_input_size;
    uint8_t baconian_decrypt_input_size;
    uint8_t playfair_decrypt_input_size;
    uint8_t railfence_decrypt_input_size;
    uint8_t vigenere_decrypt_input_size;
} App;

// Main Menu items Index
typedef enum {
    MenuIndexAtbash,
    MenuIndexBaconian,
    MenuIndexPlayfair,
    MenuIndexRailfence,
    MenuIndexVigenere,
    MenuIndexBlake2,
    MenuIndexMD5,
    MenuIndexSHA1,
    MenuIndexSHA256,
    MenuIndexAbout,
} CipherMenuIndex;

// Main Menu items Events
typedef enum {
    EventAtbash,
    EventBaconian,
    EventPlayfair,
    EventRailfence,
    EventVigenere,
    EventBlake2,
    EventMD5,
    EventSHA1,
    EventSHA256,
    EventAbout,
} CipherCustomEvent;

// Main menu functionality
void flip_crypt_menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
        case MenuIndexAtbash:
            scene_manager_handle_custom_event(app->scene_manager, EventAtbash);
            break;
        case MenuIndexBaconian:
            scene_manager_handle_custom_event(app->scene_manager, EventBaconian);
            break;
        case MenuIndexPlayfair:
            scene_manager_handle_custom_event(app->scene_manager, EventPlayfair);
            break;
        case MenuIndexRailfence:
            scene_manager_handle_custom_event(app->scene_manager, EventRailfence);
            break;
        case MenuIndexVigenere:
            scene_manager_handle_custom_event(app->scene_manager, EventVigenere);
            break;
        case MenuIndexBlake2:
            scene_manager_handle_custom_event(app->scene_manager, EventBlake2);
            break;
        case MenuIndexMD5:
            scene_manager_handle_custom_event(app->scene_manager, EventMD5);
            break;
        case MenuIndexSHA1:
            scene_manager_handle_custom_event(app->scene_manager, EventSHA1);
            break;
        case MenuIndexSHA256:
            scene_manager_handle_custom_event(app->scene_manager, EventSHA256);
            break;
        case MenuIndexAbout:
            scene_manager_handle_custom_event(app->scene_manager, EventAbout);
            break;
    }
}

// Main menu initialization
void flip_crypt_main_menu_scene_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "FlipCrypt");
    submenu_add_item(app->submenu, "Atbash Cipher", MenuIndexAtbash, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "Baconian Cipher", MenuIndexBaconian, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "Playfair Cipher", MenuIndexPlayfair, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "Railfence Cipher", MenuIndexRailfence, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "Vigenere Cipher", MenuIndexVigenere, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "BLAKE-2 Hash", MenuIndexBlake2, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "MD5 Hash", MenuIndexMD5, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "SHA-1 Hash", MenuIndexSHA1, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "SHA-256 Hash", MenuIndexSHA256, flip_crypt_menu_callback, app);
    submenu_add_item(app->submenu, "About", MenuIndexAbout, flip_crypt_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptSubmenuView);
}

// More main menu functionality
bool flip_crypt_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
            case EventAtbash:
                scene_manager_next_scene(app->scene_manager, FlipCryptAtbashSubmenuScene);
                consumed = true;
                break;
            case EventBaconian:
                scene_manager_next_scene(app->scene_manager, FlipCryptBaconianSubmenuScene);
                consumed = true;
                break;
            case EventPlayfair:
                scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairSubmenuScene);
                consumed = true;
                break;
            case EventRailfence:
                scene_manager_next_scene(app->scene_manager, FlipCryptRailfenceSubmenuScene);
                consumed = true;
                break;
            case EventVigenere:
                scene_manager_next_scene(app->scene_manager, FlipCryptVigenereSubmenuScene);
                consumed = true;
                break;
            case EventBlake2:
                scene_manager_next_scene(app->scene_manager, FlipCryptBlake2SubmenuScene);
                consumed = true;
                break;
            case EventMD5:
                scene_manager_next_scene(app->scene_manager, FlipCryptMD5SubmenuScene);
                consumed = true;
                break;
            case EventSHA1:
                scene_manager_next_scene(app->scene_manager, FlipCryptSHA1SubmenuScene);
                consumed = true;
                break;
            case EventSHA256:
                scene_manager_next_scene(app->scene_manager, FlipCryptSHA256SubmenuScene);
                consumed = true;
                break;
            case EventAbout:
                scene_manager_next_scene(app->scene_manager, FlipCryptAboutScene);
                consumed = true;
                break;
        }
    }
    return consumed;
}

void flip_crypt_main_menu_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// Cipher / Hash 'Encrypt' submenu option functionality
void cipher_encrypt_submenu_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    App* app = context;
    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptAtbashInputScene);
            break;
        case FlipCryptBaconianSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBaconianInputScene);
            break;
        case FlipCryptPlayfairSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairKeywordInputScene);
            break;
        case FlipCryptRailfenceSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptRailfenceInputScene);
            break;
        case FlipCryptVigenereSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptVigenereKeywordInputScene);
            break;
        case FlipCryptBlake2SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBlake2InputScene);
            break;
        case FlipCryptMD5SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptMD5InputScene);
            break;
        case FlipCryptSHA1SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptSHA1InputScene);
            break;
        case FlipCryptSHA256SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptSHA256InputScene);
            break;
        default:
            break;
    }
}

// Cipher / Hash 'Decrypt' submenu option functionality
void cipher_decrypt_submenu_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    App* app = context;
    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptAtbashDecryptInputScene);
            break;
        case FlipCryptBaconianSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBaconianDecryptInputScene);
            break;
        case FlipCryptPlayfairSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairDecryptKeywordInputScene);
            break;
        case FlipCryptRailfenceSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptRailfenceDecryptInputScene);
            break;
        case FlipCryptVigenereSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptVigenereDecryptKeywordInputScene);
            break;
        // Can't decrypt hashes so they are absent
        default:
            break;
    }
}

// Cipher / Hash 'Learn' submenu option functionality
void cipher_learn_submenu_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    App* app = context;
    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptAtbashLearnScene);
            break;
        case FlipCryptBaconianSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBaconianLearnScene);
            break;
        case FlipCryptPlayfairSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairLearnScene);
            break;
        case FlipCryptRailfenceSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptRailfenceLearnScene);
            break;
        case FlipCryptVigenereSubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptVigenereLearnScene);
            break;
        case FlipCryptBlake2SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBlake2LearnScene);
            break;
        case FlipCryptMD5SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptMD5LearnScene);
            break;
        case FlipCryptSHA1SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptSHA1LearnScene);
            break;
        case FlipCryptSHA256SubmenuScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptSHA256LearnScene);
            break;
        default:
            break;
    }
}

void cipher_do_nothing_submenu_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    App* app = context;
    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        default:
            break;
    }
}

// Cipher / Hash submenu initialization
void cipher_submenu_scene_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashSubmenuScene:
            submenu_set_header(app->submenu, "Atbash Cipher");
            submenu_add_item(app->submenu, "Encrypt Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Decrypt Text", 1, cipher_decrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 2, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptBaconianSubmenuScene:
            submenu_set_header(app->submenu, "Baconian Cipher");
            submenu_add_item(app->submenu, "Encrypt Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Decrypt Text", 1, cipher_decrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 2, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptPlayfairSubmenuScene:
            submenu_set_header(app->submenu, "Playfair Cipher");
            submenu_add_item(app->submenu, "Encrypt Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Decrypt Text", 1, cipher_decrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 2, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptRailfenceSubmenuScene:
            submenu_set_header(app->submenu, "Railfence Cipher");
            submenu_add_item(app->submenu, "Encrypt Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Decrypt Text", 1, cipher_decrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Rails: 3", 1, cipher_do_nothing_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 2, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptVigenereSubmenuScene:
            submenu_set_header(app->submenu, "Vigenere Cipher");
            submenu_add_item(app->submenu, "Encrypt Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Decrypt Text", 1, cipher_decrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 2, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptBlake2SubmenuScene:
            submenu_set_header(app->submenu, "BLAKE-2s Hash");
            submenu_add_item(app->submenu, "Hash Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 1, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptMD5SubmenuScene:
            submenu_set_header(app->submenu, "MD5 Hash");
            submenu_add_item(app->submenu, "Hash Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 1, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptSHA1SubmenuScene:
            submenu_set_header(app->submenu, "SHA1 Hash");
            submenu_add_item(app->submenu, "Hash Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 1, cipher_learn_submenu_callback, app);
            break;
        case FlipCryptSHA256SubmenuScene:
            submenu_set_header(app->submenu, "SHA256 Hash");
            submenu_add_item(app->submenu, "Hash Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 1, cipher_learn_submenu_callback, app);
            break;
        default:
            submenu_set_header(app->submenu, "Unknown");
            submenu_add_item(app->submenu, "Hash Text", 0, cipher_encrypt_submenu_callback, app);
            submenu_add_item(app->submenu, "Learn", 1, cipher_learn_submenu_callback, app);
            break;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptSubmenuView);
}

// Moves view forward from input to output scenes
void flip_crypt_text_input_callback(void* context) {
    App* app = context;
    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptAtbashOutputScene);
            break;
        case FlipCryptAtbashDecryptInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptAtbashDecryptOutputScene);
            break;
        case FlipCryptBaconianInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBaconianOutputScene);
            break;
        case FlipCryptBaconianDecryptInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBaconianDecryptOutputScene);
            break;
        case FlipCryptPlayfairKeywordInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairInputScene);
            break;
        case FlipCryptPlayfairInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairOutputScene);
            break;
        case FlipCryptPlayfairDecryptKeywordInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairDecryptInputScene);
            break;
        case FlipCryptPlayfairDecryptInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptPlayfairDecryptOutputScene);
            break;
        case FlipCryptRailfenceInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptRailfenceOutputScene);
            break;
        case FlipCryptRailfenceDecryptInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptRailfenceDecryptOutputScene);
            break;
        case FlipCryptVigenereInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptVigenereOutputScene);
            break;
        case FlipCryptVigenereKeywordInputScene: 
            scene_manager_next_scene(app->scene_manager, FlipCryptVigenereInputScene);
            break;
        case FlipCryptVigenereDecryptKeywordInputScene: 
            scene_manager_next_scene(app->scene_manager, FlipCryptVigenereDecryptInputScene);
            break;
        case FlipCryptVigenereDecryptInputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptVigenereDecryptOutputScene);
            break;
        case FlipCryptBlake2InputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptBlake2OutputScene);
            break;
        case FlipCryptMD5InputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptMD5OutputScene);
            break;
        case FlipCryptSHA1InputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptSHA1OutputScene);
            break;
        case FlipCryptSHA256InputScene:
            scene_manager_next_scene(app->scene_manager, FlipCryptSHA256OutputScene);
            break;
        default:
            break;
    }
}

// Text input views
void cipher_input_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter text");

    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->atbash_input, app->atbash_input_size, true);
            break;
        case FlipCryptAtbashDecryptInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->atbash_decrypt_input, app->atbash_decrypt_input_size, true);
            break;
        case FlipCryptBaconianInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->baconian_input, app->baconian_input_size, true);
            break;
        case FlipCryptBaconianDecryptInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->baconian_decrypt_input, app->baconian_decrypt_input_size, true);
            break;
        case FlipCryptPlayfairInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->playfair_input, app->playfair_input_size, true);
            break;
        case FlipCryptPlayfairKeywordInputScene:
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Enter Key");

            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->playfair_keyword_input, app->playfair_keyword_input_size, true);
            break;
        case FlipCryptPlayfairDecryptKeywordInputScene:
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Enter Key");

            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->playfair_keyword_input, app->playfair_keyword_input_size, true);
            break;
        case FlipCryptPlayfairDecryptInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->playfair_decrypt_input, app->playfair_decrypt_input_size, true);
            break;
        case FlipCryptRailfenceInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->railfence_input, app->railfence_input_size, true);
            break;
        case FlipCryptRailfenceDecryptInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->railfence_decrypt_input, app->railfence_decrypt_input_size, true);
            break;
        case FlipCryptVigenereKeywordInputScene:
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Enter Key");
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->vigenere_keyword_input, app->vigenere_keyword_input_size, true);
            break;
        case FlipCryptVigenereDecryptKeywordInputScene:
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Enter Key");
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->vigenere_keyword_input, app->vigenere_keyword_input_size, true);
            break;
        case FlipCryptVigenereInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->vigenere_input, app->vigenere_input_size, true);
            break;
        case FlipCryptVigenereDecryptInputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->vigenere_decrypt_input, app->vigenere_decrypt_input_size, true);
            break;
        case FlipCryptBlake2InputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->blake2_input, app->blake2_input_size, true);
            break;
        case FlipCryptMD5InputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->md5_input, app->md5_input_size, true);
            break;
        case FlipCryptSHA1InputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->sha1_input, app->sha1_input_size, true);
            break;
        case FlipCryptSHA256InputScene:
            text_input_set_result_callback(app->text_input, flip_crypt_text_input_callback,
                                           app, app->sha256_input, app->sha256_input_size, true);
            break;
        default:
            break;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptTextInputView);
}

// Output screen views
void cipher_output_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    FuriString* message = furi_string_alloc();

    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                atbash_encrypt_or_decrypt(app->atbash_input));
            break;
        case FlipCryptBaconianOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                baconian_encrypt(app->baconian_input));
            break;
        case FlipCryptPlayfairOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                playfair_encrypt(app->playfair_input, playfair_make_table(app->playfair_keyword_input)));
            break;
        case FlipCryptRailfenceOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                rail_fence_encrypt(app->railfence_input, 3));
            break;
        case FlipCryptVigenereOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                vigenere_cipher(app->vigenere_input, app->vigenere_keyword_input));
            break;
        case FlipCryptBlake2OutputScene:
            Blake2sContext blake2_ctx;
            uint8_t blake2_hash[BLAKE2S_OUTLEN];
            char blake2_hex_output[BLAKE2S_OUTLEN * 2 + 1] = {0};
            blake2s_init(&blake2_ctx);
            blake2s_update(&blake2_ctx, (const uint8_t*)app->blake2_input, strlen(app->blake2_input));
            blake2s_finalize(&blake2_ctx, blake2_hash);
            blake2s_to_hex(blake2_hash, blake2_hex_output);
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                blake2_hex_output);
            break;
        case FlipCryptMD5OutputScene:
            uint8_t md5_hash[16];
            char md5_hex_output[33] = {0};
            MD5Context md5_ctx;
            md5_init(&md5_ctx);
            md5_update(&md5_ctx, (const uint8_t*)app->md5_input, strlen(app->md5_input));
            md5_finalize(&md5_ctx, md5_hash);
            md5_to_hex(md5_hash, md5_hex_output);
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                md5_hex_output);
            break;
        case FlipCryptSHA1OutputScene:
            Sha1Context sha1_ctx;
            uint8_t sha1_hash[20];
            char sha1_hex_output[41] = {0};
            sha1_init(&sha1_ctx);
            sha1_update(&sha1_ctx, (const uint8_t*)app->sha1_input, strlen(app->sha1_input));
            sha1_finalize(&sha1_ctx, sha1_hash);
            sha1_to_hex(sha1_hash, sha1_hex_output);
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                sha1_hex_output);
            break;
        case FlipCryptSHA256OutputScene:
            Sha256Context sha256_ctx;
            uint8_t sha256_hash[32];
            char sha256_hex_output[65] = {0};

            sha256_init(&sha256_ctx);
            sha256_update(&sha256_ctx, (const uint8_t*)app->sha256_input, strlen(app->sha256_input));
            sha256_finalize(&sha256_ctx, sha256_hash);

            sha256_to_hex(sha256_hash, sha256_hex_output);

            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                sha256_hex_output);
            break;
        case FlipCryptAtbashDecryptOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                atbash_encrypt_or_decrypt(app->atbash_decrypt_input));
            break;
        case FlipCryptBaconianDecryptOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                baconian_decrypt(app->baconian_decrypt_input));
            break;
        case FlipCryptPlayfairDecryptOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                playfair_decrypt(app->playfair_decrypt_input, playfair_make_table(app->playfair_keyword_input)));
            break;
        case FlipCryptRailfenceDecryptOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                rail_fence_decrypt(app->railfence_input, 3));
            break;
        case FlipCryptVigenereDecryptOutputScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                vigenere_cipher(app->vigenere_decrypt_input, app->vigenere_keyword_input));
            break;
        default:
            furi_string_printf(message, "Unknown output scene.");
            break;
    }

    widget_add_string_multiline_element(app->widget, 5, 15, AlignLeft, AlignCenter, FontPrimary, furi_string_get_cstr(message));
    furi_string_free(message);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
}

// Learn screen views
void cipher_learn_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    FlipCryptScene current = scene_manager_get_current_scene(app->scene_manager);
    switch(current) {
        case FlipCryptAtbashLearnScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "The Atbash cipher is a classical substitution cipher originally used for the Hebrew alphabet. It works by reversing the alphabet so that the first letter becomes the last, the second becomes the second-to-last, and so on. For example, in the Latin alphabet, \'A\' becomes \'Z\', \'B\' becomes \'Y\', and \'C\' becomes \'X\'. It is a simple and symmetric cipher, meaning that the same algorithm is used for both encryption and decryption. Though not secure by modern standards, the Atbash cipher is often studied for its historical significance and simplicity.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptBaconianLearnScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "The Baconian cipher, developed by Francis Bacon in the early 17th century, is a steganographic cipher that encodes each letter of the alphabet into a series of five binary characters, typically using combinations of \'A\' and \'B\'. For example, the letter \'A\' is represented as \'AAAAA\', \'B\' as \'AAAAB\', and so on. This binary code can then be hidden within text, images, or formatting—making it a method of concealed rather than encrypted communication. The Baconian cipher is notable for being an early example of steganography and is often used in historical or educational contexts.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptPlayfairLearnScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "The Playfair cipher is a manual symmetric encryption technique invented in 1854 by Charles Wheatstone but popularized by Lord Playfair. It encrypts pairs of letters (digraphs) instead of single letters, making it more secure than simple substitution ciphers. The cipher uses a 5x5 grid of letters constructed from a keyword, combining \'I\' and \'J\' to fit the alphabet into 25 cells. To encrypt, each pair of letters is located in the grid, and rules based on their positions—same row, same column, or rectangle—are applied to substitute them with new letters. The Playfair cipher was used historically for military communication due to its relative ease of use and stronger encryption for its time.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptRailfenceLearnScene:
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "The Rail Fence cipher is a form of transposition cipher that rearranges the letters of the plaintext in a zigzag pattern across multiple \'rails\' (or rows), and then reads them off row by row to create the ciphertext. For example, using 3 rails, the message \'HELLO WORLD\' would be written in a zigzag across three lines and then read horizontally to produce the encrypted message. It\'s a simple method that relies on obscuring the letter order rather than substituting characters, and it\'s relatively easy to decrypt with enough trial and error.");
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptVigenereLearnScene:
                widget_add_text_scroll_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    "The Vigenere cipher is a classical polyalphabetic substitution cipher that uses a keyword to determine the shift for each letter of the plaintext. Each letter of the keyword corresponds to a Caesar cipher shift, which is applied cyclically over the plaintext. For example, with the keyword \'KEY\', the first letter of the plaintext is shifted by the position of \'K\', the second by \'E\', and so on. This method makes frequency analysis more difficult than in simple substitution ciphers, and it was considered unbreakable for centuries until modern cryptanalysis techniques were developed.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptBlake2LearnScene:
                widget_add_text_scroll_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    "BLAKE2 is a high-performance cryptographic hash function designed as an improved alternative to MD5, SHA-1, and even SHA-2, offering strong security with faster hashing speeds. Developed by Jean-Philippe Aumasson and others, it builds on the cryptographic foundations of the BLAKE algorithm (a finalist in the SHA-3 competition) but is optimized for practical use. BLAKE2 comes in two main variants: BLAKE2b (optimized for 64-bit platforms) and BLAKE2s (for 8- to 32-bit platforms). It provides features like keyed hashing, salting, and personalization, making it suitable for applications like password hashing, digital signatures, and message authentication. BLAKE2 is widely adopted due to its balance of speed, simplicity, and security, and is used in software like Argon2 (a password hashing algorithm) and various blockchain projects.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptMD5LearnScene:
                widget_add_text_scroll_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    "MD5 (Message-Digest Algorithm 5) is a widely used cryptographic hash function that produces a 128-bit (16-byte) hash value, typically rendered as a 32-character hexadecimal number. Originally designed for digital signatures and file integrity verification, MD5 is now considered cryptographically broken due to known collision vulnerabilities. While still used in some non-security-critical contexts, it is not recommended for new cryptographic applications.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptSHA1LearnScene:
                widget_add_text_scroll_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    "SHA-1 (Secure Hash Algorithm 1) is a cryptographic hash function that produces a 160-bit (20-byte) hash value. Once widely used in SSL certificates and digital signatures, SHA-1 has been deprecated due to demonstrated collision attacks, where two different inputs produce the same hash. As a result, it\'s no longer considered secure for cryptographic purposes and has largely been replaced by stronger alternatives.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        case FlipCryptSHA256LearnScene:
                widget_add_text_scroll_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    "SHA-256 is part of the SHA-2 family of cryptographic hash functions and generates a 256-bit (32-byte) hash value. It is currently considered secure and is widely used in blockchain, password hashing, digital signatures, and data integrity verification. SHA-256 offers strong resistance against collision and preimage attacks, making it a trusted standard in modern cryptography.");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
            break;
        default:
            break;
    }
}

// About screen from main menu option
void flip_crypt_about_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "FlipCrypt\nv0.1\nEncrypt, decrypt, and hash text using a variety of classic and modern crypto tools.\n\nAuthor: @taxelanderson\nSource Code: https://github.com/TAxelAnderson/FlipCrypt");
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
}

// Event handlers
bool flip_crypt_generic_event_handler(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flip_crypt_generic_on_exit(void* context) {
    UNUSED(context);
}

void (*const flip_crypt_scene_on_enter_handlers[])(void*) = {
    flip_crypt_main_menu_scene_on_enter,
    cipher_submenu_scene_on_enter, // AtbashSubmenu
    cipher_input_scene_on_enter,   // AtbashInput
    cipher_output_scene_on_enter,  // AtbashOutput
    cipher_input_scene_on_enter,   // AtbashDecryptInput
    cipher_output_scene_on_enter,  // AtbashDecryptOutput
    cipher_learn_scene_on_enter,   // AtbashLearn
    cipher_submenu_scene_on_enter, // BaconianSubmenu
    cipher_input_scene_on_enter,   // BaconianInput
    cipher_output_scene_on_enter,  // BaconianOutput
    cipher_input_scene_on_enter,   // BaconianDecryptInput
    cipher_output_scene_on_enter,  // BaconianDecryptOutput
    cipher_learn_scene_on_enter,   // BaconianLearn
    cipher_submenu_scene_on_enter, // PlayfairSubmenu
    cipher_input_scene_on_enter,   // PlayfairInput
    cipher_input_scene_on_enter,   // PlayfairKeywordInput
    cipher_input_scene_on_enter,   // PlayfairDecryptKeywordInput
    cipher_output_scene_on_enter,  // PlayfairOutput
    cipher_input_scene_on_enter,   // PlayfairDecryptInput
    cipher_output_scene_on_enter,  // PlayfairDecryptOutput
    cipher_learn_scene_on_enter,   // PlayfairLearn
    cipher_submenu_scene_on_enter, // RailfenceSubmenu
    cipher_input_scene_on_enter,   // RailfenceInput
    cipher_output_scene_on_enter,  // RailfenceOutput
    cipher_input_scene_on_enter,   // RailfenceDecryptInput
    cipher_output_scene_on_enter,  // RailfenceDecryptOutput
    cipher_learn_scene_on_enter,   // RailfenceLearn
    cipher_submenu_scene_on_enter, // VigenereSubmenu
    cipher_input_scene_on_enter,   // VigenereInput
    cipher_input_scene_on_enter,   // VigenereKeywordInput
    cipher_input_scene_on_enter,   // VigenereDecryptKeywordInput
    cipher_output_scene_on_enter,  // VigenereOutput
    cipher_input_scene_on_enter,   // VigenereDecryptInput
    cipher_output_scene_on_enter,  // VigenereDecryptOutput
    cipher_learn_scene_on_enter,   // VigenereLearn
    cipher_submenu_scene_on_enter, // Blake2Submenu
    cipher_input_scene_on_enter,   // Blake2Input
    cipher_output_scene_on_enter,  // Blake2Output
    cipher_learn_scene_on_enter,   // Blake2Learn
    cipher_submenu_scene_on_enter, // MD5Submenu
    cipher_input_scene_on_enter,   // MD5Input
    cipher_output_scene_on_enter,  // MD5Output
    cipher_learn_scene_on_enter,   // MD5Learn
    cipher_submenu_scene_on_enter, // SHA1Submenu
    cipher_input_scene_on_enter,   // SHA1Input
    cipher_output_scene_on_enter,  // SHA1Output
    cipher_learn_scene_on_enter,   // SHA1Learn
    cipher_submenu_scene_on_enter, // SHA256Submenu
    cipher_input_scene_on_enter,   // SHA256Input
    cipher_output_scene_on_enter,  // SHA256Output
    cipher_learn_scene_on_enter,   // SHA256Learn
    flip_crypt_about_scene_on_enter,
};

bool (*const flip_crypt_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    flip_crypt_main_menu_scene_on_event,
    flip_crypt_generic_event_handler, // AtbashSubmenu
    flip_crypt_generic_event_handler, // AtbashInput
    flip_crypt_generic_event_handler, // AtbashOutput
    flip_crypt_generic_event_handler, // AtbashDecryptInput
    flip_crypt_generic_event_handler, // AtbashDecryptOutput
    flip_crypt_generic_event_handler, // AtbashLearn
    flip_crypt_generic_event_handler, // BaconianSubmenu
    flip_crypt_generic_event_handler, // BaconianInput
    flip_crypt_generic_event_handler, // BaconianOutput
    flip_crypt_generic_event_handler, // BaconianDecryptInput
    flip_crypt_generic_event_handler, // BaconianDecryptOutput
    flip_crypt_generic_event_handler, // BaconianLearn
    flip_crypt_generic_event_handler, // PlayfairSubmenu
    flip_crypt_generic_event_handler, // PlayfairInput
    flip_crypt_generic_event_handler, // PlayfairKeywordInput
    flip_crypt_generic_event_handler, // PlayfairDecryptKeywordInput
    flip_crypt_generic_event_handler, // PlayfairOutput
    flip_crypt_generic_event_handler, // PlayfairDecryptInput
    flip_crypt_generic_event_handler, // PlayfairDecryptOutput
    flip_crypt_generic_event_handler, // PlayfairLearn
    flip_crypt_generic_event_handler, // RailfenceSubmenu
    flip_crypt_generic_event_handler, // RailfenceInput
    flip_crypt_generic_event_handler, // RailfenceOutput
    flip_crypt_generic_event_handler, // RailfenceDecryptInput
    flip_crypt_generic_event_handler, // RailfenceDecryptOutput
    flip_crypt_generic_event_handler, // RailfenceLearn
    flip_crypt_generic_event_handler, // VigenereSubmenu
    flip_crypt_generic_event_handler, // VigenereInput
    flip_crypt_generic_event_handler, // VigenereKeywordInput
    flip_crypt_generic_event_handler, // VigenereDecryptKeywordInput
    flip_crypt_generic_event_handler, // VigenereOutput
    flip_crypt_generic_event_handler, // VigenereDecryptInput
    flip_crypt_generic_event_handler, // VigenereDecryptOutput
    flip_crypt_generic_event_handler, // VigenereLearn
    flip_crypt_generic_event_handler, // Blake2Submenu
    flip_crypt_generic_event_handler, // Blake2Input
    flip_crypt_generic_event_handler, // Blake2Output
    flip_crypt_generic_event_handler, // Blake2Learn
    flip_crypt_generic_event_handler, // MD5Submenu
    flip_crypt_generic_event_handler, // MD5Input
    flip_crypt_generic_event_handler, // MD5Output
    flip_crypt_generic_event_handler, // MD5Learn
    flip_crypt_generic_event_handler, // SHA1Submenu
    flip_crypt_generic_event_handler, // SHA1Input
    flip_crypt_generic_event_handler, // SHA1Output
    flip_crypt_generic_event_handler, // SHA1Learn
    flip_crypt_generic_event_handler, // SHA256Submenu
    flip_crypt_generic_event_handler, // SHA256Input
    flip_crypt_generic_event_handler, // SHA256Output
    flip_crypt_generic_event_handler, // SHA256Learn
    flip_crypt_generic_event_handler, // About
};

void (*const flip_crypt_scene_on_exit_handlers[])(void*) = {
    flip_crypt_main_menu_scene_on_exit,
    flip_crypt_generic_on_exit, // AtbashSubmenu
    flip_crypt_generic_on_exit, // AtbashInput
    flip_crypt_generic_on_exit, // AtbashOutput
    flip_crypt_generic_on_exit, // AtbashDecryptInput
    flip_crypt_generic_on_exit, // AtbashDecryptOutput
    flip_crypt_generic_on_exit, // AtbashLearn
    flip_crypt_generic_on_exit, // BaconianSubmenu
    flip_crypt_generic_on_exit, // BaconianInput
    flip_crypt_generic_on_exit, // BaconianOutput
    flip_crypt_generic_on_exit, // BaconianDecryptInput
    flip_crypt_generic_on_exit, // BaconianDecryptOutput
    flip_crypt_generic_on_exit, // BaconianLearn
    flip_crypt_generic_on_exit, // PlayfairSubmenu
    flip_crypt_generic_on_exit, // PlayfairInput
    flip_crypt_generic_on_exit, // PlayfairKeywordInput
    flip_crypt_generic_on_exit, // PlayfairDecryptKeywordInput
    flip_crypt_generic_on_exit, // PlayfairOutput
    flip_crypt_generic_on_exit, // PlayfairDecryptInput
    flip_crypt_generic_on_exit, // PlayfairDecryptOutput
    flip_crypt_generic_on_exit, // PlayfairLearn
    flip_crypt_generic_on_exit, // RailfenceSubmenu
    flip_crypt_generic_on_exit, // RailfenceInput
    flip_crypt_generic_on_exit, // RailfenceOutput
    flip_crypt_generic_on_exit, // RailfenceDecryptInput
    flip_crypt_generic_on_exit, // RailfenceDecryptOutput
    flip_crypt_generic_on_exit, // RailfenceLearn
    flip_crypt_generic_on_exit, // VigenereSubmenu
    flip_crypt_generic_on_exit, // VigenereInput
    flip_crypt_generic_on_exit, // VigenereKeywordInput
    flip_crypt_generic_on_exit, // VigenereDecryptKeywordInput
    flip_crypt_generic_on_exit, // VigenereOutput
    flip_crypt_generic_on_exit, // VigenereDecryptInput
    flip_crypt_generic_on_exit, // VigenereDecryptOutput
    flip_crypt_generic_on_exit, // VigenereLearn
    flip_crypt_generic_on_exit, // Blake2Submenu
    flip_crypt_generic_on_exit, // Blake2Input
    flip_crypt_generic_on_exit, // Blake2Output
    flip_crypt_generic_on_exit, // Blake2Learn
    flip_crypt_generic_on_exit, // MD5Submenu
    flip_crypt_generic_on_exit, // MD5Input
    flip_crypt_generic_on_exit, // MD5Output
    flip_crypt_generic_on_exit, // MD5Learn
    flip_crypt_generic_on_exit, // SHA1Submenu
    flip_crypt_generic_on_exit, // SHA1Input
    flip_crypt_generic_on_exit, // SHA1Output
    flip_crypt_generic_on_exit, // SHA1Learn
    flip_crypt_generic_on_exit, // SHA256Submenu
    flip_crypt_generic_on_exit, // SHA256Input
    flip_crypt_generic_on_exit, // SHA256Output
    flip_crypt_generic_on_exit, // SHA256Learn
    flip_crypt_generic_on_exit, // About
};

static const SceneManagerHandlers flip_crypt_scene_manager_handlers = {
    .on_enter_handlers = flip_crypt_scene_on_enter_handlers,
    .on_event_handlers = flip_crypt_scene_on_event_handlers,
    .on_exit_handlers = flip_crypt_scene_on_exit_handlers,
    .scene_num = FlipCryptSceneCount,
};

// Custom event callbacks
static bool basic_scene_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

bool basic_scene_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// Memory allocation
static App* app_alloc() {
    // App
    App* app = malloc(sizeof(App));
    // Vars
    app->atbash_input_size = 64;
    app->atbash_decrypt_input_size = 64;
    app->baconian_input_size = 64;
    app->baconian_decrypt_input_size = 128;
    app->playfair_input_size = 64;
    app->playfair_keyword_input_size = 26;
    app->playfair_decrypt_input_size = 64;
    app->railfence_input_size = 64;
    app->railfence_decrypt_input_size = 64;
    app->vigenere_input_size = 64;
    app->vigenere_keyword_input_size = 64;
    app->vigenere_decrypt_input_size = 64;
    app->blake2_input_size = 64;
    app->md5_input_size = 64;
    app->sha1_input_size = 64;
    app->sha256_input_size = 64;
    app->atbash_input = malloc(app->atbash_input_size);
    app->atbash_decrypt_input = malloc(app->atbash_decrypt_input_size);
    app->baconian_input = malloc(app->baconian_input_size);
    app->baconian_decrypt_input = malloc(app->baconian_decrypt_input_size);
    app->playfair_input = malloc(app->playfair_input_size);
    app->playfair_keyword_input = malloc(app->playfair_keyword_input_size);
    app->playfair_decrypt_input = malloc(app->playfair_decrypt_input_size);
    app->railfence_input = malloc(app->railfence_input_size);
    app->railfence_decrypt_input = malloc(app->railfence_decrypt_input_size);
    app->vigenere_input = malloc(app->vigenere_input_size);
    app->vigenere_keyword_input = malloc(app->vigenere_keyword_input_size);
    app->vigenere_decrypt_input = malloc(app->vigenere_decrypt_input_size);
    app->blake2_input = malloc(app->blake2_input_size);
    app->md5_input = malloc(app->md5_input_size);
    app->sha1_input = malloc(app->sha1_input_size);
    app->sha256_input = malloc(app->sha256_input_size);
    // Other
    app->scene_manager = scene_manager_alloc(&flip_crypt_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, basic_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, basic_scene_back_event_callback);
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptSubmenuView, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptWidgetView, widget_get_view(app->widget));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptTextInputView, text_input_get_view(app->text_input));
    return app;
}

// Free memory on app termination
static void app_free(App* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptSubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptWidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptTextInputView);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    free(app->atbash_input);
    free(app->atbash_decrypt_input);
    free(app->baconian_input);
    free(app->baconian_decrypt_input);
    free(app->playfair_input);
    free(app->playfair_keyword_input);
    free(app->playfair_decrypt_input);
    free(app->railfence_input);
    free(app->railfence_decrypt_input);
    free(app->vigenere_input);
    free(app->vigenere_keyword_input);
    free(app->vigenere_decrypt_input);
    free(app->blake2_input);
    free(app->md5_input);
    free(app->sha1_input);
    free(app->sha256_input);
    free(app);
}

// Main function
int32_t flip_crypt_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FlipCryptMainMenuScene);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0; // return of 0 means success, return of 1 means failure
}