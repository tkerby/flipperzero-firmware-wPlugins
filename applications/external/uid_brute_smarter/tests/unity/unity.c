/* Unity Testing Framework Implementation - Lightweight C Unit Testing */
#include "unity.h"
#include <stdarg.h>

Unity_t Unity = {0, 0, 0};
static const char* current_test_name = NULL;
static unsigned int test_start_failures = 0;

void unity_begin(void) {
    Unity.tests = 0;
    Unity.failures = 0;
    Unity.ignored = 0;
    printf("\n");
    printf("🧪 Unity Test Framework Starting\n");
    printf("================================\n\n");
}

int unity_end(void) {
    printf("\n");
    printf("================================\n");
    printf(
        "🎯 Test Results: %d tests, %d passed, %d failed\n",
        Unity.tests,
        Unity.tests - Unity.failures,
        Unity.failures);

    if(Unity.failures == 0) {
        printf("✅ ALL TESTS PASSED!\n");
    } else {
        printf("❌ SOME TESTS FAILED!\n");
    }
    printf("================================\n\n");

    return Unity.failures;
}

void unity_test_start(const char* name) {
    current_test_name = name;
    Unity.tests++;
    test_start_failures = Unity.failures;
    printf("📝 Running: %s\n", name);
}

void unity_test_finish(void) {
    if(Unity.failures == test_start_failures || current_test_name == NULL) {
        printf("   ✅ PASSED\n\n");
    } else {
        printf("   ❌ FAILED\n\n");
    }
    current_test_name = NULL;
}

void _unity_assert_equal_int(int expected, int actual, int line, const char* file) {
    if(expected != actual) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: %d (0x%08x)\n", expected, expected);
        printf("      Actual:   %d (0x%08x)\n", actual, actual);
        Unity.failures++;
    }
}

void _unity_assert_equal_uint(
    unsigned int expected,
    unsigned int actual,
    int line,
    const char* file) {
    if(expected != actual) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: %u (0x%08x)\n", expected, expected);
        printf("      Actual:   %u (0x%08x)\n", actual, actual);
        Unity.failures++;
    }
}

void _unity_assert_equal_hex32(uint32_t expected, uint32_t actual, int line, const char* file) {
    if(expected != actual) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: 0x%08lx\n", (unsigned long)expected);
        printf("      Actual:   0x%08lx\n", (unsigned long)actual);
        Unity.failures++;
    }
}

void _unity_assert_true(bool condition, int line, const char* file) {
    if(!condition) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: TRUE\n");
        printf("      Actual:   FALSE\n");
        Unity.failures++;
    }
}

void _unity_assert_false(bool condition, int line, const char* file) {
    if(condition) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: FALSE\n");
        printf("      Actual:   TRUE\n");
        Unity.failures++;
    }
}

void _unity_assert_null(const void* pointer, int line, const char* file) {
    if(pointer != NULL) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: NULL\n");
        printf("      Actual:   %p\n", pointer);
        Unity.failures++;
    }
}

void _unity_assert_not_null(const void* pointer, int line, const char* file) {
    if(pointer == NULL) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: Not NULL\n");
        printf("      Actual:   NULL\n");
        Unity.failures++;
    }
}

void _unity_assert_equal_string(
    const char* expected,
    const char* actual,
    int line,
    const char* file) {
    if((expected == NULL && actual != NULL) || (expected != NULL && actual == NULL) ||
       (expected != NULL && actual != NULL && strcmp(expected, actual) != 0)) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Expected: \"%s\"\n", expected ? expected : "NULL");
        printf("      Actual:   \"%s\"\n", actual ? actual : "NULL");
        Unity.failures++;
    }
}

void _unity_assert_equal_memory(
    const void* expected,
    const void* actual,
    size_t len,
    int line,
    const char* file) {
    if(expected == NULL || actual == NULL) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      NULL pointer in memory comparison\n");
        Unity.failures++;
        return;
    }

    if(memcmp(expected, actual, len) != 0) {
        printf("   ❌ ASSERT FAILED at %s:%d\n", file, line);
        printf("      Memory contents differ\n");
        Unity.failures++;
    }
}

void _unity_test_fail(const char* message, int line, const char* file) {
    printf("   ❌ TEST FAILED at %s:%d\n", file, line);
    printf("      Message: %s\n", message ? message : "No message provided");
    Unity.failures++;
}
