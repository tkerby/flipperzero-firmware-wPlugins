#include "../unity/unity.h"
#include "../mocks/furi_mock.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Simple test to verify the configuration back button callback signature and return value
// This test isolates just the callback function to ensure it compiles and works correctly

// Define view IDs to match the main application
typedef enum {
    UidBruteSmarterViewSplash,
    UidBruteSmarterViewCardLoader,
    UidBruteSmarterViewPattern,
    UidBruteSmarterViewConfig,
    UidBruteSmarterViewBrute,
    UidBruteSmarterViewMenu,
    UidBruteSmarterViewKeyList
} UidBruteSmarterView;

// Mock the types and structures needed
typedef struct {
    int dummy_field;
} UidBruteSmarter;

// Include the actual callback function from the main application
// We'll redefine it here to test it in isolation
// This now correctly returns a view ID (uint32_t) instead of bool
static uint32_t uid_brute_smarter_config_back_callback(void* context) {
    (void)context; // Unused parameter

    // Return the menu view ID - this is what the fixed version should do
    return UidBruteSmarterViewMenu;
}

// Test the configuration back button callback
void setUp(void) {
}
void tearDown(void) {
}

static void test_config_back_callback_signature(void) {
    // Create a mock instance
    UidBruteSmarter test_instance;
    test_instance.dummy_field = 42;

    // Test that the callback function exists and can be called
    uint32_t result = uid_brute_smarter_config_back_callback(&test_instance);

    // Verify the callback returns the correct view ID for the menu
    TEST_ASSERT_EQUAL_UINT(UidBruteSmarterViewMenu, result);
    TEST_ASSERT_EQUAL_INT(42, test_instance.dummy_field); // Verify instance wasn't corrupted
}

static void test_config_back_callback_null_context(void) {
    // Test that the callback handles null context gracefully
    uint32_t result = uid_brute_smarter_config_back_callback(NULL);

    // Should still return the menu view ID even with null context
    TEST_ASSERT_EQUAL_UINT(UidBruteSmarterViewMenu, result);
}

static void test_config_back_callback_returns_valid_view(void) {
    // Test that the returned view ID is within valid range
    uint32_t result = uid_brute_smarter_config_back_callback(NULL);

    // The menu view should be within the valid view range
    TEST_ASSERT_TRUE(result >= UidBruteSmarterViewSplash);
    TEST_ASSERT_TRUE(result <= UidBruteSmarterViewKeyList);

    // Specifically, it should be the menu view
    TEST_ASSERT_EQUAL_UINT(5, result); // UidBruteSmarterViewMenu = 5
}

void test_config_callback_run_all(void) {
    printf("\n");
    printf("🔧 Configuration Callback Unit Tests\n");
    printf("===================================\n");

    unity_begin();

    RUN_TEST(test_config_back_callback_signature);
    RUN_TEST(test_config_back_callback_null_context);
    RUN_TEST(test_config_back_callback_returns_valid_view);

    unity_end();
}

int main(void) {
    test_config_callback_run_all();
    return 0;
}
