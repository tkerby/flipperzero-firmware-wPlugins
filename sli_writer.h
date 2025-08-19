#pragma once

/* ============================================================================
 *  SLI Writer — Header
 *  - High-level scenes/worker flow preserved
 *  - Includes trimmed to match Unleashed / API 86 toolchain
 *  - Notes:
 *      * UID on ISO15693 is LSB->MSB on the wire; we keep that order internally.
 *      * FINALIZE payload toggle handled in .c via SLI_FINALIZE_WITH_PAYLOAD.
 * ========================================================================== */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_nfc.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>  // Pour nfc_detect
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/loading.h>
#include <gui/scene_manager.h>

#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <toolbox/bit_buffer.h>

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 *  Constants
 * ========================================================================== */

#define SLI_WRITER_FILE_EXTENSION  ".nfc"
#define SLI_MAGIC_BLOCK_SIZE       4
#define SLI_MAGIC_MAX_BLOCKS       64

/* Some “magic” cards accept FINALIZE without payload */
#define SLI_FINALIZE_WITH_PAYLOAD  1

/* ============================================================================
 *  Scenes / Views / Menu
 * ========================================================================== */

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
    SliWriterSubmenuIndexWrite = 0,     /* "Write NFC File" */
    SliWriterSubmenuIndexTestMagic,     /* "Read ISO15693 UID" */
    SliWriterSubmenuIndexAbout,         /* "About" */
} SliWriterSubmenuIndex;

typedef enum {
    SliWriterCustomEventWriteSuccess = 100,
    SliWriterCustomEventWriteError,
    SliWriterCustomEventParseError,
    SliWriterCustomEventPollerReady = 0xA100,  // Valeur directe, pas de #define
} SliWriterCustomEvent;

/* ============================================================================
 *  Parsed .nfc data (kept LSB->MSB for UID to match RF order)
 * ========================================================================== */

typedef struct {
    /* UID byte order note:
     * ISO15693 frames carry UID LSB->MSB. We keep that exact order here
     * to avoid accidental mirroring when building RF payloads. */
    uint8_t uid[8];
    uint8_t password_privacy[4];
    uint8_t data[SLI_MAGIC_MAX_BLOCKS * SLI_MAGIC_BLOCK_SIZE];
    uint8_t block_count;
    uint8_t block_size;
} SliWriterNfcData;

/* ============================================================================
 *  App context
 * ========================================================================== */

typedef struct {
    /* UI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    DialogEx* dialog_ex;
    Loading* loading;
    /* FS & dialogs */
    Storage* storage;
    DialogsApp* dialogs;
    /* Strings */
    FuriString* file_path;
    FuriString* error_message;
    /* Worker */
    FuriThread* worker_thread;
    /* NFC (high-level) */
    Nfc* nfc;
    NfcPoller* poller;
    /* State */
    bool test_mode;
    bool have_uid;
    uint8_t detected_uid[8];
    bool running;
    bool poller_ready;
    bool field_present;
    bool fatal_error;
    /* Data to write */
    SliWriterNfcData nfc_data;
} SliWriterApp;

/* ============================================================================
 *  API
 * ========================================================================== */

/* Lifecycle */
SliWriterApp* sli_writer_app_alloc(void);
void sli_writer_app_free(SliWriterApp* app);

/* File parsing + actions (implemented in .c) */
bool sli_writer_parse_nfc_file(SliWriterApp* app, const char* file_path);

/* Worker */
int32_t slix_writer_work_thread(void* context);

/* UI callbacks */
void sli_writer_submenu_callback(void* context, uint32_t index);
void sli_writer_dialog_ex_callback(DialogExResult result, void* context);
bool sli_writer_back_event_callback(void* context);
bool sli_writer_custom_event_callback(void* context, uint32_t event);

/* Entry point (FBT entry_point="sli_writer_app") */
int32_t sli_writer_app(void* p);
