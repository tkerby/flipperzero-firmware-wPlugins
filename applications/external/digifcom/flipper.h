#ifndef FLIPPER_HEADERS
#define FLIPPER_HEADERS

#include <furi.h>

#include <gui/gui.h>
#include <gui/icon_i.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/text_box.h>

#include <furi_hal_cortex.h>
#include <flipper_format/flipper_format_i.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

/* generated by fbt from .png files in images folder */
#include <fcom_icons.h>

#define APP_DIRECTORY_PATH EXT_PATH("apps_data/fcom")

#define TAG "FCOM"

#endif