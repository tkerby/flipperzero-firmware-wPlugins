#pragma once

#include "../icon.h"
#include <iconedit_icons.h>

#define TOOL_WIDTH  14
#define TOOL_HEIGHT 14

typedef enum {
    Tool_START,
    Tool_New = Tool_START,
    Tool_Open,
    Tool_Save, // save popup
    Tool_Rename,
    Tool_Draw,
    Tool_Line,
    Tool_Circle,
    Tool_Rect,
    Tool_COUNT,
    Tool_NONE
} EditorTool;

extern const char* EditorTool_Desc[];
extern const Icon* EditorTool_Icon[]; // Actual Flipperzero icon

// Tool selection allowed movements
extern const EditorTool EditorTool_UP[Tool_COUNT];
extern const EditorTool EditorTool_DOWN[Tool_COUNT];
extern const EditorTool EditorTool_LEFT[Tool_COUNT];
extern const EditorTool EditorTool_RIGHT[Tool_COUNT];

typedef enum {
    Save_START,
    Save_PNG = Save_START,
    Save_C,
    Save_XBM,
    Save_COUNT,
    Save_NONE
} EditorToolSaveMode;

extern const char* EditorToolSaveModeStr[Save_COUNT];

extern const EditorToolSaveMode EditorToolSave_UP[Save_COUNT];
extern const EditorToolSaveMode EditorToolSave_DOWN[Save_COUNT];

IEIcon* tools_get_icon(EditorTool tool);
