#include "test_index.h"

#include <furi.h>
#include <cli/cli.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "minunit.h"

#define TAG "TeaTimerTest"

/** Длина отчёта в строке CLI. */
#define TEST_LINE_LEN 96

/** Реестр наборов тестов. */
typedef struct {
    const char* name;
    int (*entry)(void);
} UnitTest;

extern int run_minunit_test_tea_preset(void);
extern int run_minunit_test_brew_math(void);
extern int run_minunit_test_preset_write(void);

static const UnitTest unit_tests[] = {
    {"tea_preset", run_minunit_test_tea_preset},
    {"brew_math", run_minunit_test_brew_math},
    {"preset_write", run_minunit_test_preset_write},
};

/** Определения глобальных переменных прогона. */
PipeSide* minunit_pipe = NULL;
unsigned int minunit_run = 0;
unsigned int minunit_assert = 0;
unsigned int minunit_fail = 0;
int minunit_status = 0;
char minunit_last_message[256];

static CliRegistry* cli_registry = NULL;

/** Выполнить наборы (или один набор по имени) и напечатать отчёт. */
static void tea_timer_test_cli_callback(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    minunit_pipe = pipe;

    char filter[32];
    filter[0] = '\0';
    if(args != NULL && !furi_string_empty(args)) {
        snprintf(filter, sizeof(filter), "%s", furi_string_get_cstr(args));
        size_t len = strlen(filter);
        while(len > 0 &&
              (filter[len - 1] == ' ' || filter[len - 1] == '\r' || filter[len - 1] == '\n')) {
            filter[--len] = '\0';
        }
        size_t skip = 0;
        while(filter[skip] == ' ') {
            skip++;
        }
        if(skip > 0) {
            memmove(filter, filter + skip, strlen(filter + skip) + 1);
        }
    }

    minunit_run = 0;
    minunit_assert = 0;
    minunit_fail = 0;
    minunit_status = 0;

    uint32_t start_tick = furi_get_tick();
    size_t heap_before = memmgr_get_free_heap();

    size_t suite_count = sizeof(unit_tests) / sizeof(unit_tests[0]);
    for(size_t i = 0; i < suite_count; i++) {
        if(filter[0] != '\0' && strcmp(filter, unit_tests[i].name) != 0) {
            continue;
        }
        minunit_status |= unit_tests[i].entry();
    }

    uint32_t stop_tick = furi_get_tick();
    size_t heap_after = memmgr_get_free_heap();

    char line[TEST_LINE_LEN];
    MU_REPORT();

    snprintf(
        line, sizeof(line), "Time: %lu ms\r\n", (unsigned long)(uint32_t)(stop_tick - start_tick));
    minunit_send(line);

    int32_t leaked = (int32_t)heap_before - (int32_t)heap_after;
    snprintf(line, sizeof(line), "Leaked: %ld\r\n", (long)leaked);
    minunit_send(line);

    minunit_send((minunit_status == 0) ? "Status: PASSED\r\n" : "Status: FAILED\r\n");
    FURI_LOG_I(
        TAG,
        "run=%u assert=%u fail=%u status=%d",
        minunit_run,
        minunit_assert,
        minunit_fail,
        minunit_status);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(
        notification, (minunit_status == 0) ? &sequence_success : &sequence_error);
    furi_record_close(RECORD_NOTIFICATION);
}

void tea_timer_tests_register(void) {
    cli_registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command_ex(
        cli_registry,
        "tea_timer_test",
        CliCommandFlagParallelSafe,
        tea_timer_test_cli_callback,
        NULL,
        2048);
}

void tea_timer_tests_unregister(void) {
    if(cli_registry != NULL) {
        cli_registry_delete_command(cli_registry, "tea_timer_test");
        furi_record_close(RECORD_CLI);
        cli_registry = NULL;
    }
}
