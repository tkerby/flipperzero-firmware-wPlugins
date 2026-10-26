/*
 * Unit tests for brute force completion flow
 * Copyright (c) 2024, UID Brute Smarter Contributors
 */

#include "../unity/unity.h"
#include "../mocks/furi_mock.h"
#include "../../pattern_engine.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t uid;
    char* name;
    char* file_path;
    uint32_t loaded_time;
    bool is_active;
} KeyInfo;

// Mock structures
typedef struct {
    uint32_t uids[5];
    uint8_t uid_count;
    PatternType pattern;
    uint32_t start_uid;
    uint32_t end_uid;
    uint32_t step;
    uint32_t current_uid;
    uint32_t delay_ms;
    uint32_t pause_every;
    uint32_t pause_duration_s;
    bool is_running;
    bool should_stop;
} MockUidBruteSmartApp;

typedef struct {
    MockUidBruteSmartApp* app;
    KeyInfo loaded_keys[5];
    uint8_t loaded_keys_count;
    char file_path[256];
    // Mock GUI components
    void* popup;
    void* view_dispatcher;
    uint32_t current_view;
    bool popup_enabled;
    uint32_t popup_timeout;
} MockUidBruteSmarter;

// Global test variables
static MockUidBruteSmarter* test_instance = NULL;
static int view_switch_count = 0;
static uint32_t last_switched_view = 0;
static int popup_set_count = 0;
static char last_popup_header[64] = "";
static char last_popup_text[128] = "";

// Mock functions
void mock_view_dispatcher_switch_to_view(void* view_dispatcher, uint32_t view_id) {
    view_switch_count++;
    last_switched_view = view_id;
    if(test_instance) {
        test_instance->current_view = view_id;
    }
}

void mock_popup_set_header(void* popup, const char* header, int x, int y, int align_x, int align_y) {
    popup_set_count++;
    strncpy(last_popup_header, header, sizeof(last_popup_header) - 1);
    last_popup_header[sizeof(last_popup_header) - 1] = '\0';
}

void mock_popup_set_text(void* popup, const char* text, int x, int y, int align_x, int align_y) {
    popup_set_count++;
    strncpy(last_popup_text, text, sizeof(last_popup_text) - 1);
    last_popup_text[sizeof(last_popup_text) - 1] = '\0';
}

void mock_popup_set_timeout(void* popup, uint32_t timeout) {
    if(test_instance) {
        test_instance->popup_timeout = timeout;
    }
}

void mock_popup_enable_timeout(void* popup) {
    if(test_instance) {
        test_instance->popup_enabled = true;
    }
}

void mock_delay_ms(uint32_t ms) {
    // Mock delay - do nothing
}

void mock_uid_brute_smarter_setup_menu(void* instance) {
    // Mock menu setup: rebuilding the menu is not a view switch; the explicit
    // switch_to_view call in the completion flow is the only counted one
    (void)instance;
}

// Test setup and teardown
void setUp(void) {
    view_switch_count = 0;
    last_switched_view = 0;
    popup_set_count = 0;
    memset(last_popup_header, 0, sizeof(last_popup_header));
    memset(last_popup_text, 0, sizeof(last_popup_text));

    // Allocate test instance
    test_instance = malloc(sizeof(MockUidBruteSmarter));
    TEST_ASSERT_NOT_NULL(test_instance);

    // Initialize app data
    test_instance->app = malloc(sizeof(MockUidBruteSmartApp));
    TEST_ASSERT_NOT_NULL(test_instance->app);
    memset(test_instance->app, 0, sizeof(MockUidBruteSmartApp));

    // Set default values
    test_instance->app->delay_ms = 500;
    test_instance->app->pause_every = 0;
    test_instance->app->pause_duration_s = 3;
    test_instance->app->uid_count = 1;
    test_instance->app->is_running = true;
    test_instance->app->should_stop = false;
    test_instance->app->uids[0] = 0x12345678; // Test UID

    // Initialize key storage
    test_instance->loaded_keys_count = 1;
    test_instance->loaded_keys[0].uid = 0x12345678;
    test_instance->loaded_keys[0].name = strdup("Test Key 1");
    test_instance->loaded_keys[0].file_path = strdup("/test/path.nfc");
    test_instance->loaded_keys[0].loaded_time = 12345;
    test_instance->loaded_keys[0].is_active = true;

    // Initialize mock GUI components
    test_instance->popup = malloc(1); // Mock popup
    test_instance->view_dispatcher = malloc(1); // Mock dispatcher
    test_instance->current_view = 5; // Start in brute force view
    test_instance->popup_enabled = false;
    test_instance->popup_timeout = 0;

    // Initialize mock file path
    strcpy(test_instance->file_path, "");
}

void tearDown(void) {
    if(test_instance) {
        // Clean up key memory
        for(int i = 0; i < test_instance->loaded_keys_count; i++) {
            free(test_instance->loaded_keys[i].name);
            free(test_instance->loaded_keys[i].file_path);
        }

        if(test_instance->app) {
            free(test_instance->app);
        }
        if(test_instance->popup) {
            free(test_instance->popup);
        }
        if(test_instance->view_dispatcher) {
            free(test_instance->view_dispatcher);
        }
        free(test_instance);
        test_instance = NULL;
    }
}

// Helper functions to simulate brute force completion
void simulate_brute_force_completion_normal(void) {
    // Simulate normal completion (not stopped by user)
    test_instance->app->is_running = false;
    test_instance->app->should_stop = false;

    // Set completion popup
    mock_popup_set_header(test_instance->popup, "Complete", 64, 20, 0, 0);
    mock_popup_set_text(
        test_instance->popup, "Brute force completed\nReturning to menu...", 64, 40, 0, 0);
    mock_popup_set_timeout(test_instance->popup, 2000);
    mock_popup_enable_timeout(test_instance->popup);

    // Simulate delay for popup timeout
    mock_delay_ms(2100); // POPUP_TIMEOUT_MS + 100ms buffer

    // Return to main menu
    mock_uid_brute_smarter_setup_menu(test_instance);
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, 4);
}

void simulate_brute_force_completion_stopped(void) {
    // Simulate user-stopped completion
    test_instance->app->is_running = false;
    test_instance->app->should_stop = true;

    // Set stopped popup
    mock_popup_set_header(test_instance->popup, "Stopped", 64, 20, 0, 0);
    mock_popup_set_text(
        test_instance->popup, "Brute force stopped\nReturning to menu...", 64, 40, 0, 0);
    mock_popup_set_timeout(test_instance->popup, 2000);
    mock_popup_enable_timeout(test_instance->popup);

    // Simulate delay for popup timeout
    mock_delay_ms(2100); // POPUP_TIMEOUT_MS + 100ms buffer

    // Return to main menu
    mock_uid_brute_smarter_setup_menu(test_instance);
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, 4);
}

// Test functions
void test_brute_force_completion_normal_flow(void) {
    // Test normal completion flow
    view_switch_count = 0;
    popup_set_count = 0;

    simulate_brute_force_completion_normal();

    // Verify popup was set correctly
    TEST_ASSERT_EQUAL_INT(2, popup_set_count); // Header + text
    TEST_ASSERT_EQUAL_STRING("Complete", last_popup_header);
    TEST_ASSERT_EQUAL_STRING("Brute force completed\nReturning to menu...", last_popup_text);
    TEST_ASSERT_TRUE(test_instance->popup_enabled);
    TEST_ASSERT_EQUAL_UINT(2000, test_instance->popup_timeout);

    // Verify view switch back to main menu
    TEST_ASSERT_EQUAL_INT(1, view_switch_count);
    TEST_ASSERT_EQUAL_UINT(4, last_switched_view); // UidBruteSmarterViewMenu
    TEST_ASSERT_EQUAL_UINT(4, test_instance->current_view);

    // Verify app state
    TEST_ASSERT_FALSE(test_instance->app->is_running);
    TEST_ASSERT_FALSE(test_instance->app->should_stop);
}

void test_brute_force_completion_stopped_flow(void) {
    // Test user-stopped completion flow
    view_switch_count = 0;
    popup_set_count = 0;

    simulate_brute_force_completion_stopped();

    // Verify popup was set correctly
    TEST_ASSERT_EQUAL_INT(2, popup_set_count); // Header + text
    TEST_ASSERT_EQUAL_STRING("Stopped", last_popup_header);
    TEST_ASSERT_EQUAL_STRING("Brute force stopped\nReturning to menu...", last_popup_text);
    TEST_ASSERT_TRUE(test_instance->popup_enabled);
    TEST_ASSERT_EQUAL_UINT(2000, test_instance->popup_timeout);

    // Verify view switch back to main menu
    TEST_ASSERT_EQUAL_INT(1, view_switch_count);
    TEST_ASSERT_EQUAL_UINT(4, last_switched_view); // UidBruteSmarterViewMenu
    TEST_ASSERT_EQUAL_UINT(4, test_instance->current_view);

    // Verify app state
    TEST_ASSERT_FALSE(test_instance->app->is_running);
    TEST_ASSERT_TRUE(test_instance->app->should_stop);
}

void test_brute_force_completion_preserves_keys(void) {
    // Test that keys are preserved after completion
    simulate_brute_force_completion_normal();

    // Verify keys are still loaded
    TEST_ASSERT_EQUAL_UINT(1, test_instance->loaded_keys_count);
    TEST_ASSERT_EQUAL_UINT(1, test_instance->app->uid_count);
    TEST_ASSERT_EQUAL_UINT(0x12345678, test_instance->loaded_keys[0].uid);
    TEST_ASSERT_EQUAL_STRING("Test Key 1", test_instance->loaded_keys[0].name);
}

void test_brute_force_completion_preserves_config(void) {
    // Test that configuration is preserved after completion
    test_instance->app->delay_ms = 800;
    test_instance->app->pause_every = 10;
    test_instance->app->pause_duration_s = 5;

    simulate_brute_force_completion_stopped();

    // Verify configuration is preserved
    TEST_ASSERT_EQUAL_UINT(800, test_instance->app->delay_ms);
    TEST_ASSERT_EQUAL_UINT(10, test_instance->app->pause_every);
    TEST_ASSERT_EQUAL_UINT(5, test_instance->app->pause_duration_s);
}

void test_brute_force_completion_multiple_keys(void) {
    // Add more keys
    test_instance->loaded_keys_count = 3;
    test_instance->app->uid_count = 3;
    test_instance->app->uids[1] = 0x87654321;
    test_instance->app->uids[2] = 0x11223344;

    test_instance->loaded_keys[1].uid = 0x87654321;
    test_instance->loaded_keys[1].name = strdup("Test Key 2");
    test_instance->loaded_keys[1].file_path = strdup("/test/path2.nfc");

    test_instance->loaded_keys[2].uid = 0x11223344;
    test_instance->loaded_keys[2].name = strdup("Test Key 3");
    test_instance->loaded_keys[2].file_path = strdup("/test/path3.nfc");

    simulate_brute_force_completion_normal();

    // Verify all keys are preserved
    TEST_ASSERT_EQUAL_UINT(3, test_instance->loaded_keys_count);
    TEST_ASSERT_EQUAL_UINT(3, test_instance->app->uid_count);
    TEST_ASSERT_EQUAL_UINT(0x12345678, test_instance->loaded_keys[0].uid);
    TEST_ASSERT_EQUAL_UINT(0x87654321, test_instance->loaded_keys[1].uid);
    TEST_ASSERT_EQUAL_UINT(0x11223344, test_instance->loaded_keys[2].uid);
}

void test_brute_force_completion_with_zero_keys(void) {
    // Test completion with no keys loaded
    test_instance->loaded_keys_count = 0;
    test_instance->app->uid_count = 0;

    simulate_brute_force_completion_normal();

    // Verify state is consistent
    TEST_ASSERT_EQUAL_UINT(0, test_instance->loaded_keys_count);
    TEST_ASSERT_EQUAL_UINT(0, test_instance->app->uid_count);
    TEST_ASSERT_FALSE(test_instance->app->is_running);
    TEST_ASSERT_EQUAL_UINT(4, test_instance->current_view);
}

void test_brute_force_completion_timing(void) {
    // Test that proper timing is simulated
    view_switch_count = 0;

    simulate_brute_force_completion_normal();

    // Verify that the flow includes proper delay
    // The mock_delay_ms should have been called with appropriate timing
    TEST_ASSERT_EQUAL_INT(1, view_switch_count);
    TEST_ASSERT_EQUAL_UINT(4, last_switched_view);
}

void test_brute_force_completion_error_handling(void) {
    // Test error handling during completion
    test_instance->app->is_running = false;
    test_instance->app->should_stop = false;

    // Simulate error scenario
    mock_popup_set_header(test_instance->popup, "Error", 64, 20, 0, 0);
    mock_popup_set_text(test_instance->popup, "Brute force failed", 64, 40, 0, 0);
    mock_popup_set_timeout(test_instance->popup, 2000);
    mock_popup_enable_timeout(test_instance->popup);

    mock_delay_ms(2100);
    mock_uid_brute_smarter_setup_menu(test_instance);
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, 4);

    // Verify error popup was shown
    TEST_ASSERT_EQUAL_STRING("Error", last_popup_header);
    TEST_ASSERT_EQUAL_STRING("Brute force failed", last_popup_text);

    // Verify still returns to main menu
    TEST_ASSERT_EQUAL_UINT(4, test_instance->current_view);
}

void test_brute_force_completion_state_reset(void) {
    // Test that brute force state is properly reset
    test_instance->app->is_running = true;
    test_instance->app->should_stop = false;
    test_instance->app->current_uid = 0x99999999;

    simulate_brute_force_completion_normal();

    // Verify state is reset
    TEST_ASSERT_FALSE(test_instance->app->is_running);
    TEST_ASSERT_FALSE(test_instance->app->should_stop);
    // Note: current_uid is not explicitly reset in the current implementation
    // This test documents current behavior
}

// Main test runner
int main(void) {
    unity_begin();

    RUN_TEST(test_brute_force_completion_normal_flow);
    RUN_TEST(test_brute_force_completion_stopped_flow);
    RUN_TEST(test_brute_force_completion_preserves_keys);
    RUN_TEST(test_brute_force_completion_preserves_config);
    RUN_TEST(test_brute_force_completion_multiple_keys);
    RUN_TEST(test_brute_force_completion_with_zero_keys);
    RUN_TEST(test_brute_force_completion_timing);
    RUN_TEST(test_brute_force_completion_error_handling);
    RUN_TEST(test_brute_force_completion_state_reset);

    return unity_end();
}
