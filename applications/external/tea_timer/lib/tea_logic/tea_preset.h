#pragma once

#include <stddef.h>

#include "../../tea_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Разобрать содержимое файла пресета (чистый слой, без Storage).
 *
 * @param buf             буфер с содержимым файла
 * @param len             длина содержимого в байтах (без терминатора)
 * @param out             результат разбора; поле `path` не заполняется
 * @param error_field     буфер для имени проблемного ключа (может быть NULL)
 * @param error_field_size размер буфера error_field
 * @return TeaPresetResultOK при успехе, иначе код ошибки
 */
TeaPresetResult tea_preset_parse_buf(
    const char* buf,
    size_t len,
    TeaPreset* out,
    char* error_field,
    size_t error_field_size);

#ifdef __cplusplus
}
#endif
