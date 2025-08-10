#pragma once

// ===== SDK / std =====
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <furi.h>
#include <furi_hal.h>

// ===== GUI / scenes / modules =====
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/loading.h>

// ===== Storage / dialogs =====
#include <storage/storage.h>
#include <dialogs/dialogs.h>

// ===== NFC (Unleashed 0.82) =====
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

// ===== Toolbox =====
#include <toolbox/bit_buffer.h>

//============================================================================
// Constantes
//============================================================================
#define SLI_WRITER_FILE_EXTENSION ".nfc"
#define SLI_WRITER_FILE_NAME_LEN  64

#define SLI_MAGIC_UID_SIZE        8
#define SLI_MAGIC_BLOCK_SIZE      4
#define SLI_MAGIC_MAX_BLOCKS      64

//============================================================================
// Énumérations
//============================================================================
typedef enum {
    SliWriterSceneStart = 0,
    SliWriterSceneFileSelect,
    SliWriterSceneWrite,
    SliWriterSceneSuccess,
    SliWriterSceneError,
    SliWriterSceneNum,
} SliWriterScene;

typedef enum {
    SliWriterViewSubmenu = 0,
    SliWriterViewDialogEx,
    SliWriterViewLoading,
} SliWriterView;

typedef enum {
    SliWriterSubmenuIndexWrite = 0,
    SliWriterSubmenuIndexTestMagic,   // nouveau: test INIT magic
    SliWriterSubmenuIndexAbout,
} SliWriterSubmenuIndex;

typedef enum {
    SliWriterCustomEventParseError = 1,
    SliWriterCustomEventWriteSuccess,
    SliWriterCustomEventWriteError,
    SliWriterCustomEventKickoff = 1001,   // différé: start NFC/poller + worker
} SliWriterCustomEvent;

//============================================================================
// Structures
//============================================================================

typedef struct {
    uint8_t uid[SLI_MAGIC_UID_SIZE];   // 8 bytes

    // Champs ISO15693/SLIX usuels utilisés/parsés
    uint8_t dsfid;
    uint8_t afi;
    uint8_t ic_reference;

    bool    lock_dsfid;
    bool    lock_afi;

    uint8_t block_count;               // nombre de blocs (<= 64 ici)
    uint8_t block_size;                // taille d’un bloc (4 pour SLIX)

    uint8_t data[SLI_MAGIC_MAX_BLOCKS * SLI_MAGIC_BLOCK_SIZE];
    uint8_t security_status[SLI_MAGIC_MAX_BLOCKS];

    // MDP SLIX (optionnels dans le .nfc)
    uint8_t password_privacy[4];
    uint8_t password_destroy[4];
    uint8_t password_eas[4];

    bool    privacy_mode;
    bool    lock_eas;
} SliWriterNfcData;

typedef struct {
    // --- GUI / Scenes / Views ---
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;

    Submenu*         submenu;
    DialogEx*        dialog_ex;
    Loading*         loading;

    // --- Services ---
    Storage*         storage;
    DialogsApp*      dialogs;

    // --- NFC runtime ---
    Nfc*             nfc;         // alloué une fois, start/stop par scène
    NfcPoller*       poller;      // poller ISO15693_3

    // UID vu sur la carte au moment de l’inventory
    uint8_t          detected_uid[SLI_MAGIC_UID_SIZE];
    bool             have_uid;

    // Flags de session
    bool             nfc_started;
    bool             test_mode;   // nouveau: si true -> n’envoie que INIT magic

    // --- Données parsees du .nfc ---
    SliWriterNfcData nfc_data;

    // --- Fichiers / messages ---
    FuriString*      file_path;
    char             file_name[SLI_WRITER_FILE_NAME_LEN];

    bool             write_success;
    FuriString*      error_message;

    // --- Worker thread ---
    FuriThread*      worker_thread;
} SliWriterApp;

//============================================================================
// Prototypes
//============================================================================

bool     sli_writer_parse_nfc_file(SliWriterApp* app, const char* file_path);
bool     slix_writer_perform_write(SliWriterApp* app);
bool     slix_writer_perform_test(SliWriterApp* app);
int32_t  slix_writer_work_thread(void* context);

// Scenes
void  sli_writer_scene_start_on_enter(void* context);
bool  sli_writer_scene_start_on_event(void* context, SceneManagerEvent event);
void  sli_writer_scene_start_on_exit(void* context);

void  sli_writer_scene_file_select_on_enter(void* context);
bool  sli_writer_scene_file_select_on_event(void* context, SceneManagerEvent event);
void  sli_writer_scene_file_select_on_exit(void* context);

void  sli_writer_scene_write_on_enter(void* context);
bool  sli_writer_scene_write_on_event(void* context, SceneManagerEvent event);
void  sli_writer_scene_write_on_exit(void* context);

void  sli_writer_scene_success_on_enter(void* context);
bool  sli_writer_scene_success_on_event(void* context, SceneManagerEvent event);
void  sli_writer_scene_success_on_exit(void* context);

void  sli_writer_scene_error_on_enter(void* context);
bool  sli_writer_scene_error_on_event(void* context, SceneManagerEvent event);
void  sli_writer_scene_error_on_exit(void* context);

// Callbacks
void  sli_writer_submenu_callback(void* context, uint32_t index);
void  sli_writer_dialog_ex_callback(DialogExResult result, void* context);
bool  sli_writer_back_event_callback(void* context);
bool  sli_writer_custom_event_callback(void* context, uint32_t event);

// Lifecycle
SliWriterApp* sli_writer_app_alloc(void);
void          sli_writer_app_free(SliWriterApp* app);

// Entry point
int32_t       sli_writer_app(void* p);
