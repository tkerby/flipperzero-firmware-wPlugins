#include "pet_app_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define PET_APP_STATE_FILE_PATH      INT_PATH(".pet_app.state")
#define PET_APP_STATE_HEADER_MAGIC   0xDE
#define PET_APP_STATE_HEADER_VERSION 0x01

bool pet_app_state_save(const PetAppState* state) {
    if(!state) return false;

    bool success = saved_struct_save(
        PET_APP_STATE_FILE_PATH,
        (void*)state,
        sizeof(PetAppState),
        PET_APP_STATE_HEADER_MAGIC,
        PET_APP_STATE_HEADER_VERSION);
    if(!success) {
        FURI_LOG_E("PetAppState", "Failed to save app state");
    } else {
        FURI_LOG_I("PetAppState", "App state saved");
    }
    return success;
}

bool pet_app_state_load(PetAppState* state) {
    if(!state) return false;

    bool success = saved_struct_load(
        PET_APP_STATE_FILE_PATH,
        (void*)state,
        sizeof(PetAppState),
        PET_APP_STATE_HEADER_MAGIC,
        PET_APP_STATE_HEADER_VERSION);
    if(!success) {
        FURI_LOG_W("PetAppState", "No valid state found, or load error");
    } else {
        FURI_LOG_I("PetAppState", "App state loaded");
    }
    return success;
}
