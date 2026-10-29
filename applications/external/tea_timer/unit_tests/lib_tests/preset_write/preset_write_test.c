#include "../../minunit.h"

#include <furi.h>
#include <storage/storage.h>

#include "../../../tea_timer.h"
#include "../../../lib/tea_logic/brew_math.h"
#include "../../../lib/tea_logic/tea_preset.h"
#include "../../../lib/tea_logic/tea_preset_load.h"
#include "../../../lib/tea_logic/tea_preset_save.h"

/** Каталог временных файлов набора (создаётся и удаляется самим набором). */
#define TEST_DIR              APP_DATA_PATH("test")
#define TEST_FILE             APP_DATA_PATH("test/test_tea.tea")
#define TEST_FILE_SUFFIX      APP_DATA_PATH("test/test_tea_2.tea")
/** Файлы правки: имя строится из названия `Edit Me` (FR-012). */
#define TEST_FILE_EDIT        APP_DATA_PATH("test/edit_me.tea")
#define TEST_FILE_EDIT_SUFFIX APP_DATA_PATH("test/edit_me_2.tea")

/** Файл старого формата 001/002 без ключей температуры (US3). */
#define TEST_FILE_LEGACY APP_DATA_PATH("test/legacy_tea.tea")

/** Файл старого формата: пары температуры нет, остальное — эталон. */
static const char* const test_legacy_text = "name: Legacy Tea\n"
                                            "leaf_g_per_100ml: 5.0\n"
                                            "first_pour_sec: 20\n"
                                            "increment_sec: 5\n"
                                            "max_pours: 6\n";

/** Буферы статические: стек CLI-потока теста ограничен (2048 байт). */
static char test_write_buffer[TEA_FILE_MAX_SIZE + 1];
static char test_read_buffer[TEA_FILE_MAX_SIZE + 1];
static char test_slug_buffer[TEA_SLUG_MAX + 1];
static char test_path_buffer[TEA_PATH_MAX];
static TeaPreset test_draft;

/** Эталон: 5.0 / 85 / 90 / 20 / 5 / 6 — умолчания формы создания (FR-005, FR-015). */
static void test_make_default_draft(TeaPreset* draft, const char* name, const char* path) {
    memset(draft, 0, sizeof(TeaPreset));
    snprintf(draft->name, sizeof(draft->name), "%s", name);
    draft->leaf_tenths_g = TEA_DEFAULT_LEAF_TENTHS;
    draft->water_temp_min_c = TEA_DEFAULT_WATER_TEMP_MIN;
    draft->water_temp_max_c = TEA_DEFAULT_WATER_TEMP_MAX;
    draft->first_pour_sec = TEA_DEFAULT_FIRST_POUR_SEC;
    draft->increment_sec = TEA_DEFAULT_INCREMENT_SEC;
    draft->max_pours = TEA_DEFAULT_MAX_POURS;
    if(path != NULL) {
        snprintf(draft->path, sizeof(draft->path), "%s", path);
    }
}

/** Записать файл целиком через Storage. */
static bool test_write_file(Storage* storage, const char* path, const char* data, size_t size) {
    File* file = storage_file_alloc(storage);
    bool written = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(written) {
        written = storage_file_write(file, data, size) == size;
    }
    storage_file_close(file);
    storage_file_free(file);
    return written;
}

/** Прочитать файл целиком через Storage; 0 — файл не прочитан. */
static size_t test_read_file(Storage* storage, const char* path, char* buf, size_t size) {
    File* file = storage_file_alloc(storage);
    size_t total = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        total = storage_file_read(file, buf, size - 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    buf[total] = '\0';
    return total;
}

MU_TEST(test_build_slug) {
    tea_preset_build_slug("Green Tea", test_slug_buffer, sizeof(test_slug_buffer));
    mu_assert_string_eq("green_tea", test_slug_buffer);

    tea_preset_build_slug("  A  B  ", test_slug_buffer, sizeof(test_slug_buffer));
    mu_assert_string_eq("a__b", test_slug_buffer);

    tea_preset_build_slug("", test_slug_buffer, sizeof(test_slug_buffer));
    mu_assert_string_eq("preset", test_slug_buffer);

    tea_preset_build_slug("!!!", test_slug_buffer, sizeof(test_slug_buffer));
    mu_assert_string_eq("preset", test_slug_buffer);

    tea_preset_build_slug("Oolong-2", test_slug_buffer, sizeof(test_slug_buffer));
    mu_assert_string_eq("oolong2", test_slug_buffer);

    size_t len = tea_preset_build_slug(
        "abcdefghijklmnopqrstuvwxyz", test_slug_buffer, sizeof(test_slug_buffer));
    mu_assert_int_eq(TEA_SLUG_MAX, len);
    mu_assert_int_eq(TEA_SLUG_MAX, strlen(test_slug_buffer));
    mu_assert_string_eq("abcdefghijklmnopqrstuvwx", test_slug_buffer);
}

MU_TEST(test_make_path) {
    tea_preset_make_path("/data", "green_tea", 0, test_path_buffer, sizeof(test_path_buffer));
    mu_assert_string_eq("/data/green_tea.tea", test_path_buffer);

    tea_preset_make_path("/data", "green_tea", 2, test_path_buffer, sizeof(test_path_buffer));
    mu_assert_string_eq("/data/green_tea_2.tea", test_path_buffer);

    mu_assert_int_eq(0, tea_preset_make_path("/data", "green_tea", 0, test_path_buffer, 4));
}

MU_TEST(test_serialize_reference) {
    test_make_default_draft(&test_draft, "Test Tea", NULL);

    size_t len = 0;
    TeaPresetResult result =
        tea_preset_serialize(&test_draft, test_write_buffer, sizeof(test_write_buffer), &len);
    mu_assert_int_eq(TeaPresetResultOK, result);
    static const char* const expected = "name: Test Tea\n"
                                        "leaf_g_per_100ml: 5.0\n"
                                        "water_temp_min_c: 85\n"
                                        "water_temp_max_c: 90\n"
                                        "first_pour_sec: 20\n"
                                        "increment_sec: 5\n"
                                        "max_pours: 6\n";
    mu_assert("serialized text mismatch", strcmp(expected, test_write_buffer) == 0);
    mu_assert_int_eq(strlen(test_write_buffer), len);
    mu_assert("length must fit file limit", len <= TEA_FILE_MAX_SIZE);
    mu_assert("last byte must be LF", test_write_buffer[len - 1] == '\n');
}

MU_TEST(test_serialize_round_trip) {
    test_make_default_draft(&test_draft, "Green Tea", NULL);
    test_draft.leaf_tenths_g = 123;
    test_draft.first_pour_sec = 3600;
    test_draft.increment_sec = 600;
    test_draft.max_pours = 99;

    size_t len = 0;
    mu_assert_int_eq(
        TeaPresetResultOK,
        tea_preset_serialize(&test_draft, test_write_buffer, sizeof(test_write_buffer), &len));

    TeaPreset parsed;
    memset(&parsed, 0, sizeof(parsed));
    mu_assert_int_eq(
        TeaPresetResultOK, tea_preset_parse_buf(test_write_buffer, len, &parsed, NULL, 0));
    mu_assert_string_eq("Green Tea", parsed.name);
    mu_assert_int_eq(123, parsed.leaf_tenths_g);
    mu_assert_int_eq(3600, parsed.first_pour_sec);
    mu_assert_int_eq(600, parsed.increment_sec);
    mu_assert_int_eq(99, parsed.max_pours);
}

MU_TEST(test_serialize_water_keys) {
    // FR-004, FR-008, SC-007: семь ключей в порядке контракта и обратное чтение.
    test_make_default_draft(&test_draft, "Green Tea", NULL);
    test_draft.water_temp_min_c = 85;
    test_draft.water_temp_max_c = 87;

    size_t len = 0;
    mu_assert_int_eq(
        TeaPresetResultOK,
        tea_preset_serialize(&test_draft, test_write_buffer, sizeof(test_write_buffer), &len));

    static const char* const expected = "name: Green Tea\n"
                                        "leaf_g_per_100ml: 5.0\n"
                                        "water_temp_min_c: 85\n"
                                        "water_temp_max_c: 87\n"
                                        "first_pour_sec: 20\n"
                                        "increment_sec: 5\n"
                                        "max_pours: 6\n";
    mu_assert("serialized text mismatch", strcmp(expected, test_write_buffer) == 0);
    mu_assert_int_eq(strlen(test_write_buffer), len);
    mu_assert("last byte must be LF", test_write_buffer[len - 1] == '\n');

    unsigned lines = 0;
    for(size_t i = 0; i < len; i++) {
        if(test_write_buffer[i] == '\n') {
            lines++;
        }
    }
    mu_assert_int_eq(7, lines);

    TeaPreset parsed;
    memset(&parsed, 0, sizeof(parsed));
    mu_assert_int_eq(
        TeaPresetResultOK, tea_preset_parse_buf(test_write_buffer, len, &parsed, NULL, 0));
    mu_assert_string_eq("Green Tea", parsed.name);
    mu_assert_int_eq(50, parsed.leaf_tenths_g);
    mu_assert_int_eq(85, parsed.water_temp_min_c);
    mu_assert_int_eq(87, parsed.water_temp_max_c);
    mu_assert_int_eq(20, parsed.first_pour_sec);
    mu_assert_int_eq(5, parsed.increment_sec);
    mu_assert_int_eq(6, parsed.max_pours);
}

MU_TEST(test_check_draft_water_temp) {
    // FR-002, FR-003, FR-017: диапазон 85–90 и порядок «минимум ≤ максимум».
    char field[24];

    test_make_default_draft(&test_draft, "Test Tea", NULL);
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));

    test_draft.water_temp_min_c = 87;
    test_draft.water_temp_max_c = 87;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));

    test_draft.water_temp_min_c = 84;
    test_draft.water_temp_max_c = 90;
    mu_assert_int_eq(
        TeaPresetResultErrOutOfRange,
        tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("water_temp_min_c", field);

    test_draft.water_temp_min_c = 85;
    test_draft.water_temp_max_c = 91;
    mu_assert_int_eq(
        TeaPresetResultErrOutOfRange,
        tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("water_temp_max_c", field);

    test_draft.water_temp_min_c = 88;
    test_draft.water_temp_max_c = 86;
    mu_assert_int_eq(
        TeaPresetResultErrOutOfRange,
        tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("water_temp_max_c", field);
}

MU_TEST(test_check_draft_names) {
    char field[24];

    test_make_default_draft(&test_draft, "", NULL);
    mu_assert_int_eq(TeaPresetResultErrEmptyName, tea_preset_check_draft(&test_draft));
    mu_assert_int_eq(
        TeaPresetResultErrEmptyName, tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("name", field);

    test_make_default_draft(&test_draft, "Green-Tea", NULL);
    mu_assert_int_eq(TeaPresetResultErrBadName, tea_preset_check_draft(&test_draft));

    test_make_default_draft(&test_draft, "Green Tea_2", NULL);
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
}

MU_TEST(test_check_draft_ranges) {
    char field[24];

    test_make_default_draft(&test_draft, "Test Tea", NULL);
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));

    test_draft.leaf_tenths_g = 1;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.leaf_tenths_g = 500;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.leaf_tenths_g = 0;
    mu_assert_int_eq(
        TeaPresetResultErrOutOfRange,
        tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("leaf_g_per_100ml", field);
    test_draft.leaf_tenths_g = 501;
    mu_assert_int_eq(TeaPresetResultErrOutOfRange, tea_preset_check_draft(&test_draft));

    test_make_default_draft(&test_draft, "Test Tea", NULL);
    test_draft.first_pour_sec = 1;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.first_pour_sec = 3600;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.first_pour_sec = 0;
    mu_assert_int_eq(
        TeaPresetResultErrOutOfRange,
        tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("first_pour_sec", field);
    test_draft.first_pour_sec = 3601;
    mu_assert_int_eq(TeaPresetResultErrOutOfRange, tea_preset_check_draft(&test_draft));

    test_make_default_draft(&test_draft, "Test Tea", NULL);
    test_draft.increment_sec = 0;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.increment_sec = 600;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.increment_sec = 601;
    mu_assert_int_eq(
        TeaPresetResultErrOutOfRange,
        tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("increment_sec", field);

    test_make_default_draft(&test_draft, "Test Tea", NULL);
    test_draft.max_pours = 1;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.max_pours = 99;
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));
    test_draft.max_pours = 0;
    mu_assert_int_eq(
        TeaPresetResultErrOutOfRange,
        tea_preset_check_draft_ex(&test_draft, field, sizeof(field)));
    mu_assert_string_eq("max_pours", field);
}

MU_TEST(test_defaults_round_trip) {
    // FR-005, FR-011: умолчания формы дают корректный черновик и обратимы.
    test_make_default_draft(&test_draft, "Test Tea", NULL);
    mu_assert_int_eq(TeaPresetResultOK, tea_preset_check_draft(&test_draft));

    size_t len = 0;
    mu_assert_int_eq(
        TeaPresetResultOK,
        tea_preset_serialize(&test_draft, test_write_buffer, sizeof(test_write_buffer), &len));

    TeaPreset parsed;
    memset(&parsed, 0, sizeof(parsed));
    mu_assert_int_eq(
        TeaPresetResultOK, tea_preset_parse_buf(test_write_buffer, len, &parsed, NULL, 0));
    mu_assert_int_eq(50, parsed.leaf_tenths_g);
    mu_assert_int_eq(20, parsed.first_pour_sec);
    mu_assert_int_eq(5, parsed.increment_sec);
    mu_assert_int_eq(6, parsed.max_pours);
}

MU_TEST(test_save_create_and_suffix) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Создание: свободное имя `test_tea.tea`.
    test_make_default_draft(&test_draft, "Test Tea", TEST_FILE);
    PresetSaveResult save =
        tea_preset_save(storage, &test_draft, false, test_path_buffer, sizeof(test_path_buffer));
    bool created = storage_file_exists(storage, TEST_FILE);
    bool path_is_first = strcmp(test_draft.path, TEST_FILE) == 0;

    // Повторное создание с тем же именем: суффикс `_2`, первый файл не меняется.
    static TeaPreset second;
    test_make_default_draft(&second, "Test Tea", TEST_FILE);
    second.first_pour_sec = 30;
    PresetSaveResult save_second =
        tea_preset_save(storage, &second, false, test_path_buffer, sizeof(test_path_buffer));
    bool created_second = storage_file_exists(storage, TEST_FILE_SUFFIX);
    bool path_is_suffix = strcmp(second.path, TEST_FILE_SUFFIX) == 0;

    static TeaPreset loaded_first;
    memset(&loaded_first, 0, sizeof(loaded_first));
    TeaPresetResult load_first = tea_preset_load(
        storage, TEST_FILE, test_write_buffer, TEA_PARSE_BUF_SIZE, &loaded_first, NULL);

    // Правка того же пути: имя файла не меняется, содержимое обновляется.
    second.first_pour_sec = 45;
    PresetSaveResult save_edit =
        tea_preset_save(storage, &second, true, test_path_buffer, sizeof(test_path_buffer));
    static TeaPreset edited;
    memset(&edited, 0, sizeof(edited));
    TeaPresetResult load_edited = tea_preset_load(
        storage, TEST_FILE_SUFFIX, test_write_buffer, TEA_PARSE_BUF_SIZE, &edited, NULL);
    bool first_untouched = storage_file_exists(storage, TEST_FILE);
    bool path_kept = strcmp(second.path, TEST_FILE_SUFFIX) == 0 &&
                     strcmp(test_path_buffer, TEST_FILE_SUFFIX) == 0;

    storage_common_remove(storage, TEST_FILE);
    storage_common_remove(storage, TEST_FILE_SUFFIX);
    furi_record_close(RECORD_STORAGE);

    mu_assert_int_eq(PresetSaveOK, save);
    mu_assert("first file must exist", created);
    mu_assert("chosen path must be the first name", path_is_first);
    mu_assert_int_eq(PresetSaveOK, save_second);
    mu_assert("suffix file must exist", created_second);
    mu_assert("second save must use suffix", path_is_suffix);
    mu_assert_int_eq(TeaPresetResultOK, load_first);
    mu_assert_int_eq(20, loaded_first.first_pour_sec);
    mu_assert_int_eq(PresetSaveOK, save_edit);
    mu_assert_int_eq(TeaPresetResultOK, load_edited);
    mu_assert_int_eq(45, edited.first_pour_sec);
    mu_assert("edited file must keep its name", path_kept);
    mu_assert("first file must stay", first_untouched);
}

MU_TEST(test_save_edit_same_path) {
    // FR-012: правка пишет в тот же путь, имя файла не меняется.
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Имя файла при создании строится из названия: `Edit Me` -> `edit_me.tea`.
    storage_common_remove(storage, TEST_FILE_EDIT);
    storage_common_remove(storage, TEST_FILE_EDIT_SUFFIX);

    // Каталог берётся из пути черновика, имя файла — из названия (US1→US3).
    static TeaPreset original;
    test_make_default_draft(&original, "Edit Me", TEST_FILE);
    PresetSaveResult created =
        tea_preset_save(storage, &original, false, test_path_buffer, sizeof(test_path_buffer));
    bool path_is_first = strcmp(original.path, TEST_FILE_EDIT) == 0 &&
                         strcmp(test_path_buffer, TEST_FILE_EDIT) == 0;

    // Правка: меняем одно значение, остальные остаются прежними.
    static TeaPreset edited;
    edited = original;
    edited.leaf_tenths_g = 61;
    PresetSaveResult save_edit =
        tea_preset_save(storage, &edited, true, test_path_buffer, sizeof(test_path_buffer));
    bool path_kept = strcmp(edited.path, TEST_FILE_EDIT) == 0 &&
                     strcmp(test_path_buffer, TEST_FILE_EDIT) == 0;

    static TeaPreset loaded;
    memset(&loaded, 0, sizeof(loaded));
    TeaPresetResult load = tea_preset_load(
        storage, TEST_FILE_EDIT, test_write_buffer, TEA_PARSE_BUF_SIZE, &loaded, NULL);
    bool suffix_absent = !storage_file_exists(storage, TEST_FILE_EDIT_SUFFIX);

    storage_common_remove(storage, TEST_FILE_EDIT);
    storage_common_remove(storage, TEST_FILE_EDIT_SUFFIX);
    furi_record_close(RECORD_STORAGE);

    mu_assert_int_eq(PresetSaveOK, created);
    mu_assert("created file must use the first name", path_is_first);
    mu_assert_int_eq(PresetSaveOK, save_edit);
    mu_assert("edit must keep the file name", path_kept);
    mu_assert("edit must not create a copy", suffix_absent);
    mu_assert_int_eq(TeaPresetResultOK, load);
    mu_assert_int_eq(61, loaded.leaf_tenths_g);
    mu_assert_int_eq(TEA_DEFAULT_FIRST_POUR_SEC, loaded.first_pour_sec);
    mu_assert_int_eq(TEA_DEFAULT_INCREMENT_SEC, loaded.increment_sec);
    mu_assert_int_eq(TEA_DEFAULT_MAX_POURS, loaded.max_pours);
    mu_assert_string_eq("Edit Me", loaded.name);
}

MU_TEST(test_save_edit_legacy_adds_water_keys) {
    // FR-019…FR-022: правка файла старого формата дописывает оба ключа,
    // сохраняя имя файла и остальные значения; обратное чтение совпадает.
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_common_remove(storage, TEST_FILE_LEGACY);
    bool written =
        test_write_file(storage, TEST_FILE_LEGACY, test_legacy_text, strlen(test_legacy_text));

    static TeaPreset draft;
    memset(&draft, 0, sizeof(draft));
    TeaPresetResult loaded = tea_preset_load(
        storage, TEST_FILE_LEGACY, test_read_buffer, sizeof(test_read_buffer), &draft, NULL);
    bool legacy_defaults = draft.water_temp_min_c == TEA_WATER_TEMP_DEFAULT_MIN &&
                           draft.water_temp_max_c == TEA_WATER_TEMP_DEFAULT_MAX;

    // Правим только максимум: минимум остаётся умолчанием 85.
    draft.water_temp_max_c = 88;
    PresetSaveResult save_edit =
        tea_preset_save(storage, &draft, true, test_path_buffer, sizeof(test_path_buffer));
    bool path_kept = strcmp(draft.path, TEST_FILE_LEGACY) == 0 &&
                     strcmp(test_path_buffer, TEST_FILE_LEGACY) == 0;

    size_t text_len =
        test_read_file(storage, TEST_FILE_LEGACY, test_read_buffer, sizeof(test_read_buffer));
    bool has_min = strstr(test_read_buffer, "water_temp_min_c: 85\n") != NULL;
    bool has_max = strstr(test_read_buffer, "water_temp_max_c: 88\n") != NULL;

    static TeaPreset verify;
    memset(&verify, 0, sizeof(verify));
    TeaPresetResult load_again =
        tea_preset_parse_buf(test_read_buffer, text_len, &verify, NULL, 0);

    storage_common_remove(storage, TEST_FILE_LEGACY);
    furi_record_close(RECORD_STORAGE);

    mu_assert("legacy file must be written", written);
    mu_assert_int_eq(TeaPresetResultOK, loaded);
    mu_assert("legacy file must get defaults 85/90", legacy_defaults);
    mu_assert_int_eq(PresetSaveOK, save_edit);
    mu_assert("edit must keep the file name", path_kept);
    mu_assert("file must contain water_temp_min_c", has_min);
    mu_assert("file must contain water_temp_max_c", has_max);
    mu_assert_int_eq(TeaPresetResultOK, load_again);
    mu_assert_int_eq(85, verify.water_temp_min_c);
    mu_assert_int_eq(88, verify.water_temp_max_c);
    mu_assert_string_eq("Legacy Tea", verify.name);
    mu_assert_int_eq(TEA_DEFAULT_LEAF_TENTHS, verify.leaf_tenths_g);
    mu_assert_int_eq(TEA_DEFAULT_FIRST_POUR_SEC, verify.first_pour_sec);
    mu_assert_int_eq(TEA_DEFAULT_INCREMENT_SEC, verify.increment_sec);
    mu_assert_int_eq(TEA_DEFAULT_MAX_POURS, verify.max_pours);
}

MU_TEST(test_delete_active_preset_clears_session) {
    // FR-015/FR-017/FR-018: удаление активного пресета сбрасывает сессию.
    Storage* storage = furi_record_open(RECORD_STORAGE);

    static TeaPreset preset;
    test_make_default_draft(&preset, "Active Tea", TEST_FILE);
    PresetSaveResult saved =
        tea_preset_save(storage, &preset, false, test_path_buffer, sizeof(test_path_buffer));

    static BrewSession session;
    brew_session_reset_from_preset(&preset, &session);
    session.completed_pours = 2;
    session.next_pour_sec = 25;
    session.phase = BrewPhaseDONE;

    PresetDeleteResult deleted = tea_preset_delete(storage, session.preset.path);
    bool removed = !storage_file_exists(storage, preset.path);
    if(deleted == PresetDeleteOK && session.has_preset &&
       strcmp(session.preset.path, preset.path) == 0) {
        brew_session_clear(&session);
    }

    furi_record_close(RECORD_STORAGE);

    mu_assert_int_eq(PresetSaveOK, saved);
    mu_assert_int_eq(PresetDeleteOK, deleted);
    mu_assert("file must be removed", removed);
    mu_assert("session must be cleared", !session.has_preset);
    mu_assert_int_eq(0, session.completed_pours);
    mu_assert_int_eq(BrewPhaseIDLE, session.phase);
    mu_assert_int_eq(0, strlen(session.preset.path));
}

MU_TEST(test_save_rejects_bad_draft) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    test_make_default_draft(&test_draft, "", TEST_FILE);
    PresetSaveResult save = tea_preset_save(storage, &test_draft, false, NULL, 0);

    furi_record_close(RECORD_STORAGE);
    mu_assert_int_eq(PresetSaveErrDraft, save);
}

MU_TEST(test_delete) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    test_make_default_draft(&test_draft, "Delete Me", TEST_FILE);
    PresetSaveResult save =
        tea_preset_save(storage, &test_draft, false, test_path_buffer, sizeof(test_path_buffer));

    PresetDeleteResult first = tea_preset_delete(storage, test_draft.path);
    bool exists_after = storage_file_exists(storage, test_draft.path);
    PresetDeleteResult again = tea_preset_delete(storage, test_draft.path);
    PresetDeleteResult missing = tea_preset_delete(storage, TEST_FILE_SUFFIX);

    furi_record_close(RECORD_STORAGE);

    mu_assert_int_eq(PresetSaveOK, save);
    mu_assert_int_eq(PresetDeleteOK, first);
    mu_assert("file must be removed", !exists_after);
    mu_assert_int_eq(PresetDeleteErrNotFound, again);
    mu_assert_int_eq(PresetDeleteErrNotFound, missing);
}

MU_TEST(test_session_clear) {
    BrewSession session;
    test_make_default_draft(&test_draft, "Test Tea", TEST_FILE);
    brew_session_reset_from_preset(&test_draft, &session);
    session.completed_pours = 3;
    session.phase = BrewPhaseRUNNING;

    brew_session_clear(&session);

    mu_assert("has_preset must be false", !session.has_preset);
    mu_assert_int_eq(0, session.completed_pours);
    mu_assert_int_eq(0, session.next_pour_sec);
    mu_assert_int_eq(BrewPhaseIDLE, session.phase);
    mu_assert_int_eq(0, session.deadline_tick);
    mu_assert_int_eq(0, strlen(session.preset.path));
}

static void run_minunit_test_preset_write_suite(void) {
    MU_RUN_TEST(test_build_slug);
    MU_RUN_TEST(test_make_path);
    MU_RUN_TEST(test_serialize_reference);
    MU_RUN_TEST(test_serialize_round_trip);
    MU_RUN_TEST(test_serialize_water_keys);
    MU_RUN_TEST(test_check_draft_water_temp);
    MU_RUN_TEST(test_check_draft_names);
    MU_RUN_TEST(test_check_draft_ranges);
    MU_RUN_TEST(test_defaults_round_trip);
    MU_RUN_TEST(test_save_create_and_suffix);
    MU_RUN_TEST(test_save_edit_same_path);
    MU_RUN_TEST(test_save_edit_legacy_adds_water_keys);
    MU_RUN_TEST(test_delete_active_preset_clears_session);
    MU_RUN_TEST(test_save_rejects_bad_draft);
    MU_RUN_TEST(test_delete);
    MU_RUN_TEST(test_session_clear);
}

int run_minunit_test_preset_write(void) {
    run_minunit_test_preset_write_suite();

    // Убрать временный каталог набора, если он пуст.
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, TEST_DIR);
    furi_record_close(RECORD_STORAGE);

    return minunit_status;
}
