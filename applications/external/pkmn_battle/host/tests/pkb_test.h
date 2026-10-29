/*
 * Minimal unit-test helpers for the host build.
 *
 *   static void test_foo(void) { CHECK_EQ(1 + 1, 2); }
 *   int main(void) { RUN(test_foo); return TEST_REPORT(); }
 */
#pragma once

#include <stdio.h>
#include <string.h>

static int pkb_test_failures;
static int pkb_test_checks;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        pkb_test_checks++;                                                           \
        if(!(cond)) {                                                                \
            pkb_test_failures++;                                                     \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                            \
    } while(0)

#define CHECK_EQ(actual, expected)                          \
    do {                                                    \
        long long a_ = (long long)(actual);                 \
        long long e_ = (long long)(expected);               \
        pkb_test_checks++;                                  \
        if(a_ != e_) {                                      \
            pkb_test_failures++;                            \
            fprintf(                                        \
                stderr,                                     \
                "%s:%d: %s == %lld, expected %s == %lld\n", \
                __FILE__,                                   \
                __LINE__,                                   \
                #actual,                                    \
                a_,                                         \
                #expected,                                  \
                e_);                                        \
        }                                                   \
    } while(0)

#define CHECK_MEM_EQ(actual, expected, len)                       \
    do {                                                          \
        pkb_test_checks++;                                        \
        if(memcmp((actual), (expected), (len)) != 0) {            \
            pkb_test_failures++;                                  \
            fprintf(                                              \
                stderr,                                           \
                "%s:%d: memory mismatch: %s vs %s (%zu bytes)\n", \
                __FILE__,                                         \
                __LINE__,                                         \
                #actual,                                          \
                #expected,                                        \
                (size_t)(len));                                   \
        }                                                         \
    } while(0)

#define RUN(test_fn)                                                                 \
    do {                                                                             \
        int before_ = pkb_test_failures;                                             \
        test_fn();                                                                   \
        printf("%s %s\n", pkb_test_failures == before_ ? "PASS" : "FAIL", #test_fn); \
    } while(0)

#define TEST_REPORT()                                                        \
    (printf("%d checks, %d failures\n", pkb_test_checks, pkb_test_failures), \
     pkb_test_failures ? 1 : 0)
