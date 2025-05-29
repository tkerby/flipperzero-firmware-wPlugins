#include <gui/icon.h>
#include "editor_tools.h"
#include "../utils/file_utils.h"
#include <iconedit_icons.h>

const char* EditorTool_Names[Tool_COUNT] =
    {"New", "Open", "Save", "Rename", "Draw", "Line", "Circle", "Rect"};

const char* EditorTool_Desc[Tool_COUNT] = {
    "New Icon",
    "Open Icon",
    "Save To...",
    "Rename...",
    "Draw pixel",
    "Draw Line",
    "Draw Circle",
    "Draw Rect"};

// Tool selection allowed movements
const EditorTool EditorTool_UP[Tool_COUNT] =
    {Tool_NONE, Tool_NONE, Tool_NONE, Tool_New, Tool_Open, Tool_Save, Tool_Rename, Tool_Draw};
const EditorTool EditorTool_DOWN[Tool_COUNT] =
    {Tool_Rename, Tool_Draw, Tool_Line, Tool_Circle, Tool_Rect, Tool_NONE, Tool_NONE, Tool_NONE};
const EditorTool EditorTool_LEFT[Tool_COUNT] =
    {Tool_NONE, Tool_New, Tool_Open, Tool_NONE, Tool_Rename, Tool_Draw, Tool_NONE, Tool_Circle};
const EditorTool EditorTool_RIGHT[Tool_COUNT] =
    {Tool_Open, Tool_Save, Tool_NONE, Tool_Draw, Tool_Line, Tool_NONE, Tool_Rect, Tool_NONE};

IEIcon* icon_cache[Tool_COUNT] = {0};

const Icon* EditorTool_Icon[Tool_COUNT] = {
    [Tool_New] = &I_iet_New,
    [Tool_Open] = &I_iet_Open,
    [Tool_Save] = &I_iet_Save,
    [Tool_Rename] = &I_iet_Rename,
    [Tool_Draw] = &I_iet_Draw,
    [Tool_Line] = &I_iet_Line,
    [Tool_Circle] = &I_iet_Circle,
    [Tool_Rect] = &I_iet_Rect};

const char* EditorToolSaveModeStr[Save_COUNT] = {"PNG", ".C", "XBM"};
const EditorToolSaveMode EditorToolSave_UP[Save_COUNT] = {Save_NONE, Save_PNG, Save_C};
const EditorToolSaveMode EditorToolSave_DOWN[Save_COUNT] = {Save_C, Save_XBM, Save_NONE};

IEIcon* tools_get_icon(EditorTool tool) {
    // UNUSED(tool);

    if(icon_cache[tool]) {
        return icon_cache[tool];
    }

    // build an apps_data path for this tool's icon in .PNG
    const char* name = EditorTool_Names[tool];
    FuriString* icon_path = furi_string_alloc_printf("/ext/apps_data/iconedit/iet_%s.png", name);

    // if it's not found, we return null
    IEIcon* icon = png_file_open(furi_string_get_cstr(icon_path));
    if(icon) {
        icon_cache[tool] = icon;
    }

    furi_string_free(icon_path);
    return icon;
}
