#pragma once

#include <dialogs/dialogs.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include <storage/storage.h>

#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>

#define NFC_APP_FOLDER    ANY_PATH("nfc")
#define NFC_APP_EXTENSION ".nfc"

// Enumeration of the view indexes.
typedef enum {
    ViewIndexWidget,
    ViewIndexSubmenu,
    ViewIndexCount,
} ViewIndex;

// Enumeration of submenu items.
typedef enum {
    SubmenuIndexDirectToTag,
    SubmenuIndexEditMiZipFile,
} SubmenuIndex;

// Main application structure.
typedef struct {
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    Submenu* submenu;

    DialogsApp* dialogs;

    FuriString* filePath;
} MiZipBalanceEditorApp;
