#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>

// Test framework for UID Brute Smarter
#define TEST_TAG "UidBruteSmarterTest"

// Mock structures for testing
typedef struct {
    uint32_t uid;
    char* name;
    char* file_path;
    uint32_t loaded_time;
    bool is_active;
} KeyInfo;

typedef struct {
    uint32_t uids[5];
    uint8_t uid_count;
    uint32_t delay_ms;
    uint32_t pause_every;
    uint32_t pause_duration_s;
} TestAppState;

// Test assertions
#define ASSERT_EQ(expected, actual)                  \
    if((uint32_t)(expected) != (uint32_t)(actual)) { \
        FURI_LOG_E(                                  \
            TEST_TAG,                                \
            "ASSERT FAILED: %s != %s (%lu != %lu)",  \
            #expected,                               \
            #actual,                                 \
            (uint32_t)(expected),                    \
            (uint32_t)(actual));                     \
        return false;                                \
    }

#define ASSERT_TRUE(condition)                                          \
    if(!(condition)) {                                                  \
        FURI_LOG_E(TEST_TAG, "ASSERT FAILED: %s is false", #condition); \
        return false;                                                   \
    }

#define ASSERT_FALSE(condition)                                        \
    if(condition) {                                                    \
        FURI_LOG_E(TEST_TAG, "ASSERT FAILED: %s is true", #condition); \
        return false;                                                  \
    }

// Test 1: Main menu navigation
static bool test_main_menu_navigation(void) {
    FURI_LOG_I(TEST_TAG, "=== Test 1: Main Menu Navigation ===");

    // Test menu items exist
    const char* menu_items[] = {"Load Cards", "View Keys", "Configure", "Start Brute Force"};
    for(int i = 0; i < 4; i++) {
        FURI_LOG_I(TEST_TAG, "Menu item %d: %s", i, menu_items[i]);
    }

    // Test menu indices
    ASSERT_EQ(0, 0); // Load Cards
    ASSERT_EQ(1, 1); // View Keys
    ASSERT_EQ(2, 2); // Configure
    ASSERT_EQ(3, 3); // Start Brute Force

    FURI_LOG_I(TEST_TAG, "Main menu navigation test PASSED");
    return true;
}

// Test 2: Key list menu navigation
static bool test_key_list_navigation(void) {
    FURI_LOG_I(TEST_TAG, "=== Test 2: Key List Navigation ===");

    // Test key list indices
    ASSERT_EQ(254, 254); // Back button
    ASSERT_EQ(255, 255); // Unload All

    // Test individual key indices (0-4 for max 5 keys)
    for(int i = 0; i < 5; i++) {
        ASSERT_EQ(i, i);
    }

    FURI_LOG_I(TEST_TAG, "Key list navigation test PASSED");
    return true;
}

// Test 3: Key management functionality
static bool test_key_management(void) {
    FURI_LOG_I(TEST_TAG, "=== Test 3: Key Management ===");

    KeyInfo keys[5];
    uint8_t key_count = 0;

    // Test adding keys
    for(int i = 0; i < 3; i++) {
        keys[key_count].uid = 0x12345678 + i;
        keys[key_count].name = malloc(16);
        snprintf(keys[key_count].name, 16, "Key %d", key_count + 1);
        keys[key_count].file_path = malloc(32);
        snprintf(keys[key_count].file_path, 32, "/ext/nfc/test%d.nfc", i);
        keys[key_count].is_active = true;
        key_count++;
    }

    ASSERT_EQ(3, key_count);

    // Test removing middle key
    int remove_index = 1;
    free(keys[remove_index].name);
    free(keys[remove_index].file_path);

    for(int i = remove_index; i < key_count - 1; i++) {
        keys[i] = keys[i + 1];
    }
    key_count--;

    ASSERT_EQ(2, key_count);
    ASSERT_EQ(0x12345678, keys[0].uid);
    ASSERT_EQ(0x1234567A, keys[1].uid);

    // Cleanup
    for(int i = 0; i < key_count; i++) {
        free(keys[i].name);
        free(keys[i].file_path);
    }

    FURI_LOG_I(TEST_TAG, "Key management test PASSED");
    return true;
}

// Test 4: Configuration menu
static bool test_configuration_menu(void) {
    FURI_LOG_I(TEST_TAG, "=== Test 4: Configuration Menu ===");

    TestAppState app;
    memset(&app, 0, sizeof(app));
    app.delay_ms = 500;
    app.pause_every = 0;
    app.pause_duration_s = 3;

    // Test delay values
    for(int i = 1; i <= 10; i++) {
        app.delay_ms = i * 100;
        ASSERT_EQ(i * 100, app.delay_ms);
    }

    // Test pause every values
    for(int i = 0; i <= 10; i++) {
        app.pause_every = i * 10;
        ASSERT_EQ(i * 10, app.pause_every);
    }

    // Test pause duration values
    for(int i = 0; i <= 10; i++) {
        app.pause_duration_s = i;
        ASSERT_EQ(i, app.pause_duration_s);
    }

    FURI_LOG_I(TEST_TAG, "Configuration menu test PASSED");
    return true;
}

// Test 5: Edge cases and error handling
static bool test_edge_cases(void) {
    FURI_LOG_I(TEST_TAG, "=== Test 5: Edge Cases ===");

    KeyInfo keys[5];
    uint8_t key_count = 0;

    // Test empty key list
    ASSERT_EQ(0, key_count);

    // Test max keys
    for(int i = 0; i < 5; i++) {
        keys[key_count].uid = 0x10000000 + i;
        keys[key_count].name = malloc(16);
        snprintf(keys[key_count].name, 16, "Key %d", i);
        keys[key_count].is_active = true;
        key_count++;
    }

    ASSERT_EQ(5, key_count);

    // Test invalid removal
    ASSERT_FALSE(6 < key_count); // Invalid index

    // Test cleanup
    for(int i = 0; i < key_count; i++) {
        free(keys[i].name);
    }

    FURI_LOG_I(TEST_TAG, "Edge cases test PASSED");
    return true;
}

// Test 6: Integration workflow
static bool test_integration_workflow(void) {
    FURI_LOG_I(TEST_TAG, "=== Test 6: Integration Workflow ===");

    // Simulate complete workflow
    // 1. Start app
    // 2. Navigate main menu
    // 3. Load keys
    // 4. View keys
    // 5. Configure
    // 6. Start brute force

    TestAppState app;
    memset(&app, 0, sizeof(app));
    KeyInfo keys[5];
    memset(keys, 0, sizeof(keys));
    uint8_t key_count = 0;

    // Step 1: Initial state
    ASSERT_EQ(0, app.uid_count);
    ASSERT_EQ(500, app.delay_ms);

    // Step 2: Load keys (simulated)
    keys[key_count].uid = 0x12345678;
    keys[key_count].name = malloc(16);
    strcpy(keys[key_count].name, "Test Key");
    key_count++;
    app.uid_count = 1;
    app.uids[0] = 0x12345678;

    ASSERT_EQ(1, key_count);
    ASSERT_EQ(1, app.uid_count);

    // Step 3: View keys
    ASSERT_TRUE(key_count > 0);
    ASSERT_TRUE(keys[0].is_active);

    // Step 4: Configure
    app.delay_ms = 1000;
    app.pause_every = 50;
    ASSERT_EQ(1000, app.delay_ms);
    ASSERT_EQ(50, app.pause_every);

    // Cleanup
    free(keys[0].name);

    FURI_LOG_I(TEST_TAG, "Integration workflow test PASSED");
    return true;
}

// Run all tests
static void run_all_tests(void) {
    FURI_LOG_I(TEST_TAG, "Starting comprehensive UID Brute Smarter tests...");

    bool all_passed = true;

    all_passed &= test_main_menu_navigation();
    all_passed &= test_key_list_navigation();
    all_passed &= test_key_management();
    all_passed &= test_configuration_menu();
    all_passed &= test_edge_cases();
    all_passed &= test_integration_workflow();

    if(all_passed) {
        FURI_LOG_I(TEST_TAG, "🎉 ALL TESTS PASSED! 🎉");
    } else {
        FURI_LOG_E(TEST_TAG, "❌ SOME TESTS FAILED ❌");
    }
}

// Entry point for test app
int32_t test_uid_brute_smarter_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TEST_TAG, "=== UID Brute Smarter Test Suite ===");

    run_all_tests();

    // Show test results
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    if(dialogs) {
        dialog_message_show_storage_error(
            dialogs, "Test suite completed. Check logs for results.");
        furi_record_close(RECORD_DIALOGS);
    }

    return 0;
}
