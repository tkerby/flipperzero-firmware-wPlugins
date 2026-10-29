#include "tea_preset_save.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

#include "tea_preset.h"
#include "tea_preset_load.h"

#define TAG "TeaPresetSave"

/** Максимальный индекс суффикса свободного имени (`_2`…`_99`). */
#define TEA_PRESET_INDEX_MAX 99

/** Длина буфера записи/обратного чтения: файл 1 КБ + терминатор. */
#define TEA_SAVE_BUF_SIZE (TEA_FILE_MAX_SIZE + 1)

/** Пути операции записи; выделяются в куче, чтобы не расходовать стек. */
typedef struct {
    char base[TEA_PATH_MAX];
    char target[TEA_PATH_MAX];
    char tmp[TEA_PATH_MAX + sizeof(TEA_TMP_SUFFIX)];
} TeaPresetSavePaths;

static bool tea_char_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static bool tea_char_is_slug_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/** Привести символ к нижнему регистру ASCII (остальные не меняются). */
static char tea_char_lower(char c) {
    if(c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

/** Скопировать строку с ограничением длины. */
static void tea_copy_str(char* dst, size_t dst_size, const char* src) {
    if(dst == NULL || dst_size == 0) {
        return;
    }
    size_t n = (src != NULL) ? strlen(src) : 0;
    if(n > dst_size - 1) {
        n = dst_size - 1;
    }
    if(n > 0 && src != NULL) {
        memcpy(dst, src, n);
    }
    dst[n] = '\0';
}

size_t tea_preset_build_slug(const char* name, char* out, size_t size) {
    if(out == NULL || size == 0) {
        return 0;
    }
    out[0] = '\0';
    if(name == NULL) {
        tea_copy_str(out, size, "preset");
        return strlen(out);
    }

    // Крайние пробелы не участвуют в имени файла.
    size_t start = 0;
    size_t end = strlen(name);
    while(start < end && tea_char_is_space(name[start])) {
        start++;
    }
    while(end > start && tea_char_is_space(name[end - 1])) {
        end--;
    }

    size_t limit = TEA_SLUG_MAX;
    if(limit > size - 1) {
        limit = size - 1;
    }

    size_t len = 0;
    for(size_t i = start; i < end && len < limit; i++) {
        char c = tea_char_lower(name[i]);
        if(tea_char_is_space(c)) {
            out[len++] = '_';
        } else if(tea_char_is_slug_char(c)) {
            out[len++] = c;
        }
        // Прочие символы отбрасываются.
    }
    out[len] = '\0';

    if(len == 0) {
        tea_copy_str(out, size, "preset");
        len = strlen(out);
    }

    return len;
}

size_t
    tea_preset_make_path(const char* base, const char* slug, uint8_t index, char* out, size_t size) {
    if(out == NULL || size == 0) {
        return 0;
    }
    out[0] = '\0';
    if(base == NULL || slug == NULL) {
        return 0;
    }

    int written = 0;
    if(index == 0) {
        written = snprintf(out, size, "%s/%s.tea", base, slug);
    } else {
        written = snprintf(out, size, "%s/%s_%u.tea", base, slug, (unsigned)index);
    }

    if(written < 0 || (size_t)written >= size) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)written;
}

TeaPresetResult
    tea_preset_serialize(const TeaPreset* preset, char* buf, size_t size, size_t* out_len) {
    if(out_len != NULL) {
        *out_len = 0;
    }
    if(preset == NULL || buf == NULL || size == 0) {
        return TeaPresetResultErrTooLarge;
    }

    int written = snprintf(
        buf,
        size,
        "name: %s\n"
        "leaf_g_per_100ml: %u.%u\n"
        "water_temp_min_c: %u\n"
        "water_temp_max_c: %u\n"
        "first_pour_sec: %u\n"
        "increment_sec: %u\n"
        "max_pours: %u\n",
        preset->name,
        (unsigned)(preset->leaf_tenths_g / 10U),
        (unsigned)(preset->leaf_tenths_g % 10U),
        (unsigned)preset->water_temp_min_c,
        (unsigned)preset->water_temp_max_c,
        (unsigned)preset->first_pour_sec,
        (unsigned)preset->increment_sec,
        (unsigned)preset->max_pours);

    if(written < 0 || (size_t)written >= size || (size_t)written > TEA_FILE_MAX_SIZE) {
        buf[0] = '\0';
        return TeaPresetResultErrTooLarge;
    }

    if(out_len != NULL) {
        *out_len = (size_t)written;
    }
    return TeaPresetResultOK;
}

/** Записать имя поля в буфер ошибки (если он есть). */
static void tea_error_field_set(char* error_field, size_t error_field_size, const char* field) {
    if(error_field == NULL || error_field_size == 0) {
        return;
    }
    tea_copy_str(error_field, error_field_size, field);
}

TeaPresetResult
    tea_preset_check_draft_ex(const TeaPreset* draft, char* error_field, size_t error_field_size) {
    tea_error_field_set(error_field, error_field_size, "");
    if(draft == NULL) {
        tea_error_field_set(error_field, error_field_size, "name");
        return TeaPresetResultErrEmptyName;
    }

    size_t name_len = 0;
    while(name_len < sizeof(draft->name) && draft->name[name_len] != '\0') {
        name_len++;
    }
    if(name_len == 0) {
        tea_error_field_set(error_field, error_field_size, "name");
        return TeaPresetResultErrEmptyName;
    }
    if(name_len > TEA_NAME_MAX) {
        tea_error_field_set(error_field, error_field_size, "name");
        return TeaPresetResultErrBadName;
    }
    for(size_t i = 0; i < name_len; i++) {
        char c = draft->name[i];
        bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == ' ' || c == '_';
        if(!allowed) {
            tea_error_field_set(error_field, error_field_size, "name");
            return TeaPresetResultErrBadName;
        }
    }

    if(draft->leaf_tenths_g < 1 || draft->leaf_tenths_g > 500) {
        tea_error_field_set(error_field, error_field_size, "leaf_g_per_100ml");
        return TeaPresetResultErrOutOfRange;
    }
    // FR-002, FR-017: каждое значение температуры — в 85–90.
    if(draft->water_temp_min_c < TEA_WATER_TEMP_MIN || draft->water_temp_min_c > TEA_WATER_TEMP_MAX) {
        tea_error_field_set(error_field, error_field_size, "water_temp_min_c");
        return TeaPresetResultErrOutOfRange;
    }
    if(draft->water_temp_max_c < TEA_WATER_TEMP_MIN || draft->water_temp_max_c > TEA_WATER_TEMP_MAX) {
        tea_error_field_set(error_field, error_field_size, "water_temp_max_c");
        return TeaPresetResultErrOutOfRange;
    }
    // FR-003: минимум не больше максимума (равенство допустимо).
    if(draft->water_temp_min_c > draft->water_temp_max_c) {
        tea_error_field_set(error_field, error_field_size, "water_temp_max_c");
        return TeaPresetResultErrOutOfRange;
    }
    if(draft->first_pour_sec < 1 || draft->first_pour_sec > 3600) {
        tea_error_field_set(error_field, error_field_size, "first_pour_sec");
        return TeaPresetResultErrOutOfRange;
    }
    if(draft->increment_sec > 600) {
        tea_error_field_set(error_field, error_field_size, "increment_sec");
        return TeaPresetResultErrOutOfRange;
    }
    if(draft->max_pours < 1 || draft->max_pours > 99) {
        tea_error_field_set(error_field, error_field_size, "max_pours");
        return TeaPresetResultErrOutOfRange;
    }

    return TeaPresetResultOK;
}

TeaPresetResult tea_preset_check_draft(const TeaPreset* draft) {
    return tea_preset_check_draft_ex(draft, NULL, 0);
}

/** Каталог записи: каталог исходного файла черновика либо каталог данных приложения. */
static void tea_preset_base_dir(const TeaPreset* draft, char* out, size_t size) {
    out[0] = '\0';
    if(draft != NULL && draft->path[0] != '\0') {
        const char* last_slash = strrchr(draft->path, '/');
        if(last_slash != NULL && last_slash != draft->path) {
            size_t len = (size_t)(last_slash - draft->path);
            if(len >= size) {
                len = size - 1;
            }
            memcpy(out, draft->path, len);
            out[len] = '\0';
            return;
        }
    }
    tea_copy_str(out, size, STORAGE_APP_DATA_PATH_PREFIX);
}

/** Подобрать свободный путь записи: `slug.tea`, затем `slug_2.tea`…`slug_99.tea`. */
static PresetSaveResult tea_preset_choose_path(
    Storage* storage,
    const TeaPreset* draft,
    const char* base,
    char* target,
    size_t target_size) {
    char slug[TEA_SLUG_MAX + 1];
    tea_preset_build_slug(draft->name, slug, sizeof(slug));

    if(tea_preset_make_path(base, slug, 0, target, target_size) == 0) {
        return PresetSaveErrNoName;
    }
    if(!storage_file_exists(storage, target)) {
        return PresetSaveOK;
    }

    for(uint8_t index = 2; index <= TEA_PRESET_INDEX_MAX; index++) {
        if(tea_preset_make_path(base, slug, index, target, target_size) == 0) {
            return PresetSaveErrNoName;
        }
        if(!storage_file_exists(storage, target)) {
            FURI_LOG_I(TAG, "name taken, using %s", target);
            return PresetSaveOK;
        }
    }

    return PresetSaveErrNoName;
}

/** Сравнить прочитанный пресет с черновиком (без пути). */
static bool tea_preset_same_values(const TeaPreset* a, const TeaPreset* b) {
    return strcmp(a->name, b->name) == 0 && a->leaf_tenths_g == b->leaf_tenths_g &&
           a->water_temp_min_c == b->water_temp_min_c &&
           a->water_temp_max_c == b->water_temp_max_c &&
           a->first_pour_sec == b->first_pour_sec && a->increment_sec == b->increment_sec &&
           a->max_pours == b->max_pours;
}

/** Записать содержимое во временный файл; закрывает файл на всех путях. */
static PresetSaveResult
    tea_preset_write_tmp(Storage* storage, const char* tmp, const char* data, size_t len) {
    File* file = storage_file_alloc(storage);
    furi_check(file != NULL);

    PresetSaveResult result = PresetSaveErrOpen;
    do {
        if(!storage_file_open(file, tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E(TAG, "open %s failed", tmp);
            break;
        }

        size_t written = storage_file_write(file, data, len);
        if(written != len) {
            FURI_LOG_E(TAG, "write %s: %u of %u", tmp, (unsigned)written, (unsigned)len);
            result = PresetSaveErrWrite;
            break;
        }

        result = PresetSaveOK;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    return result;
}

PresetSaveResult tea_preset_save(
    Storage* storage,
    TeaPreset* draft,
    bool editing,
    char* out_path,
    size_t out_path_size) {
    furi_check(storage != NULL);
    furi_check(draft != NULL);

    if(out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }

    if(tea_preset_check_draft(draft) != TeaPresetResultOK) {
        return PresetSaveErrDraft;
    }
    if(editing && draft->path[0] == '\0') {
        return PresetSaveErrDraft;
    }

    // Пути и буферы — в куче: стек Main-потока ограничен (принцип V).
    TeaPresetSavePaths* paths = malloc(sizeof(TeaPresetSavePaths));
    char* data = malloc(TEA_SAVE_BUF_SIZE);
    char* verify_buf = malloc(TEA_SAVE_BUF_SIZE);
    furi_check(paths != NULL);
    furi_check(data != NULL);
    furi_check(verify_buf != NULL);

    PresetSaveResult result = PresetSaveErrDraft;
    do {
        tea_preset_base_dir(draft, paths->base, sizeof(paths->base));
        if(!storage_dir_exists(storage, paths->base)) {
            FS_Error status = storage_common_mkdir(storage, paths->base);
            if(status != FSE_OK && status != FSE_EXIST) {
                FURI_LOG_E(TAG, "mkdir %s failed: %d", paths->base, (int)status);
                result = PresetSaveErrNoDir;
                break;
            }
        }

        paths->target[0] = '\0';
        if(editing) {
            tea_copy_str(paths->target, sizeof(paths->target), draft->path);
        } else {
            result = tea_preset_choose_path(
                storage, draft, paths->base, paths->target, sizeof(paths->target));
            if(result != PresetSaveOK) {
                break;
            }
        }

        size_t len = 0;
        if(tea_preset_serialize(draft, data, TEA_SAVE_BUF_SIZE, &len) != TeaPresetResultOK) {
            result = PresetSaveErrDraft;
            break;
        }

        snprintf(paths->tmp, sizeof(paths->tmp), "%s%s", paths->target, TEA_TMP_SUFFIX);

        result = tea_preset_write_tmp(storage, paths->tmp, data, len);
        if(result != PresetSaveOK) {
            storage_common_remove(storage, paths->tmp);
            break;
        }

        TeaPreset* verified = malloc(sizeof(TeaPreset));
        furi_check(verified != NULL);
        memset(verified, 0, sizeof(TeaPreset));
        TeaPresetResult load =
            tea_preset_load(storage, paths->tmp, verify_buf, TEA_SAVE_BUF_SIZE, verified, NULL);
        bool same = (load == TeaPresetResultOK) && tea_preset_same_values(verified, draft);
        free(verified);
        if(!same) {
            FURI_LOG_E(TAG, "verify %s failed: %d", paths->tmp, (int)load);
            storage_common_remove(storage, paths->tmp);
            result = PresetSaveErrVerify;
            break;
        }

        // Подмена целевого файла: сначала убрать старый, затем переименовать временный.
        if(storage_file_exists(storage, paths->target)) {
            FS_Error removed = storage_common_remove(storage, paths->target);
            if(removed != FSE_OK) {
                FURI_LOG_E(TAG, "remove %s failed: %d", paths->target, (int)removed);
                storage_common_remove(storage, paths->tmp);
                result = PresetSaveErrReplace;
                break;
            }
        }

        FS_Error renamed = storage_common_rename(storage, paths->tmp, paths->target);
        if(renamed != FSE_OK) {
            FURI_LOG_E(TAG, "rename %s -> %s failed: %d", paths->tmp, paths->target, (int)renamed);
            result = PresetSaveErrReplace;
            break;
        }

        tea_copy_str(draft->path, sizeof(draft->path), paths->target);
        if(out_path != NULL && out_path_size > 0) {
            tea_copy_str(out_path, out_path_size, paths->target);
        }
        result = PresetSaveOK;
    } while(false);

    free(verify_buf);
    free(data);
    free(paths);

    return result;
}

PresetDeleteResult tea_preset_delete(Storage* storage, const char* path) {
    furi_check(storage != NULL);

    if(storage_sd_status(storage) != FSE_OK) {
        FURI_LOG_E(TAG, "sd not ready");
        return PresetDeleteErrNoCard;
    }
    if(path == NULL || path[0] == '\0' || !storage_file_exists(storage, path)) {
        return PresetDeleteErrNotFound;
    }

    FS_Error removed = storage_common_remove(storage, path);
    if(removed != FSE_OK) {
        FURI_LOG_E(TAG, "remove %s failed: %d", path, (int)removed);
        return PresetDeleteErrRemove;
    }
    if(storage_file_exists(storage, path)) {
        FURI_LOG_E(TAG, "remove %s: file still exists", path);
        return PresetDeleteErrRemove;
    }

    return PresetDeleteOK;
}
