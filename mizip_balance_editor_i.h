#pragma once

#include <furi.h>

#include <dialogs/dialogs.h>

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_box.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <dolphin/dolphin.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller.h>
#include <toolbox/keys_dict.h>

#include <mizip_balance_editor_icons.h>
#include "scenes/mizip_balance_editor_scene.h"

#include "mizip_balance_editor.h"
#include "adapted_from_ofw/mizip.h"

#define NFC_APP_FOLDER                      EXT_PATH("nfc")
#define NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH (NFC_APP_FOLDER "/assets/mf_classic_dict.nfc")
#define NFC_APP_EXTENSION                   ".nfc"
#define NFC_APP_SHADOW_EXTENSION            ".shd"
#define MIZIP_BALANCE_MIN_VALUE             0
#define MIZIP_BALANCE_MAX_VALUE             65535

// Enumeration of the view indexes.
typedef enum {
    MiZipBalanceEditorViewIdMainMenu,
    MiZipBalanceEditorViewIdScanner,
    MiZipBalanceEditorViewIdFileSelect,
    MiZipBalanceEditorViewIdNumberInput,
    MiZipBalanceEditorViewIdShowBalance,
    MiZipBalanceEditorViewIdWriteSuccess,
    MiZipBalanceEditorViewIdAbout,
} MiZipBalanceEditorViewId;

// Enumeration of submenu items.
typedef enum {
    SubmenuIndexDirectToTag,
    SubmenuIndexEditMiZipFile,
    SubmenuIndexAbout,
} SubmenuIndex;

enum MiZipBalanceEditorCustomEvent {
    MiZipBalanceEditorCustomEventCardDetected,
    MiZipBalanceEditorCustomEventCardLost,
    MiZipBalanceEditorCustomEventDictAttackDataUpdate,
    MiZipBalanceEditorCustomEventMfClassicCard,
    MiZipBalanceEditorCustomEventPollerSuccess,
    MiZipBalanceEditorCustomEventWrongCard,
    MiZipBalanceEditorCustomEventViewExit,
};

typedef struct {
    KeysDict* dict;
    uint8_t sectors_total;
    uint8_t sectors_read;
    uint8_t current_sector;
    uint8_t keys_found;
    size_t dict_keys_total;
    size_t dict_keys_current;
    bool is_key_attack;
    uint8_t key_attack_current_sector;
    bool is_card_present;
    MfClassicNestedPhase nested_phase;
    MfClassicPrngType prng_type;
    MfClassicBackdoor backdoor;
    uint16_t nested_target_key;
    uint16_t msb_count;
    bool enhanced_dict;
} NfcMfClassicDictAttackContext;

// Main application structure.
struct MiZipBalanceEditorApp {
    //GUI components
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    //Scenes
    Submenu* submenu;
    Popup* popup;
    DialogsApp* dialogs;
    DialogEx* dialog_ex;
    NumberInput* number_input;
    TextBox* text_box;

    SubmenuIndex last_selected_submenu_index;

    //NFC
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;
    NfcDevice* nfc_device;
    bool is_scan_active;

    //File
    Storage* storage;
    FuriString* filePath;
    FuriString* shadowFilePath;
    bool is_shadow_file_exists;

    //Mifare Classic data
    MfClassicData* mf_classic_data;
    NfcMfClassicDictAttackContext nfc_dict_context;
    bool is_valid_mizip_data;
    bool is_number_input_active;

    //MiZip data
    uint8_t uid[4];
    uint8_t credit_pointer;
    uint8_t previous_credit_pointer;
    uint16_t current_balance;
    uint16_t previous_balance;
    uint16_t new_balance;
};

bool mizip_balance_editor_write_new_balance(void* context);
