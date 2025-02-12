#pragma once
#include <gui/view.h>

typedef struct SchedulerUIRunState SchedulerUIRunState;
typedef struct SchedulerRunView SchedulerRunView;

//void scheduler_update_progress(uint8_t x);

SchedulerRunView* scheduler_run_view_alloc();
void scheduler_run_view_free(SchedulerRunView* run_view);
View* scheduler_run_view_get_view(SchedulerRunView* run_view);
