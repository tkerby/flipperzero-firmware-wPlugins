#pragma once

#include <toolbox/pipe.h>

/** Глобальные переменные прогона тестов (определены в test_index.c). */
extern PipeSide* minunit_pipe;
extern unsigned int minunit_run;
extern unsigned int minunit_assert;
extern unsigned int minunit_fail;
extern int minunit_status;
extern char minunit_last_message[256];
