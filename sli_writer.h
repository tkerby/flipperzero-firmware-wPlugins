#pragma once

// ============================================================================
// Includes (API Flipper, NFC, GUI, stockage, etc.)
// ============================================================================
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_nfc.h>

#include <nfc/nfc.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/loading.h>
#include <gui/scene_manager.h>

#include <dialogs/dialogs.h>   // <-- service Dialogs (wrappe le file browser)

#include <storage/storage.h>
#include <toolbox/bit_buffer.h>

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Constantes
// ============================================================================
#define SLI_WRITER_FILE_EXTENSION  ".nfc"  // Extension attendue pour les fichiers
#define SLI_MAGIC_BLOCK_SIZE       4       // Taille par défaut d'un bloc en octets
#define SLI_MAGIC_MAX_BLOCKS       64      // Nombre max de blocs supportés

// ============================================================================
// Énumérations pour les scènes
// ============================================================================
typedef enum {
    SliWriterSceneStart,       // Menu principal
    SliWriterSceneFileSelect,  // Sélection fichier
    SliWriterSceneWrite,       // Écriture / test (worker)
    SliWriterSceneSuccess,     // Succès
    SliWriterSceneError,       // Erreur
    SliWriterSceneNum,         // Nombre total de scènes
} SliWriterScene;

// ============================================================================
// Énumérations pour les vues
// ============================================================================
typedef enum {
    SliWriterViewSubmenu,
    SliWriterViewDialogEx,
    SliWriterViewLoading,
} SliWriterView;

// ============================================================================
// Énumérations pour le menu principal
// ============================================================================
typedef enum {
    SliWriterSubmenuIndexWrite,      // "Write NFC File"
    SliWriterSubmenuIndexTestMagic,  // "Test Magic INIT"
    SliWriterSubmenuIndexAbout,      // "About"
} SliWriterSubmenuIndex;

// ============================================================================
// Événements personnalisés
// ============================================================================
typedef enum {
    SliWriterCustomEventWriteSuccess, // Écriture terminée
    SliWriterCustomEventWriteError,   // Échec écriture
    SliWriterCustomEventParseError,   // Échec lecture fichier
} SliWriterCustomEvent;

// ============================================================================
// Contenu minimal d'un .nfc
// ============================================================================
typedef struct {
    uint8_t uid[8];                                         // UID à écrire
    uint8_t password_privacy[4];                            // Mot de passe Privacy
    uint8_t data[SLI_MAGIC_MAX_BLOCKS * SLI_MAGIC_BLOCK_SIZE]; // Données brutes
    uint8_t block_count;                                    // Nombre de blocs
    uint8_t block_size;                                     // Taille d’un bloc
} SliWriterNfcData;

// ============================================================================
// Structure principale de l'application
// ============================================================================
typedef struct {
    // UI et gestion de vues
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    DialogEx* dialog_ex;
    Loading* loading;

    // Services
    Storage* storage;
    DialogsApp* dialogs;  // service Dialogs pour le file browser

    // Chaînes pour chemin du fichier et messages d'erreur
    FuriString* file_path;
    FuriString* error_message;

    // Thread worker NFC
    FuriThread* worker_thread;

    // NFC et poller
    Nfc* nfc;
    bool nfc_started;
    NfcPoller* poller;

    // État courant
    bool test_mode;             // Mode test ou écriture
    bool have_uid;              // UID détecté
    uint8_t detected_uid[8];    // UID détecté

    // Données NFC à écrire
    SliWriterNfcData nfc_data;
} SliWriterApp;

// ============================================================================
// Prototypes
// ============================================================================
SliWriterApp* sli_writer_app_alloc(void);
void sli_writer_app_free(SliWriterApp* app);

bool sli_writer_parse_nfc_file(SliWriterApp* app, const char* file_path);
bool slix_writer_perform_write(SliWriterApp* app);
bool slix_writer_perform_test(SliWriterApp* app);

int32_t slix_writer_work_thread(void* context);

void sli_writer_submenu_callback(void* context, uint32_t index);
void sli_writer_dialog_ex_callback(DialogExResult result, void* context);
bool sli_writer_back_event_callback(void* context);
bool sli_writer_custom_event_callback(void* context, uint32_t event);

int32_t sli_writer_app(void* p);

