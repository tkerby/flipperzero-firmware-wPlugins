/*
 * Integration tests for UI state management
 * Copyright (c) 2024, UID Brute Smarter Contributors
 */

#include "../unity/unity.h"
#include "../mocks/furi_mock.h"
#include "../../pattern_engine.h"
#include <stdlib.h>
#include <string.h>

// Mock UI state enumeration
typedef enum {
    MockViewSplash = 1,
    MockViewMenu = 4,
    MockViewConfig = 3,
    MockViewBrute = 5,
    MockViewKeyList = 6,
    MockViewCardLoader = 2
} MockViewType;

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
    void* gui;
    void* view_dispatcher;
    void* submenu;
    void* variable_item_list;
    void* popup;
    void* dialog_ex;

    // UI state tracking
    MockViewType current_view;
    MockViewType previous_view;
    bool view_initialized[7]; // Track which views are initialized

    // State flags
    bool is_running;
    bool should_stop;
    bool popup_enabled;
    uint32_t popup_timeout;
} MockUidBruteSmarter;

// Global test variables
static MockUidBruteSmarter* test_instance = NULL;
static int view_switch_count = 0;
static MockViewType last_switched_view = MockViewSplash;
static int state_change_count = 0;

// Mock functions
void mock_view_dispatcher_switch_to_view(void* dispatcher, MockViewType view_id) {
    view_switch_count++;
    last_switched_view = view_id;

    if(test_instance) {
        test_instance->previous_view = test_instance->current_view;
        test_instance->current_view = view_id;
        state_change_count++;
        // Returning to the menu is the recovery point: any pending stop
        // request has been consumed by then (mirrors the real app's cleanup)
        if(view_id == MockViewMenu) {
            test_instance->should_stop = false;
        }
    }
}

void* mock_view_dispatcher_alloc(void) {
    return malloc(1); // Mock dispatcher
}

void mock_view_dispatcher_free(void* dispatcher) {
    free(dispatcher);
}

void* mock_submenu_alloc(void) {
    return malloc(1);
}

void mock_submenu_free(void* submenu) {
    free(submenu);
}

void* mock_variable_item_list_alloc(void) {
    return malloc(1);
}

void mock_variable_item_list_free(void* list) {
    free(list);
}

void* mock_popup_alloc(void) {
    return malloc(1);
}

void mock_popup_free(void* popup) {
    free(popup);
}

void* mock_dialog_ex_alloc(void) {
    return malloc(1);
}

void mock_dialog_ex_free(void* dialog) {
    free(dialog);
}

void mock_view_dispatcher_add_view(void* dispatcher, MockViewType view_id, void* view) {
    if(test_instance && view_id > 0 && view_id <= 6) {
        test_instance->view_initialized[view_id] = true;
    }
}

void mock_view_dispatcher_remove_view(void* dispatcher, MockViewType view_id) {
    if(test_instance && view_id > 0 && view_id <= 6) {
        test_instance->view_initialized[view_id] = false;
    }
}

void mock_view_dispatcher_attach_to_gui(void* dispatcher, void* gui, int type) {
    // Mock implementation
}

void mock_view_dispatcher_run(void* dispatcher) {
    // Mock implementation - simulate running
    test_instance->is_running = true;
}

void mock_view_set_previous_callback(void* view, void* callback) {
    // Mock implementation
}

// Test setup and teardown
void setUp(void) {
    view_switch_count = 0;
    last_switched_view = MockViewSplash;
    state_change_count = 0;

    // Allocate test instance
    test_instance = malloc(sizeof(MockUidBruteSmarter));
    TEST_ASSERT_NOT_NULL_MESSAGE(test_instance, "Failed to allocate test instance");

    // Initialize app data
    test_instance->app = malloc(sizeof(MockUidBruteSmartApp));
    TEST_ASSERT_NOT_NULL_MESSAGE(test_instance->app, "Failed to allocate app data");
    memset(test_instance->app, 0, sizeof(MockUidBruteSmartApp));

    // Set default values
    test_instance->app->delay_ms = 500;
    test_instance->app->pause_every = 0;
    test_instance->app->pause_duration_s = 3;
    test_instance->app->uid_count = 0;
    test_instance->app->is_running = false;
    test_instance->app->should_stop = false;

    // Initialize key storage
    test_instance->loaded_keys_count = 0;
    memset(test_instance->loaded_keys, 0, sizeof(test_instance->loaded_keys));

    // Initialize GUI components
    test_instance->gui = malloc(1);
    test_instance->view_dispatcher = mock_view_dispatcher_alloc();
    test_instance->submenu = mock_submenu_alloc();
    test_instance->variable_item_list = mock_variable_item_list_alloc();
    test_instance->popup = mock_popup_alloc();
    test_instance->dialog_ex = mock_dialog_ex_alloc();

    // Initialize UI state
    test_instance->current_view = MockViewSplash;
    test_instance->previous_view = MockViewSplash;
    memset(test_instance->view_initialized, 0, sizeof(test_instance->view_initialized));

    // Initialize state flags
    test_instance->is_running = false;
    test_instance->should_stop = false;
    test_instance->popup_enabled = false;
    test_instance->popup_timeout = 0;

    // Initialize file path
    strcpy(test_instance->file_path, "");
}

void tearDown(void) {
    if(test_instance) {
        // Clean up key memory
        for(int i = 0; i < test_instance->loaded_keys_count; i++) {
            free(test_instance->loaded_keys[i].name);
            free(test_instance->loaded_keys[i].file_path);
        }

        // Clean up GUI components
        if(test_instance->app) free(test_instance->app);
        if(test_instance->gui) free(test_instance->gui);
        if(test_instance->view_dispatcher)
            mock_view_dispatcher_free(test_instance->view_dispatcher);
        if(test_instance->submenu) mock_submenu_free(test_instance->submenu);
        if(test_instance->variable_item_list)
            mock_variable_item_list_free(test_instance->variable_item_list);
        if(test_instance->popup) mock_popup_free(test_instance->popup);
        if(test_instance->dialog_ex) mock_dialog_ex_free(test_instance->dialog_ex);

        free(test_instance);
        test_instance = NULL;
    }
}

// Helper functions
void add_test_key(MockUidBruteSmarter* instance, uint32_t uid, const char* name) {
    if(instance->loaded_keys_count >= 5) return;

    instance->loaded_keys[instance->loaded_keys_count].uid = uid;
    instance->loaded_keys[instance->loaded_keys_count].name = strdup(name);
    instance->loaded_keys[instance->loaded_keys_count].file_path = strdup("/test/path.nfc");
    instance->loaded_keys[instance->loaded_keys_count].loaded_time = 12345;
    instance->loaded_keys[instance->loaded_keys_count].is_active = true;

    instance->app->uids[instance->app->uid_count] = uid;
    instance->app->uid_count++;
    instance->loaded_keys_count++;
}

void simulate_full_app_initialization(void) {
    // Simulate complete app initialization
    mock_view_dispatcher_add_view(
        test_instance->view_dispatcher, MockViewSplash, test_instance->dialog_ex);
    mock_view_dispatcher_add_view(
        test_instance->view_dispatcher, MockViewMenu, test_instance->submenu);
    mock_view_dispatcher_add_view(
        test_instance->view_dispatcher, MockViewConfig, test_instance->variable_item_list);
    mock_view_dispatcher_add_view(
        test_instance->view_dispatcher, MockViewBrute, test_instance->popup);
    mock_view_dispatcher_add_view(
        test_instance->view_dispatcher, MockViewCardLoader, test_instance->submenu);
    mock_view_dispatcher_add_view(
        test_instance->view_dispatcher, MockViewKeyList, test_instance->submenu);

    mock_view_dispatcher_attach_to_gui(test_instance->view_dispatcher, test_instance->gui, 1);
}

// Test functions
void test_ui_state_initialization(void) {
    // Test that UI state is properly initialized
    TEST_ASSERT_EQUAL_INT(MockViewSplash, test_instance->current_view);
    TEST_ASSERT_EQUAL_INT(MockViewSplash, test_instance->previous_view);
    TEST_ASSERT_FALSE(test_instance->is_running);
    TEST_ASSERT_FALSE(test_instance->should_stop);
    TEST_ASSERT_EQUAL_INT(0, view_switch_count);
    TEST_ASSERT_EQUAL_INT(0, state_change_count);

    // Test that GUI components are allocated
    TEST_ASSERT_NOT_NULL(test_instance->gui);
    TEST_ASSERT_NOT_NULL(test_instance->view_dispatcher);
    TEST_ASSERT_NOT_NULL(test_instance->submenu);
    TEST_ASSERT_NOT_NULL(test_instance->variable_item_list);
    TEST_ASSERT_NOT_NULL(test_instance->popup);
    TEST_ASSERT_NOT_NULL(test_instance->dialog_ex);
}

void test_ui_state_view_registration(void) {
    // Test view registration and initialization
    simulate_full_app_initialization();

    // Verify all views are registered
    for(int i = 1; i <= 6; i++) {
        TEST_ASSERT_TRUE_MESSAGE(test_instance->view_initialized[i], "View should be initialized");
    }
}

void test_ui_state_navigation_flow(void) {
    // Test complete navigation flow
    simulate_full_app_initialization();

    // Splash -> Menu
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewMenu);
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->current_view);
    TEST_ASSERT_EQUAL_INT(MockViewSplash, test_instance->previous_view);
    TEST_ASSERT_EQUAL_INT(1, view_switch_count);

    // Menu -> Config
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewConfig);
    TEST_ASSERT_EQUAL_INT(MockViewConfig, test_instance->current_view);
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->previous_view);
    TEST_ASSERT_EQUAL_INT(2, view_switch_count);

    // Config -> Menu (back)
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewMenu);
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->current_view);
    TEST_ASSERT_EQUAL_INT(MockViewConfig, test_instance->previous_view);
    TEST_ASSERT_EQUAL_INT(3, view_switch_count);

    // Menu -> Key List
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewKeyList);
    TEST_ASSERT_EQUAL_INT(MockViewKeyList, test_instance->current_view);
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->previous_view);
    TEST_ASSERT_EQUAL_INT(4, view_switch_count);

    // Key List -> Menu (back)
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewMenu);
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->current_view);
    TEST_ASSERT_EQUAL_INT(MockViewKeyList, test_instance->previous_view);
    TEST_ASSERT_EQUAL_INT(5, view_switch_count);
}

void test_ui_state_with_loaded_keys(void) {
    // Test UI state management with loaded keys
    simulate_full_app_initialization();

    // Add test keys
    add_test_key(test_instance, 0x12345678, "Test Key 1");
    add_test_key(test_instance, 0x87654321, "Test Key 2");

    TEST_ASSERT_EQUAL_UINT8(2, test_instance->loaded_keys_count);
    TEST_ASSERT_EQUAL_UINT8(2, test_instance->app->uid_count);

    // Navigate to key list
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewKeyList);
    TEST_ASSERT_EQUAL_INT(MockViewKeyList, test_instance->current_view);

    // Simulate removing a key
    free(test_instance->loaded_keys[0].name);
    free(test_instance->loaded_keys[0].file_path);

    // Shift remaining keys
    for(int i = 0; i < test_instance->loaded_keys_count - 1; i++) {
        test_instance->loaded_keys[i] = test_instance->loaded_keys[i + 1];
    }

    // Update app uids
    for(int i = 0; i < test_instance->app->uid_count; i++) {
        if(test_instance->app->uids[i] == 0x12345678) {
            for(int j = i; j < test_instance->app->uid_count - 1; j++) {
                test_instance->app->uids[j] = test_instance->app->uids[j + 1];
            }
            test_instance->app->uid_count--;
            break;
        }
    }

    test_instance->loaded_keys_count--;

    // Verify state consistency
    TEST_ASSERT_EQUAL_UINT8(1, test_instance->loaded_keys_count);
    TEST_ASSERT_EQUAL_UINT8(1, test_instance->app->uid_count);
    TEST_ASSERT_EQUAL_UINT32(0x87654321, test_instance->loaded_keys[0].uid);
}

void test_ui_state_brute_force_integration(void) {
    // Test UI state during brute force operation
    simulate_full_app_initialization();

    // Add a key
    add_test_key(test_instance, 0x12345678, "Test Key 1");

    // Navigate to brute force view
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewBrute);
    TEST_ASSERT_EQUAL_INT(MockViewBrute, test_instance->current_view);

    // Simulate brute force starting
    test_instance->app->is_running = true;
    test_instance->app->should_stop = false;
    test_instance->is_running = true;

    TEST_ASSERT_TRUE(test_instance->app->is_running);
    TEST_ASSERT_FALSE(test_instance->app->should_stop);
    TEST_ASSERT_TRUE(test_instance->is_running);

    // Simulate brute force completion
    test_instance->app->is_running = false;
    test_instance->is_running = false;

    // Return to main menu
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewMenu);
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->current_view);
    TEST_ASSERT_FALSE(test_instance->app->is_running);
    TEST_ASSERT_FALSE(test_instance->is_running);
}

void test_ui_state_configuration_persistence(void) {
    // Test that configuration persists across view changes
    simulate_full_app_initialization();

    // Set custom configuration
    test_instance->app->delay_ms = 800;
    test_instance->app->pause_every = 10;
    test_instance->app->pause_duration_s = 5;

    // Navigate through different views
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewConfig);
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewKeyList);
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewBrute);
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewMenu);

    // Verify configuration is preserved
    TEST_ASSERT_EQUAL_UINT32(800, test_instance->app->delay_ms);
    TEST_ASSERT_EQUAL_UINT32(10, test_instance->app->pause_every);
    TEST_ASSERT_EQUAL_UINT32(5, test_instance->app->pause_duration_s);
}

void test_ui_state_error_recovery(void) {
    // Test UI state recovery from error conditions
    simulate_full_app_initialization();

    // Navigate to a view
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewConfig);
    TEST_ASSERT_EQUAL_INT(MockViewConfig, test_instance->current_view);

    // Simulate error condition
    test_instance->should_stop = true;

    // Recover by returning to main menu
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewMenu);
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->current_view);

    // Verify error state is cleared
    TEST_ASSERT_FALSE(test_instance->should_stop);

    // Verify normal operation can continue
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewKeyList);
    TEST_ASSERT_EQUAL_INT(MockViewKeyList, test_instance->current_view);
}

void test_ui_state_memory_management(void) {
    // Test proper memory management during state changes
    simulate_full_app_initialization();

    // Add and remove keys multiple times
    for(int cycle = 0; cycle < 3; cycle++) {
        // Add keys
        add_test_key(test_instance, 0x12345678 + cycle, "Test Key 1");
        add_test_key(test_instance, 0x87654321 + cycle, "Test Key 2");

        TEST_ASSERT_EQUAL_UINT8(2, test_instance->loaded_keys_count);

        // Remove all keys
        for(int i = test_instance->loaded_keys_count - 1; i >= 0; i--) {
            free(test_instance->loaded_keys[i].name);
            free(test_instance->loaded_keys[i].file_path);
        }

        test_instance->loaded_keys_count = 0;
        test_instance->app->uid_count = 0;

        TEST_ASSERT_EQUAL_UINT8(0, test_instance->loaded_keys_count);
        TEST_ASSERT_EQUAL_UINT8(0, test_instance->app->uid_count);
    }

    // Verify system is still stable
    TEST_ASSERT_EQUAL_INT(MockViewSplash, test_instance->current_view);
    TEST_ASSERT_NOT_NULL(test_instance->view_dispatcher);
    TEST_ASSERT_NOT_NULL(test_instance->submenu);
}

void test_ui_state_concurrent_operations(void) {
    // Test UI state management during concurrent operations
    simulate_full_app_initialization();

    // Add keys
    add_test_key(test_instance, 0x12345678, "Test Key 1");
    add_test_key(test_instance, 0x87654321, "Test Key 2");

    // Start brute force
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewBrute);
    test_instance->app->is_running = true;

    // Simulate user trying to navigate while brute force is running
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, MockViewMenu);

    // Verify state consistency
    TEST_ASSERT_EQUAL_INT(MockViewMenu, test_instance->current_view);
    TEST_ASSERT_TRUE(test_instance->app->is_running); // Brute force still running

    // Stop brute force
    test_instance->app->is_running = false;
    test_instance->app->should_stop = true;

    // Verify final state
    TEST_ASSERT_FALSE(test_instance->app->is_running);
    TEST_ASSERT_TRUE(test_instance->app->should_stop);
}

void test_ui_state_view_cleanup(void) {
    // Test proper cleanup of views
    simulate_full_app_initialization();

    // Verify all views are initialized
    for(int i = 1; i <= 6; i++) {
        TEST_ASSERT_TRUE(test_instance->view_initialized[i]);
    }

    // Remove views
    mock_view_dispatcher_remove_view(test_instance->view_dispatcher, MockViewSplash);
    mock_view_dispatcher_remove_view(test_instance->view_dispatcher, MockViewMenu);
    mock_view_dispatcher_remove_view(test_instance->view_dispatcher, MockViewConfig);
    mock_view_dispatcher_remove_view(test_instance->view_dispatcher, MockViewBrute);
    mock_view_dispatcher_remove_view(test_instance->view_dispatcher, MockViewCardLoader);
    mock_view_dispatcher_remove_view(test_instance->view_dispatcher, MockViewKeyList);

    // Verify views are cleaned up
    for(int i = 1; i <= 6; i++) {
        TEST_ASSERT_FALSE(test_instance->view_initialized[i]);
    }
}

// Main test runner
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_ui_state_initialization);
    RUN_TEST(test_ui_state_view_registration);
    RUN_TEST(test_ui_state_navigation_flow);
    RUN_TEST(test_ui_state_with_loaded_keys);
    RUN_TEST(test_ui_state_brute_force_integration);
    RUN_TEST(test_ui_state_configuration_persistence);
    RUN_TEST(test_ui_state_error_recovery);
    RUN_TEST(test_ui_state_memory_management);
    RUN_TEST(test_ui_state_concurrent_operations);
    RUN_TEST(test_ui_state_view_cleanup);

    return UNITY_END();
}
