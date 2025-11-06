/* Unity Testing Framework Header - Lightweight C Unit Testing */
#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

// Unity configuration
#define UNITY_INT_WIDTH     32
#define UNITY_POINTER_WIDTH 32
#define UNITY_OUTPUT_COLOR

// Test result counters
typedef struct {
    unsigned int tests;
    unsigned int failures;
    unsigned int ignored;
} Unity_t;

extern Unity_t Unity;

// Assertion macros
#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    _unity_assert_equal_int((int)(expected), (int)(actual), __LINE__, __FILE__)

#define TEST_ASSERT_EQUAL_UINT(expected, actual) \
    _unity_assert_equal_uint((unsigned int)(expected), (unsigned int)(actual), __LINE__, __FILE__)

#define TEST_ASSERT_EQUAL_HEX32(expected, actual) \
    _unity_assert_equal_hex32((uint32_t)(expected), (uint32_t)(actual), __LINE__, __FILE__)

#define TEST_ASSERT_TRUE(condition) _unity_assert_true((condition), __LINE__, __FILE__)

#define TEST_ASSERT_FALSE(condition) _unity_assert_false((condition), __LINE__, __FILE__)

#define TEST_ASSERT_NULL(pointer) _unity_assert_null((pointer), __LINE__, __FILE__)

#define TEST_ASSERT_NOT_NULL(pointer) _unity_assert_not_null((pointer), __LINE__, __FILE__)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    _unity_assert_equal_string((expected), (actual), __LINE__, __FILE__)

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) \
    _unity_assert_equal_memory((expected), (actual), (len), __LINE__, __FILE__)

#define TEST_FAIL(message) _unity_test_fail((message), __LINE__, __FILE__)

// Test framework functions
void unity_begin(void);
int unity_end(void);
void unity_test_start(const char* name);
void unity_test_finish(void);

// Internal assertion functions
void _unity_assert_equal_int(int expected, int actual, int line, const char* file);
void _unity_assert_equal_uint(
    unsigned int expected,
    unsigned int actual,
    int line,
    const char* file);
void _unity_assert_equal_hex32(uint32_t expected, uint32_t actual, int line, const char* file);
void _unity_assert_true(bool condition, int line, const char* file);
void _unity_assert_false(bool condition, int line, const char* file);
void _unity_assert_null(const void* pointer, int line, const char* file);
void _unity_assert_not_null(const void* pointer, int line, const char* file);
void _unity_assert_equal_string(
    const char* expected,
    const char* actual,
    int line,
    const char* file);
void _unity_assert_equal_memory(
    const void* expected,
    const void* actual,
    size_t len,
    int line,
    const char* file);
void _unity_test_fail(const char* message, int line, const char* file);

// Test runner helpers
#define RUN_TEST(test_func)           \
    do {                              \
        unity_test_start(#test_func); \
        test_func();                  \
        unity_test_finish();          \
    } while(0)

// Mock support
#define MOCK_PTR(type)   ((type*)0x1000)
#define MOCK_INT(value)  ((int)(value))
#define MOCK_UINT(value) ((unsigned int)(value))

#endif /* UNITY_FRAMEWORK_H */
