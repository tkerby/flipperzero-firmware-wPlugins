#pragma once

#include "../bambu_rfid.h"

#include <gui/view_dispatcher.h>

enum class BrMagicFamily : uint8_t {
    Unknown,
    Gen1A,
    Gen1B,
    Gen1Alt,
    Mf4Unsupported,
    Ufuid,
    Gen2,
    Gen3,
    Gen4,
};

struct BrWriteResult {
    bool ok;
    bool locked;
    bool blanked;
    BrMagicFamily family;
    char family_name[24];
    char message[160];
};

struct BrWipeContext {
    ViewDispatcher* dispatcher;
    volatile bool* cancel;
    uint32_t done_event;
    uint32_t progress_event;
    BrWriteResult* result;
};

struct BrWriteContext {
    Storage* storage;
    ViewDispatcher* dispatcher;
    volatile bool* cancel;
    uint32_t done_event;
    uint32_t progress_event;
    char source_path[BR_PATH_MAX];
    BrWriteResult* result;
};

int32_t br_write_worker(void* context);
int32_t br_wipe_worker(void* context);
const char* br_magic_family_name(BrMagicFamily family);
