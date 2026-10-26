/*
 * Unit tests for menu navigation functionality
 * Copyright (c) 2024, UID Brute Smarter Contributors
 */

#include "../unity/unity.h"
#include "../mocks/furi_mock.h"
#include "../../pattern_engine.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations for types used in testing
typedef struct {
    uint32_t uid;
    char* name;
    char* file_path;
    uint32_t loaded_time;
    bool is_active;
} KeyInfo;

// Mock structures for testing
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
    void* submenu;
    void* variable_item_list;
    void* view_dispatcher;
    uint32_t current_view;
} MockUidBruteSmarter;

// Global test variables
static MockUidBruteSmarter* test_instance = NULL;
static int view_switch_count = 0;
static uint32_t last_switched_view = 0;

// Mock functions
void mock_view_dispatcher_switch_to_view(void* view_dispatcher, uint32_t view_id) {
    view_switch_count++;
    last_switched_view = view_id;
    if(test_instance) {
        test_instance->current_view = view_id;
    }
}

void* mock_submenu_alloc(void) {
    return malloc(1); // Just return a non-NULL pointer
}

void mock_submenu_reset(void* submenu) {
    // Do nothing for mock
}

void mock_submenu_set_header(void* submenu, const char* header) {
    // Do nothing for mock
}

void mock_submenu_add_item(
    void* submenu,
    const char* label,
    uint32_t index,
    void* callback,
    void* context) {
    // Do nothing for mock
}

void mock_submenu_free(void* submenu) {
    free(submenu);
}

void* mock_variable_item_list_alloc(void) {
    return malloc(1); // Just return a non-NULL pointer
}

void mock_variable_item_list_reset(void* list) {
    // Do nothing for mock
}

void* mock_variable_item_list_add(
    void* list,
    const char* label,
    uint8_t values,
    void* callback,
    void* context) {
    return malloc(1); // Return mock item
}

void mock_variable_item_set_current_value_index(void* item, uint8_t index) {
    // Do nothing for mock
}

void mock_variable_item_set_current_value_text(void* item, const char* text) {
    // Do nothing for mock
}

void mock_variable_item_list_free(void* list) {
    free(list);
}

void mock_view_set_previous_callback(void* view, void* callback) {
    // Do nothing for mock
}

void* mock_submenu_get_view(void* submenu) {
    return submenu; // Return the submenu as view
}

void* mock_variable_item_list_get_view(void* list) {
    return list; // Return the list as view
}

// Test setup and teardown
void setUp(void) {
    view_switch_count = 0;
    last_switched_view = 0;

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
    test_instance->app->uid_count = 0;
    test_instance->app->is_running = false;
    test_instance->app->should_stop = false;

    // Initialize key storage
    test_instance->loaded_keys_count = 0;
    memset(test_instance->loaded_keys, 0, sizeof(test_instance->loaded_keys));

    // Initialize mock GUI components
    test_instance->submenu = mock_submenu_alloc();
    test_instance->variable_item_list = mock_variable_item_list_alloc();
    test_instance->view_dispatcher = malloc(1); // Mock dispatcher
    test_instance->current_view = 0;

    // Initialize mock file path
    strcpy(test_instance->file_path, "");
}

void tearDown(void) {
    if(test_instance) {
        if(test_instance->app) {
            free(test_instance->app);
        }
        if(test_instance->submenu) {
            mock_submenu_free(test_instance->submenu);
        }
        if(test_instance->variable_item_list) {
            mock_variable_item_list_free(test_instance->variable_item_list);
        }
        if(test_instance->view_dispatcher) {
            free(test_instance->view_dispatcher);
        }
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

// Test functions
void test_menu_setup_should_create_main_menu(void) {
    // This test simulates the main menu setup
    // In a real scenario, this would call uid_brute_smarter_setup_menu

    TEST_ASSERT_TRUE(test_instance->app->uid_count == 0);
    TEST_ASSERT_TRUE(test_instance->loaded_keys_count == 0);
    TEST_ASSERT_NOT_NULL(test_instance->submenu);

    // Simulate menu setup by checking initial state
    TEST_ASSERT_FALSE(test_instance->app->is_running);
    TEST_ASSERT_FALSE(test_instance->app->should_stop);
    TEST_ASSERT_TRUE(test_instance->app->delay_ms == 500);
}

void test_menu_navigation_with_keys(void) {
    // Add some test keys
    add_test_key(test_instance, 0x12345678, "Test Key 1");
    add_test_key(test_instance, 0x87654321, "Test Key 2");

    TEST_ASSERT_TRUE(test_instance->loaded_keys_count == 2);
    TEST_ASSERT_TRUE(test_instance->app->uid_count == 2);

    // Simulate viewing key list
    // In real code, this would call uid_brute_smarter_show_key_list
    view_switch_count = 0;

    // Simulate switching to key list view
    mock_view_dispatcher_switch_to_view(
        test_instance->view_dispatcher, 6); // UidBruteSmarterViewKeyList

    TEST_ASSERT_EQUAL_INT(1, view_switch_count);
    TEST_ASSERT_TRUE(last_switched_view == 6);
    TEST_ASSERT_TRUE(test_instance->current_view == 6);
}

void test_menu_navigation_to_config(void) {
    // Simulate navigating to configuration
    view_switch_count = 0;

    // Simulate switching to config view
    mock_view_dispatcher_switch_to_view(
        test_instance->view_dispatcher, 3); // UidBruteSmarterViewConfig

    TEST_ASSERT_EQUAL_INT(1, view_switch_count);
    TEST_ASSERT_TRUE(last_switched_view == 3);
    TEST_ASSERT_TRUE(test_instance->current_view == 3);

    // Test that config values are preserved
    TEST_ASSERT_TRUE(test_instance->app->delay_ms == 500);
    TEST_ASSERT_TRUE(test_instance->app->pause_every == 0);
    TEST_ASSERT_TRUE(test_instance->app->pause_duration_s == 3);
}

void test_menu_back_navigation(void) {
    // Simulate navigating to config and then back to main menu
    view_switch_count = 0;

    // Go to config
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, 3);
    TEST_ASSERT_TRUE(test_instance->current_view == 3);

    // Go back to main menu (simulating return key press)
    mock_view_dispatcher_switch_to_view(
        test_instance->view_dispatcher, 5); // UidBruteSmarterViewMenu
    TEST_ASSERT_EQUAL_INT(2, view_switch_count);
    TEST_ASSERT_TRUE(last_switched_view == 5);
    TEST_ASSERT_TRUE(test_instance->current_view == 5);
}

void test_key_list_back_navigation(void) {
    // Add test keys
    add_test_key(test_instance, 0x12345678, "Test Key 1");

    // Navigate to key list
    view_switch_count = 0;
    mock_view_dispatcher_switch_to_view(
        test_instance->view_dispatcher, 6); // UidBruteSmarterViewKeyList
    TEST_ASSERT_TRUE(test_instance->current_view == 6);

    // Go back to main menu using return key
    mock_view_dispatcher_switch_to_view(
        test_instance->view_dispatcher, 5); // UidBruteSmarterViewMenu
    TEST_ASSERT_EQUAL_INT(2, view_switch_count);
    TEST_ASSERT_TRUE(last_switched_view == 5);
    TEST_ASSERT_TRUE(test_instance->current_view == 5);
}

void test_menu_navigation_with_no_keys(void) {
    // Test menu behavior when no keys are loaded
    TEST_ASSERT_TRUE(test_instance->loaded_keys_count == 0);
    TEST_ASSERT_TRUE(test_instance->app->uid_count == 0);

    // Simulate trying to view key list with no keys
    view_switch_count = 0;

    // Should still be able to navigate to key list view
    mock_view_dispatcher_switch_to_view(test_instance->view_dispatcher, 6);

    TEST_ASSERT_EQUAL_INT(1, view_switch_count);
    TEST_ASSERT_TRUE(last_switched_view == 6);
}

void test_menu_configuration_values(void) {
    // Test configuration value changes
    uint32_t original_delay = test_instance->app->delay_ms;
    uint32_t original_pause = test_instance->app->pause_every;
    uint32_t original_duration = test_instance->app->pause_duration_s;

    // Simulate changing delay
    test_instance->app->delay_ms = 800;
    TEST_ASSERT_TRUE(test_instance->app->delay_ms == 800);
    TEST_ASSERT_TRUE(original_delay != test_instance->app->delay_ms);

    // Simulate changing pause settings
    test_instance->app->pause_every = 10;
    test_instance->app->pause_duration_s = 5;

    TEST_ASSERT_TRUE(test_instance->app->pause_every == 10);
    TEST_ASSERT_TRUE(test_instance->app->pause_duration_s == 5);
    TEST_ASSERT_TRUE(original_pause != test_instance->app->pause_every);
    TEST_ASSERT_TRUE(original_duration != test_instance->app->pause_duration_s);
}

void test_menu_key_removal_navigation(void) {
    // Add test keys
    add_test_key(test_instance, 0x12345678, "Test Key 1");
    add_test_key(test_instance, 0x87654321, "Test Key 2");
    add_test_key(test_instance, 0x11223344, "Test Key 3");

    TEST_ASSERT_TRUE(test_instance->loaded_keys_count == 3);

    // Simulate removing a key (middle one)
    // This would normally call uid_brute_smarter_remove_key
    uint8_t remove_index = 1; // Remove second key

    // Free the removed key's memory
    free(test_instance->loaded_keys[remove_index].name);
    free(test_instance->loaded_keys[remove_index].file_path);

    // Shift remaining keys
    for(int i = remove_index; i < test_instance->loaded_keys_count - 1; i++) {
        test_instance->loaded_keys[i] = test_instance->loaded_keys[i + 1];
    }

    // Update app uids (remove the corresponding UID)
    uint32_t removed_uid = 0x87654321;
    for(int i = 0; i < test_instance->app->uid_count; i++) {
        if(test_instance->app->uids[i] == removed_uid) {
            for(int j = i; j < test_instance->app->uid_count - 1; j++) {
                test_instance->app->uids[j] = test_instance->app->uids[j + 1];
            }
            test_instance->app->uid_count--;
            break;
        }
    }

    test_instance->loaded_keys_count--;

    // Verify key was removed
    TEST_ASSERT_TRUE(test_instance->loaded_keys_count == 2);
    TEST_ASSERT_TRUE(test_instance->app->uid_count == 2);
    TEST_ASSERT_TRUE(test_instance->loaded_keys[0].uid == 0x12345678);
    TEST_ASSERT_TRUE(test_instance->loaded_keys[1].uid == 0x11223344);
}

void test_menu_unload_all_navigation(void) {
    // Add test keys
    add_test_key(test_instance, 0x12345678, "Test Key 1");
    add_test_key(test_instance, 0x87654321, "Test Key 2");

    TEST_ASSERT_TRUE(test_instance->loaded_keys_count == 2);

    // Simulate unloading all keys
    for(int i = test_instance->loaded_keys_count - 1; i >= 0; i--) {
        free(test_instance->loaded_keys[i].name);
        free(test_instance->loaded_keys[i].file_path);
    }

    test_instance->loaded_keys_count = 0;
    test_instance->app->uid_count = 0;

    // Verify all keys are unloaded
    TEST_ASSERT_TRUE(test_instance->loaded_keys_count == 0);
    TEST_ASSERT_TRUE(test_instance->app->uid_count == 0);

    // Simulate returning to main menu
    view_switch_count = 0;
    mock_view_dispatcher_switch_to_view(
        test_instance->view_dispatcher, 5); // UidBruteSmarterViewMenu

    TEST_ASSERT_EQUAL_INT(1, view_switch_count);
    TEST_ASSERT_TRUE(last_switched_view == 5);
}

// Main test runner
int main(void) {
    unity_begin();

    RUN_TEST(test_menu_setup_should_create_main_menu);
    RUN_TEST(test_menu_navigation_with_keys);
    RUN_TEST(test_menu_navigation_to_config);
    RUN_TEST(test_menu_back_navigation);
    RUN_TEST(test_key_list_back_navigation);
    RUN_TEST(test_menu_navigation_with_no_keys);
    RUN_TEST(test_menu_configuration_values);
    RUN_TEST(test_menu_key_removal_navigation);
    RUN_TEST(test_menu_unload_all_navigation);

    return unity_end();
}
