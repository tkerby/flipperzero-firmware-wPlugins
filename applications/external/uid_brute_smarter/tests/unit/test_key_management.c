/* Comprehensive unit tests for key management */
#include "../unity/unity.h"
#include "../mocks/furi_mock.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Ensure strdup is available for systems that don't have it
static char* test_strdup(const char* s) {
    if(!s) return NULL;
    size_t len = strlen(s) + 1;
    char* result = malloc(len);
    if(result) {
        memcpy(result, s, len);
    }
    return result;
}

// Key management structures and functions (extracted from uid_brute_smarter.c)
#define MAX_KEYS 5

typedef struct {
    uint32_t uid;
    char* name;
    char* file_path;
    uint32_t loaded_time;
    bool is_active;
} KeyInfo;

typedef struct {
    KeyInfo keys[MAX_KEYS];
    uint8_t key_count;
} KeyManager;

// Key management functions
static void key_manager_init(KeyManager* manager) {
    memset(manager, 0, sizeof(KeyManager));
    for(int i = 0; i < MAX_KEYS; i++) {
        manager->keys[i].name = NULL;
        manager->keys[i].file_path = NULL;
        manager->keys[i].is_active = false;
    }
}

static void key_manager_cleanup(KeyManager* manager) {
    for(int i = 0; i < manager->key_count; i++) {
        free(manager->keys[i].name);
        free(manager->keys[i].file_path);
        manager->keys[i].name = NULL;
        manager->keys[i].file_path = NULL;
    }
    manager->key_count = 0;
}

static bool key_manager_add_key(
    KeyManager* manager,
    uint32_t uid,
    const char* name,
    const char* file_path) {
    if(manager->key_count >= MAX_KEYS) {
        return false;
    }

    // Check for duplicates
    for(int i = 0; i < manager->key_count; i++) {
        if(manager->keys[i].uid == uid) {
            return false; // Duplicate UID
        }
    }

    KeyInfo* key = &manager->keys[manager->key_count];
    key->uid = uid;
    key->name = test_strdup(name);
    key->file_path = test_strdup(file_path);
    key->loaded_time = furi_get_tick();
    if(key->loaded_time == 0) {
        key->loaded_time = 1; // Ensure non-zero
    }
    key->is_active = true;

    manager->key_count++;
    return true;
}

static bool key_manager_remove_key(KeyManager* manager, uint8_t index) {
    if(index >= manager->key_count) {
        return false;
    }

    // Free memory
    free(manager->keys[index].name);
    free(manager->keys[index].file_path);

    // Shift remaining keys
    for(int i = index; i < manager->key_count - 1; i++) {
        manager->keys[i] = manager->keys[i + 1];
    }

    manager->key_count--;
    return true;
}

static KeyInfo* key_manager_get_key(KeyManager* manager, uint8_t index) {
    if(index >= manager->key_count) {
        return NULL;
    }
    return &manager->keys[index];
}

static int key_manager_find_key_by_uid(KeyManager* manager, uint32_t uid) {
    for(int i = 0; i < manager->key_count; i++) {
        if(manager->keys[i].uid == uid) {
            return i;
        }
    }
    return -1;
}

static bool key_manager_toggle_active(KeyManager* manager, uint8_t index) {
    if(index >= manager->key_count || index >= MAX_KEYS) {
        return false;
    }

    manager->keys[index].is_active = !manager->keys[index].is_active;
    return true;
}

static void key_manager_unload_all(KeyManager* manager) {
    key_manager_cleanup(manager);
}

static uint8_t key_manager_get_active_count(KeyManager* manager) {
    uint8_t count = 0;
    for(int i = 0; i < manager->key_count; i++) {
        if(manager->keys[i].is_active) {
            count++;
        }
    }
    return count;
}

// Test 1: Initialize key manager
static void test_key_manager_init(void) {
    KeyManager manager;
    key_manager_init(&manager);

    TEST_ASSERT_EQUAL_UINT(0, manager.key_count);
    for(int i = 0; i < MAX_KEYS; i++) {
        TEST_ASSERT_NULL(manager.keys[i].name);
        TEST_ASSERT_NULL(manager.keys[i].file_path);
        TEST_ASSERT_FALSE(manager.keys[i].is_active);
    }

    key_manager_cleanup(&manager);
}

// Test 2: Add single key
static void test_key_manager_add_single_key(void) {
    KeyManager manager;
    key_manager_init(&manager);

    TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x12345678, "Test Key", "/test/key.nfc"));
    TEST_ASSERT_EQUAL_UINT(1, manager.key_count);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, manager.keys[0].uid);
    TEST_ASSERT_EQUAL_STRING("Test Key", manager.keys[0].name);
    TEST_ASSERT_EQUAL_STRING("/test/key.nfc", manager.keys[0].file_path);
    TEST_ASSERT_TRUE(manager.keys[0].is_active);
    TEST_ASSERT_TRUE(manager.keys[0].loaded_time > 0);

    key_manager_cleanup(&manager);
}

// Test 3: Add multiple keys
static void test_key_manager_add_multiple_keys(void) {
    KeyManager manager;
    key_manager_init(&manager);

    TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc"));
    TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x22222222, "Key 2", "/key2.nfc"));
    TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x33333333, "Key 3", "/key3.nfc"));

    TEST_ASSERT_EQUAL_UINT(3, manager.key_count);
    TEST_ASSERT_EQUAL_HEX32(0x11111111, manager.keys[0].uid);
    TEST_ASSERT_EQUAL_HEX32(0x22222222, manager.keys[1].uid);
    TEST_ASSERT_EQUAL_HEX32(0x33333333, manager.keys[2].uid);

    key_manager_cleanup(&manager);
}

// Test 4: Add duplicate key (should fail)
static void test_key_manager_add_duplicate_key(void) {
    KeyManager manager;
    key_manager_init(&manager);

    TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x12345678, "Key 1", "/key1.nfc"));
    TEST_ASSERT_FALSE(key_manager_add_key(&manager, 0x12345678, "Key 2", "/key2.nfc"));
    TEST_ASSERT_EQUAL_UINT(1, manager.key_count);

    key_manager_cleanup(&manager);
}

// Test 5: Add maximum keys
static void test_key_manager_add_max_keys(void) {
    KeyManager manager;
    key_manager_init(&manager);

    for(int i = 0; i < MAX_KEYS; i++) {
        char name[32];
        char path[64];
        snprintf(name, sizeof(name), "Key %d", i + 1);
        snprintf(path, sizeof(path), "/key%d.nfc", i + 1);

        TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x10000000 + i, name, path));
    }

    TEST_ASSERT_EQUAL_UINT(MAX_KEYS, manager.key_count);

    // Try to add one more (should fail)
    TEST_ASSERT_FALSE(key_manager_add_key(&manager, 0x99999999, "Overflow Key", "/overflow.nfc"));
    TEST_ASSERT_EQUAL_UINT(MAX_KEYS, manager.key_count);

    key_manager_cleanup(&manager);
}

// Test 6: Remove key from beginning
static void test_key_manager_remove_first_key(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc");
    key_manager_add_key(&manager, 0x22222222, "Key 2", "/key2.nfc");
    key_manager_add_key(&manager, 0x33333333, "Key 3", "/key3.nfc");

    TEST_ASSERT_TRUE(key_manager_remove_key(&manager, 0));
    TEST_ASSERT_EQUAL_UINT(2, manager.key_count);
    TEST_ASSERT_EQUAL_HEX32(0x22222222, manager.keys[0].uid);
    TEST_ASSERT_EQUAL_HEX32(0x33333333, manager.keys[1].uid);

    key_manager_cleanup(&manager);
}

// Test 7: Remove key from middle
static void test_key_manager_remove_middle_key(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc");
    key_manager_add_key(&manager, 0x22222222, "Key 2", "/key2.nfc");
    key_manager_add_key(&manager, 0x33333333, "Key 3", "/key3.nfc");

    TEST_ASSERT_TRUE(key_manager_remove_key(&manager, 1));
    TEST_ASSERT_EQUAL_UINT(2, manager.key_count);
    TEST_ASSERT_EQUAL_HEX32(0x11111111, manager.keys[0].uid);
    TEST_ASSERT_EQUAL_HEX32(0x33333333, manager.keys[1].uid);

    key_manager_cleanup(&manager);
}

// Test 8: Remove key from end
static void test_key_manager_remove_last_key(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc");
    key_manager_add_key(&manager, 0x22222222, "Key 2", "/key2.nfc");
    key_manager_add_key(&manager, 0x33333333, "Key 3", "/key3.nfc");

    TEST_ASSERT_TRUE(key_manager_remove_key(&manager, 2));
    TEST_ASSERT_EQUAL_UINT(2, manager.key_count);
    TEST_ASSERT_EQUAL_HEX32(0x11111111, manager.keys[0].uid);
    TEST_ASSERT_EQUAL_HEX32(0x22222222, manager.keys[1].uid);

    key_manager_cleanup(&manager);
}

// Test 9: Remove invalid key index
static void test_key_manager_remove_invalid_index(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc");

    TEST_ASSERT_FALSE(key_manager_remove_key(&manager, 5));
    TEST_ASSERT_FALSE(key_manager_remove_key(&manager, 1));
    TEST_ASSERT_EQUAL_UINT(1, manager.key_count);

    key_manager_cleanup(&manager);
}

// Test 10: Find key by UID
static void test_key_manager_find_by_uid(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc");
    key_manager_add_key(&manager, 0x22222222, "Key 2", "/key2.nfc");
    key_manager_add_key(&manager, 0x33333333, "Key 3", "/key3.nfc");

    TEST_ASSERT_EQUAL_INT(0, key_manager_find_key_by_uid(&manager, 0x11111111));
    TEST_ASSERT_EQUAL_INT(1, key_manager_find_key_by_uid(&manager, 0x22222222));
    TEST_ASSERT_EQUAL_INT(2, key_manager_find_key_by_uid(&manager, 0x33333333));
    TEST_ASSERT_EQUAL_INT(-1, key_manager_find_key_by_uid(&manager, 0x99999999));

    key_manager_cleanup(&manager);
}

// Test 11: Toggle key active state
static void test_key_manager_toggle_active(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x12345678, "Test Key", "/test.nfc");

    TEST_ASSERT_TRUE(manager.keys[0].is_active);

    TEST_ASSERT_TRUE(key_manager_toggle_active(&manager, 0));
    TEST_ASSERT_FALSE(manager.keys[0].is_active);

    TEST_ASSERT_TRUE(key_manager_toggle_active(&manager, 0));
    TEST_ASSERT_TRUE(manager.keys[0].is_active);

    key_manager_cleanup(&manager);
}

// Test 12: Toggle invalid key index
static void test_key_manager_toggle_invalid_index(void) {
    KeyManager manager;
    key_manager_init(&manager);

    TEST_ASSERT_FALSE(key_manager_toggle_active(&manager, 0));
    TEST_ASSERT_FALSE(key_manager_toggle_active(&manager, 5));

    key_manager_cleanup(&manager);
}

// Test 13: Get active count
static void test_key_manager_active_count(void) {
    KeyManager manager;
    key_manager_init(&manager);

    TEST_ASSERT_EQUAL_UINT(0, key_manager_get_active_count(&manager));

    key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc");
    key_manager_add_key(&manager, 0x22222222, "Key 2", "/key2.nfc");
    key_manager_add_key(&manager, 0x33333333, "Key 3", "/key3.nfc");

    TEST_ASSERT_EQUAL_UINT(3, key_manager_get_active_count(&manager));

    key_manager_toggle_active(&manager, 1);
    TEST_ASSERT_EQUAL_UINT(2, key_manager_get_active_count(&manager));

    key_manager_toggle_active(&manager, 0);
    TEST_ASSERT_EQUAL_UINT(1, key_manager_get_active_count(&manager));

    key_manager_toggle_active(&manager, 2);
    TEST_ASSERT_EQUAL_UINT(0, key_manager_get_active_count(&manager));

    key_manager_cleanup(&manager);
}

// Test 14: Get key by index
static void test_key_manager_get_key(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x12345678, "Test Key", "/test.nfc");

    KeyInfo* key = key_manager_get_key(&manager, 0);
    TEST_ASSERT_NOT_NULL(key);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, key->uid);
    TEST_ASSERT_EQUAL_STRING("Test Key", key->name);

    TEST_ASSERT_NULL(key_manager_get_key(&manager, 1));
    TEST_ASSERT_NULL(key_manager_get_key(&manager, 5));

    key_manager_cleanup(&manager);
}

// Test 15: Unload all keys
static void test_key_manager_unload_all(void) {
    KeyManager manager;
    key_manager_init(&manager);

    key_manager_add_key(&manager, 0x11111111, "Key 1", "/key1.nfc");
    key_manager_add_key(&manager, 0x22222222, "Key 2", "/key2.nfc");
    key_manager_add_key(&manager, 0x33333333, "Key 3", "/key3.nfc");

    TEST_ASSERT_EQUAL_UINT(3, manager.key_count);

    key_manager_unload_all(&manager);

    TEST_ASSERT_EQUAL_UINT(0, manager.key_count);
    for(int i = 0; i < MAX_KEYS; i++) {
        TEST_ASSERT_NULL(manager.keys[i].name);
        TEST_ASSERT_NULL(manager.keys[i].file_path);
    }

    key_manager_cleanup(&manager);
}

// Test 16: Memory cleanup verification
static void test_key_manager_memory_cleanup(void) {
    KeyManager manager;
    key_manager_init(&manager);

    // Add keys with unique names/paths
    for(int i = 0; i < 3; i++) {
        char name[32], path[64];
        snprintf(name, sizeof(name), "Unique Key %d", i);
        snprintf(path, sizeof(path), "/unique/path/to/key%d.nfc", i);
        key_manager_add_key(&manager, 0x10000000 + i, name, path);
    }

    // Verify pointers are unique (not shared)
    TEST_ASSERT_NOT_NULL(manager.keys[0].name);
    TEST_ASSERT_NOT_NULL(manager.keys[1].name);
    TEST_ASSERT_NOT_NULL(manager.keys[2].name);
    TEST_ASSERT_TRUE(manager.keys[0].name != manager.keys[1].name);
    TEST_ASSERT_TRUE(manager.keys[1].name != manager.keys[2].name);

    key_manager_cleanup(&manager);

    // After cleanup, all should be NULL
    for(int i = 0; i < MAX_KEYS; i++) {
        TEST_ASSERT_NULL(manager.keys[i].name);
        TEST_ASSERT_NULL(manager.keys[i].file_path);
    }
}

// Test 17: Edge case - empty names and paths
static void test_key_manager_empty_strings(void) {
    KeyManager manager;
    key_manager_init(&manager);

    TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x12345678, "", ""));
    TEST_ASSERT_EQUAL_STRING("", manager.keys[0].name);
    TEST_ASSERT_EQUAL_STRING("", manager.keys[0].file_path);

    key_manager_cleanup(&manager);
}

// Test 18: Large strings
static void test_key_manager_large_strings(void) {
    KeyManager manager;
    key_manager_init(&manager);

    char large_name[256];
    char large_path[512];

    memset(large_name, 'A', sizeof(large_name) - 1);
    large_name[sizeof(large_name) - 1] = '\0';

    memset(large_path, 'B', sizeof(large_path) - 1);
    large_path[sizeof(large_path) - 1] = '\0';

    TEST_ASSERT_TRUE(key_manager_add_key(&manager, 0x12345678, large_name, large_path));

    TEST_ASSERT_EQUAL_STRING(large_name, manager.keys[0].name);
    TEST_ASSERT_EQUAL_STRING(large_path, manager.keys[0].file_path);

    key_manager_cleanup(&manager);
}

// Test runner
void test_key_management_run_all(void) {
    printf("\n");
    printf("🔑 Key Management Unit Tests\n");
    printf("============================\n");

    // Ensure tick starts at a positive value
    extern void furi_mock_reset_tick(void);
    furi_mock_reset_tick();

    unity_begin();

    RUN_TEST(test_key_manager_init);
    RUN_TEST(test_key_manager_add_single_key);
    RUN_TEST(test_key_manager_add_multiple_keys);
    RUN_TEST(test_key_manager_add_duplicate_key);
    RUN_TEST(test_key_manager_add_max_keys);
    RUN_TEST(test_key_manager_remove_first_key);
    RUN_TEST(test_key_manager_remove_middle_key);
    RUN_TEST(test_key_manager_remove_last_key);
    RUN_TEST(test_key_manager_remove_invalid_index);
    RUN_TEST(test_key_manager_find_by_uid);
    RUN_TEST(test_key_manager_toggle_active);
    RUN_TEST(test_key_manager_toggle_invalid_index);
    RUN_TEST(test_key_manager_active_count);
    RUN_TEST(test_key_manager_get_key);
    RUN_TEST(test_key_manager_unload_all);
    RUN_TEST(test_key_manager_memory_cleanup);
    RUN_TEST(test_key_manager_empty_strings);
    RUN_TEST(test_key_manager_large_strings);

    unity_end();
}

int main(void) {
    test_key_management_run_all();
    return 0;
}
