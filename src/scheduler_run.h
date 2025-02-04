#pragma once

#include <flipper_format_i.h>
#include "scheduler_app_i.h"

typedef struct ScheduleTxRun ScheduleTxRun;

//FuriThreadCallback scheduler_tx(SchedulerApp* context);
void scheduler_start_tx(SchedulerApp* context);
