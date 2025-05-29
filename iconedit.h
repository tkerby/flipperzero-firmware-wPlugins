#pragma once

#include <gui/gui.h>
#include <storage/storage.h>
#include <gui/modules/text_input.h>
#include "icon.h"
#include "utils/editor_tools.h"

// #include "views/editor.h"

#define TAG "IE"

typedef struct {
    InputEvent input;
} IconEditEvent;

typedef enum {
    Mode_Tools,
    Mode_Canvas
} EditorMode;

typedef enum {
    Draw_NONE,
    Draw_Point1_Set,
} DrawState;

typedef struct {
    IEIcon* icon;
    EditorMode mode; // selecting tool or working on canvas
    EditorTool tool; // selected tool
    bool in_save_mode;
    EditorToolSaveMode save_mode;
    size_t cursor_x;
    size_t cursor_y;
    DrawState draw_state;
    size_t p1x, p1y;
    uint8_t* double_buffer;

    FuriMutex* mutex;
    Storage* storage;
    FuriString* temp_str;
    char temp_cstr[64];
    bool running;
} IconEdit;
