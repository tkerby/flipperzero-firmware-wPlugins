#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <storage/storage.h>

#include "../../tea_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Умолчания формы создания пресета: 5.0 г / 85–90 °C / 20 с / 5 с / 6 проливов. */
#define TEA_DEFAULT_LEAF_TENTHS      50
#define TEA_DEFAULT_WATER_TEMP_MIN   TEA_WATER_TEMP_DEFAULT_MIN
#define TEA_DEFAULT_WATER_TEMP_MAX   TEA_WATER_TEMP_DEFAULT_MAX
#define TEA_DEFAULT_FIRST_POUR_SEC   20
#define TEA_DEFAULT_INCREMENT_SEC    5
#define TEA_DEFAULT_MAX_POURS        6

/**
 * Построить основу имени файла (`slug`) из названия пресета.
 *
 * Нижний регистр, пробелы → `_`, остаются `[a-z0-9_]`, крайние пробелы отбрасываются,
 * пустой результат → `preset`, длина ≤ TEA_SLUG_MAX.
 *
 * @return длина результата без терминатора
 */
size_t tea_preset_build_slug(const char* name, char* out, size_t size);

/**
 * Построить путь к файлу пресета.
 *
 * @param base  каталог без завершающего слэша
 * @param slug  основа имени
 * @param index 0 → `<slug>.tea`, иначе `<slug>_<index>.tea`
 * @param out   буфер результата
 * @param size  размер буфера
 * @return длина результата без терминатора (0 — результат не поместился)
 */
size_t
    tea_preset_make_path(const char* base, const char* slug, uint8_t index, char* out, size_t size);

/**
 * Сериализовать пресет в формат 003 (семь ключей, LF после каждой строки, без BOM).
 *
 * Порядок ключей: `name`, `leaf_g_per_100ml`, `water_temp_min_c`, `water_temp_max_c`,
 * `first_pour_sec`, `increment_sec`, `max_pours` (contracts/tea-file-format.md §6).
 *
 * @param out_len длина результата без терминатора (может быть NULL)
 * @return TeaPresetResultOK либо TeaPresetResultErrTooLarge
 */
TeaPresetResult
    tea_preset_serialize(const TeaPreset* preset, char* buf, size_t size, size_t* out_len);

/**
 * Проверить черновик без выдачи имени проблемного поля.
 *
 * @return TeaPresetResultOK, ErrEmptyName, ErrBadName или ErrOutOfRange
 */
TeaPresetResult tea_preset_check_draft(const TeaPreset* draft);

/**
 * Проверить черновик и записать имя проблемного ключа (для текста диалога).
 *
 * @param error_field      буфер для имени поля (может быть NULL)
 * @param error_field_size размер буфера
 * @return TeaPresetResultOK, ErrEmptyName, ErrBadName или ErrOutOfRange
 */
TeaPresetResult
    tea_preset_check_draft_ex(const TeaPreset* draft, char* error_field, size_t error_field_size);

/**
 * Записать пресет в файл через временный файл с обратным чтением.
 *
 * Каталог берётся из `draft->path` (каталог исходного файла при правке) либо из
 * `STORAGE_APP_DATA_PATH_PREFIX` при создании; при отсутствии каталог создаётся.
 *
 * @param editing      true — писать в `draft->path`; false — подобрать свободное имя
 * @param out_path     буфер выбранного пути (может быть NULL)
 * @param out_path_size размер буфера out_path
 */
PresetSaveResult tea_preset_save(
    Storage* storage,
    TeaPreset* draft,
    bool editing,
    char* out_path,
    size_t out_path_size);

/** Удалить файл пресета с SD-карты. */
PresetDeleteResult tea_preset_delete(Storage* storage, const char* path);

#ifdef __cplusplus
}
#endif
