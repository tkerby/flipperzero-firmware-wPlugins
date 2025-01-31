#pragma once

#include <flipper_format_i.h>
#include "scheduler_app_i.h"

void scheduler_tx(SchedulerApp* app, const char* file_path);
void scheduler_tx_playlist(SchedulerApp* app, const char* file_path);
