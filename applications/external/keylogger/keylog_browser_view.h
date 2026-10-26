#pragma once

#include <gui/view.h>
#include <furi.h>

typedef struct KeylogBrowserView KeylogBrowserView;

typedef void (*KeylogBrowserFileSelectedCallback)(void* context, const char* file_path);

KeylogBrowserView* keylog_browser_view_alloc(void);
void keylog_browser_view_free(KeylogBrowserView* browser_view);
View* keylog_browser_view_get_view(KeylogBrowserView* browser_view);

void keylog_browser_view_set_directory(KeylogBrowserView* browser_view, const char* directory_path);
void keylog_browser_view_set_file_selected_callback(
    KeylogBrowserView* browser_view,
    KeylogBrowserFileSelectedCallback callback,
    void* context);
