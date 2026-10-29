#pragma once

#include <storage/storage.h>
#include <stddef.h>

#include "../../tea_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Прочитать и разобрать файл пресета (слой Storage над tea_preset_parse_buf).
 *
 * @param storage  открытый Storage
 * @param path     путь к файлу `.tea`
 * @param buf      буфер разбора размером не меньше TEA_PARSE_BUF_SIZE (в куче)
 * @param buf_size размер буфера
 * @param out      результат; поле `path` заполняется путём файла
 * @param err      описание ошибки (может быть NULL)
 * @return TeaPresetResultOK при успехе, иначе код ошибки
 */
TeaPresetResult tea_preset_load(
    Storage* storage,
    const char* path,
    char* buf,
    size_t buf_size,
    TeaPreset* out,
    PresetError* err);

#ifdef __cplusplus
}
#endif
