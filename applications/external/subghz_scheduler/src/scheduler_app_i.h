#pragma once

#include "scheduler_app.h"
#include "scenes/scheduler_scene.h"
#include "scheduler_custom_event.h"
#include "subghz_scheduler.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/dialog_ex.h>
#include <expansion/expansion.h>
#include <dialogs/dialogs.h>

#include "scenes/scheduler_scene_settings.h"

struct SchedulerApp {
    Expansion* expansion;
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Widget* widget;
    DialogsApp* dialogs;
    VariableItemList* var_item_list;
    VariableItem* var_item_flow;
    FuriThread* thread;
    Scheduler* scheduler;
    FuriString* file_path;
};

typedef enum {
    SchedulerAppViewVarItemList,
    SchedulerAppViewRunSchedule,
    SchedulerAppViewExitConfirm,
} SchedulerAppView;
