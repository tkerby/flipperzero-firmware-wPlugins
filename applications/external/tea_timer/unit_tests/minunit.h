#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "minunit_vars.h"

/*
 * Минимальный вендорный minunit для прогона на устройстве.
 * Вывод отчёта — только через snprintf + pipe_send: printf/puts/vsnprintf
 * не экспортируются в FAP.
 */

#define MINUNIT_MESSAGE_LEN 256

static inline void minunit_send(const char* text) {
    if(minunit_pipe != NULL && text != NULL) {
        pipe_send(minunit_pipe, text, strlen(text));
    }
}

static inline void minunit_print_fail(const char* message, const char* file, int line) {
    char buffer[MINUNIT_MESSAGE_LEN];
    snprintf(
        buffer,
        sizeof(buffer),
        "FAIL: %s (%s:%d)\r\n",
        (message != NULL) ? message : "assertion failed",
        (file != NULL) ? file : "?",
        line);
    minunit_fail++;
    minunit_status = 1;
    minunit_send(buffer);
}

static inline void minunit_report(void) {
    char buffer[MINUNIT_MESSAGE_LEN];
    snprintf(
        buffer,
        sizeof(buffer),
        "%u tests, %u assertions, %u failures\r\n",
        minunit_run,
        minunit_assert,
        minunit_fail);
    minunit_send(buffer);
}

#define MU_TEST(_me) static void _me(void)

#define MU_RUN_TEST(_test)                                      \
    do {                                                        \
        char mu_name[64];                                       \
        snprintf(mu_name, sizeof(mu_name), "%s()\r\n", #_test); \
        minunit_send(mu_name);                                  \
        minunit_run++;                                          \
        _test();                                                \
    } while(0)

#define MU_RUN_SUITE(_suite) \
    do {                     \
        _suite();            \
    } while(0)

#define MU_REPORT() minunit_report()

#define mu_fail(_message)                                 \
    do {                                                  \
        minunit_print_fail(_message, __FILE__, __LINE__); \
        return;                                           \
    } while(0)

#define mu_check(_test)                                                      \
    do {                                                                     \
        minunit_assert++;                                                    \
        if(!(_test)) {                                                       \
            minunit_print_fail("Check failed: " #_test, __FILE__, __LINE__); \
            return;                                                          \
        }                                                                    \
    } while(0)

#define mu_assert(_message, _test)                            \
    do {                                                      \
        minunit_assert++;                                     \
        if(!(_test)) {                                        \
            minunit_print_fail(_message, __FILE__, __LINE__); \
            return;                                           \
        }                                                     \
    } while(0)

#define mu_assert_int_eq(_expected, _actual)                                                    \
    do {                                                                                        \
        minunit_assert++;                                                                       \
        long mu_expected = (long)(_expected);                                                   \
        long mu_actual = (long)(_actual);                                                       \
        if(mu_expected != mu_actual) {                                                          \
            char mu_buffer[MINUNIT_MESSAGE_LEN];                                                \
            snprintf(                                                                           \
                mu_buffer, sizeof(mu_buffer), "Expected %ld, got %ld", mu_expected, mu_actual); \
            minunit_print_fail(mu_buffer, __FILE__, __LINE__);                                  \
            return;                                                                             \
        }                                                                                       \
    } while(0)

#define mu_assert_string_eq(_expected, _actual)                                               \
    do {                                                                                      \
        minunit_assert++;                                                                     \
        const char* mu_expected = (_expected);                                                \
        const char* mu_actual = (_actual);                                                    \
        if(mu_expected == NULL || mu_actual == NULL || strcmp(mu_expected, mu_actual) != 0) { \
            char mu_buffer[MINUNIT_MESSAGE_LEN];                                              \
            snprintf(                                                                         \
                mu_buffer,                                                                    \
                sizeof(mu_buffer),                                                            \
                "Expected \"%s\", got \"%s\"",                                                \
                (mu_expected != NULL) ? mu_expected : "(null)",                               \
                (mu_actual != NULL) ? mu_actual : "(null)");                                  \
            minunit_print_fail(mu_buffer, __FILE__, __LINE__);                                \
            return;                                                                           \
        }                                                                                     \
    } while(0)

#define mu_assert_mem_eq(_expected, _actual, _size)                         \
    do {                                                                    \
        minunit_assert++;                                                   \
        if(memcmp((_expected), (_actual), (_size)) != 0) {                  \
            minunit_print_fail("Memory blocks differ", __FILE__, __LINE__); \
            return;                                                         \
        }                                                                   \
    } while(0)
