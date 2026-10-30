#include "tea_preset.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/** Флаги найденных обязательных ключей. */
#define TEA_FIELD_NAME (1U << 0)
#define TEA_FIELD_LEAF (1U << 1)
#define TEA_FIELD_FIRST (1U << 2)
#define TEA_FIELD_INCREMENT (1U << 3)
#define TEA_FIELD_MAX_POURS (1U << 4)
#define TEA_FIELD_WATER_MIN (1U << 5)
#define TEA_FIELD_WATER_MAX (1U << 6)
#define TEA_FIELD_ALL (TEA_FIELD_NAME | TEA_FIELD_LEAF | TEA_FIELD_FIRST | TEA_FIELD_INCREMENT | TEA_FIELD_MAX_POURS)
/** Пара ключей температуры: обязательна только целиком (003, FR-005). */
#define TEA_FIELD_WATER_ALL (TEA_FIELD_WATER_MIN | TEA_FIELD_WATER_MAX)

static bool tea_char_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static bool tea_char_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool tea_char_is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || tea_char_is_digit(c) ||
           c == ' ' || c == '_';
}

/** Скопировать фрагмент в буфер с терминатором и ограничением длины. */
static void tea_copy_limited(char* dst, size_t dst_size, const char* src, size_t src_len) {
    if(dst == NULL || dst_size == 0) {
        return;
    }
    size_t n = src_len;
    if(n > dst_size - 1) {
        n = dst_size - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/** Сравнить фрагмент ключа с известным именем поля. */
static bool tea_key_eq(const char* key, size_t key_len, const char* name) {
    size_t name_len = strlen(name);
    return key_len == name_len && memcmp(key, name, name_len) == 0;
}

/** Разобрать неотрицательное целое: только цифры, всё значение израсходовано. */
static bool tea_parse_uint(const char* s, size_t n, uint32_t* out) {
    char tmp[16];
    if(n == 0 || n >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    if(!tea_char_is_digit(tmp[0])) {
        return false;
    }
    char* end = NULL;
    unsigned long value = strtoul(tmp, &end, 10);
    if(end != tmp + n) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

/** Разобрать граммовку `N.N` в десятые доли (ровно одна цифра после точки). */
static bool tea_parse_leaf(const char* s, size_t n, uint16_t* tenths) {
    size_t dot = n;
    for(size_t i = 0; i < n; i++) {
        if(s[i] == '.') {
            if(dot != n) {
                return false;
            }
            dot = i;
        }
    }
    // Точка обязательна, перед ней хотя бы одна цифра, после — ровно одна.
    if(dot == n || dot == 0 || dot + 2 != n) {
        return false;
    }
    for(size_t i = 0; i < dot; i++) {
        if(!tea_char_is_digit(s[i])) {
            return false;
        }
    }
    if(!tea_char_is_digit(s[dot + 1])) {
        return false;
    }
    if(dot > 3) {
        return false;
    }
    uint32_t integer_part = 0;
    for(size_t i = 0; i < dot; i++) {
        integer_part = integer_part * 10U + (uint32_t)(s[i] - '0');
    }
    uint32_t value = integer_part * 10U + (uint32_t)(s[dot + 1] - '0');
    if(value > 0xFFFFU) {
        return false;
    }
    *tenths = (uint16_t)value;
    return true;
}

/** Вернуть имя первого отсутствующего обязательного ключа. */
static const char* tea_first_missing_field(uint32_t seen) {
    if(!(seen & TEA_FIELD_NAME)) {
        return "name";
    }
    if(!(seen & TEA_FIELD_LEAF)) {
        return "leaf_g_per_100ml";
    }
    if(!(seen & TEA_FIELD_FIRST)) {
        return "first_pour_sec";
    }
    if(!(seen & TEA_FIELD_INCREMENT)) {
        return "increment_sec";
    }
    return "max_pours";
}

TeaPresetResult tea_preset_parse_buf(
    const char* buf,
    size_t len,
    TeaPreset* out,
    char* error_field,
    size_t error_field_size) {
    if(error_field != NULL && error_field_size > 0) {
        error_field[0] = '\0';
    }
    if(buf == NULL || out == NULL) {
        return TeaPresetResultErrBadNumber;
    }
    memset(out, 0, sizeof(TeaPreset));
    // Температура не обязательна: файлы 001/002 получают умолчание 85–90 (FR-006).
    out->water_temp_min_c = TEA_WATER_TEMP_DEFAULT_MIN;
    out->water_temp_max_c = TEA_WATER_TEMP_DEFAULT_MAX;

    uint32_t seen = 0;
    size_t pos = 0;

    // Снять UTF-8 BOM, если он есть.
    if(len >= 3 && (uint8_t)buf[0] == 0xEF && (uint8_t)buf[1] == 0xBB && (uint8_t)buf[2] == 0xBF) {
        pos = 3;
    }

    while(pos < len) {
        size_t line_end = pos;
        while(line_end < len && buf[line_end] != '\n') {
            line_end++;
        }

        size_t start = pos;
        size_t end = line_end;
        while(start < end && tea_char_is_space(buf[start])) {
            start++;
        }
        while(end > start && tea_char_is_space(buf[end - 1])) {
            end--;
        }

        pos = line_end + 1;

        if(start == end || buf[start] == '#') {
            continue;
        }

        size_t colon = start;
        while(colon < end && buf[colon] != ':') {
            colon++;
        }
        if(colon == end) {
            // Строка без разделителя `:` не является полем — игнорируем.
            continue;
        }

        size_t key_start = start;
        size_t key_end = colon;
        while(key_start < key_end && tea_char_is_space(buf[key_start])) {
            key_start++;
        }
        while(key_end > key_start && tea_char_is_space(buf[key_end - 1])) {
            key_end--;
        }

        size_t value_start = colon + 1;
        size_t value_end = end;
        while(value_start < value_end && tea_char_is_space(buf[value_start])) {
            value_start++;
        }
        while(value_end > value_start && tea_char_is_space(buf[value_end - 1])) {
            value_end--;
        }

        const char* key = buf + key_start;
        size_t key_len = key_end - key_start;
        const char* value = buf + value_start;
        size_t value_len = value_end - value_start;

        uint32_t field_mask = 0;
        if(tea_key_eq(key, key_len, "name")) {
            field_mask = TEA_FIELD_NAME;
        } else if(tea_key_eq(key, key_len, "leaf_g_per_100ml")) {
            field_mask = TEA_FIELD_LEAF;
        } else if(tea_key_eq(key, key_len, "first_pour_sec")) {
            field_mask = TEA_FIELD_FIRST;
        } else if(tea_key_eq(key, key_len, "increment_sec")) {
            field_mask = TEA_FIELD_INCREMENT;
        } else if(tea_key_eq(key, key_len, "max_pours")) {
            field_mask = TEA_FIELD_MAX_POURS;
        } else if(tea_key_eq(key, key_len, "water_temp_min_c")) {
            field_mask = TEA_FIELD_WATER_MIN;
        } else if(tea_key_eq(key, key_len, "water_temp_max_c")) {
            field_mask = TEA_FIELD_WATER_MAX;
        } else {
            // Неизвестный ключ — совместимое расширение, игнорируем.
            continue;
        }

        if(seen & field_mask) {
            tea_copy_limited(error_field, error_field_size, key, key_len);
            return TeaPresetResultErrDuplicateField;
        }
        seen |= field_mask;

        if(field_mask == TEA_FIELD_NAME) {
            if(value_len == 0) {
                tea_copy_limited(error_field, error_field_size, "name", 4);
                return TeaPresetResultErrEmptyName;
            }
            if(value_len > TEA_NAME_MAX) {
                tea_copy_limited(error_field, error_field_size, "name", 4);
                return TeaPresetResultErrOutOfRange;
            }
            for(size_t i = 0; i < value_len; i++) {
                if(!tea_char_is_name_char(value[i])) {
                    tea_copy_limited(error_field, error_field_size, "name", 4);
                    return TeaPresetResultErrBadName;
                }
            }
            tea_copy_limited(out->name, sizeof(out->name), value, value_len);
            continue;
        }

        uint32_t number = 0;
        if(field_mask == TEA_FIELD_LEAF) {
            uint16_t tenths = 0;
            if(!tea_parse_leaf(value, value_len, &tenths)) {
                tea_copy_limited(error_field, error_field_size, key, key_len);
                return TeaPresetResultErrBadNumber;
            }
            if(tenths < 1 || tenths > 500) {
                tea_copy_limited(error_field, error_field_size, key, key_len);
                return TeaPresetResultErrOutOfRange;
            }
            out->leaf_tenths_g = tenths;
            continue;
        }

        if(!tea_parse_uint(value, value_len, &number)) {
            tea_copy_limited(error_field, error_field_size, key, key_len);
            return TeaPresetResultErrBadNumber;
        }

        if(field_mask == TEA_FIELD_WATER_MIN || field_mask == TEA_FIELD_WATER_MAX) {
            if(number < TEA_WATER_TEMP_MIN || number > TEA_WATER_TEMP_MAX) {
                tea_copy_limited(error_field, error_field_size, key, key_len);
                return TeaPresetResultErrOutOfRange;
            }
            if(field_mask == TEA_FIELD_WATER_MIN) {
                out->water_temp_min_c = (uint8_t)number;
            } else {
                out->water_temp_max_c = (uint8_t)number;
            }
            continue;
        }

        if(field_mask == TEA_FIELD_FIRST) {
            if(number < 1 || number > 3600) {
                tea_copy_limited(error_field, error_field_size, key, key_len);
                return TeaPresetResultErrOutOfRange;
            }
            out->first_pour_sec = (uint16_t)number;
        } else if(field_mask == TEA_FIELD_INCREMENT) {
            if(number > 600) {
                tea_copy_limited(error_field, error_field_size, key, key_len);
                return TeaPresetResultErrOutOfRange;
            }
            out->increment_sec = (uint16_t)number;
        } else if(field_mask == TEA_FIELD_MAX_POURS) {
            if(number < 1 || number > 99) {
                tea_copy_limited(error_field, error_field_size, key, key_len);
                return TeaPresetResultErrOutOfRange;
            }
            out->max_pours = (uint8_t)number;
        }
    }

    // Пара температуры: неполная пара — ошибка с именем отсутствующего ключа (FR-005).
    uint32_t water_seen = seen & TEA_FIELD_WATER_ALL;
    if(water_seen == TEA_FIELD_WATER_MIN) {
        tea_copy_limited(error_field, error_field_size, "water_temp_max_c", 16);
        return TeaPresetResultErrNoField;
    }
    if(water_seen == TEA_FIELD_WATER_MAX) {
        tea_copy_limited(error_field, error_field_size, "water_temp_min_c", 16);
        return TeaPresetResultErrNoField;
    }
    // Минимум больше максимума — ошибка диапазона на поле максимума (FR-003).
    if(water_seen == TEA_FIELD_WATER_ALL && out->water_temp_min_c > out->water_temp_max_c) {
        tea_copy_limited(error_field, error_field_size, "water_temp_max_c", 16);
        return TeaPresetResultErrOutOfRange;
    }

    if((seen & TEA_FIELD_ALL) != TEA_FIELD_ALL) {
        const char* missing = tea_first_missing_field(seen);
        tea_copy_limited(error_field, error_field_size, missing, strlen(missing));
        return TeaPresetResultErrNoField;
    }

    return TeaPresetResultOK;
}
