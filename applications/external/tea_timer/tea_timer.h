#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Максимальная длина названия пресета в байтах (без терминатора). */
#define TEA_NAME_MAX 24

/** Максимальная длина пути к файлу пресета. */
#define TEA_PATH_MAX 128

/** Размер буфера разбора: 1024 байта файла + терминатор. */
#define TEA_PARSE_BUF_SIZE 1025

/** Максимальный допустимый размер файла пресета. */
#define TEA_FILE_MAX_SIZE 1024

/** Максимальная длина основы имени файла пресета (`slug`). */
#define TEA_SLUG_MAX 24

/** Суффикс временного файла при записи пресета. */
#define TEA_TMP_SUFFIX ".tmp"

/** Нижняя граница рекомендуемой температуры воды, °C. */
#define TEA_WATER_TEMP_MIN 85

/** Верхняя граница рекомендуемой температуры воды, °C. */
#define TEA_WATER_TEMP_MAX 90

/** Умолчание температуры воды (минимум) для нового пресета и файлов старого формата. */
#define TEA_WATER_TEMP_DEFAULT_MIN TEA_WATER_TEMP_MIN

/** Умолчание температуры воды (максимум) для нового пресета и файлов старого формата. */
#define TEA_WATER_TEMP_DEFAULT_MAX TEA_WATER_TEMP_MAX

/** Результат разбора и загрузки файла пресета. */
typedef enum {
    TeaPresetResultOK, /**< Файл корректен */
    TeaPresetResultErrNotFound, /**< Файл не открывается */
    TeaPresetResultErrTooLarge, /**< Файл больше 1 КБ */
    TeaPresetResultErrNoField, /**< Отсутствует обязательное поле */
    TeaPresetResultErrBadNumber, /**< Значение не является числом в нужном формате */
    TeaPresetResultErrOutOfRange, /**< Значение вне допустимого диапазона */
    TeaPresetResultErrDuplicateField, /**< Поле встречается дважды */
    TeaPresetResultErrEmptyName, /**< Пустое название */
    TeaPresetResultErrBadName, /**< Недопустимые символы в названии */
} TeaPresetResult;

/** Описание ошибки чтения пресета (для диалога и логов). */
typedef struct {
    TeaPresetResult result; /**< Код результата */
    char error_field[24]; /**< Имя проблемного ключа или пустая строка */
    char file_name[32]; /**< Имя файла (basename) для текста диалога */
} PresetError;

/** Пресет чая, загруженный из файла `.tea`. */
typedef struct {
    char name[TEA_NAME_MAX + 1]; /**< 1–24 байта: латиница, цифры, пробел, `_` */
    uint16_t leaf_tenths_g; /**< Граммовка в десятых долях: 1–500 (0.1–50.0 г) */
    uint8_t water_temp_min_c; /**< Температура воды, минимум: 85–90 (≤ `water_temp_max_c`) */
    uint8_t water_temp_max_c; /**< Температура воды, максимум: 85–90 (≥ `water_temp_min_c`) */
    uint16_t first_pour_sec; /**< Длительность первого пролива: 1–3600 с */
    uint16_t increment_sec; /**< Приращение на каждый следующий пролив: 0–600 с */
    uint8_t max_pours; /**< Рекомендованное число проливов: 1–99 */
    char path[TEA_PATH_MAX]; /**< Путь к файлу пресета на SD-карте */
} TeaPreset;

/** Результат записи пресета в файл. */
typedef enum {
    PresetSaveOK, /**< Файл записан и проверен обратным чтением */
    PresetSaveErrDraft, /**< Черновик не прошёл проверку */
    PresetSaveErrNoDir, /**< Каталог приложения не создан */
    PresetSaveErrOpen, /**< Временный файл не открылся на запись */
    PresetSaveErrWrite, /**< Записано меньше байт, чем нужно */
    PresetSaveErrVerify, /**< Обратное чтение не совпало с черновиком */
    PresetSaveErrReplace, /**< Не удалось подменить целевой файл */
    PresetSaveErrNoName, /**< Нет свободного имени в диапазоне `_2`…`_99` */
} PresetSaveResult;

/** Результат удаления пресета. */
typedef enum {
    PresetDeleteOK, /**< Файл удалён */
    PresetDeleteErrNotFound, /**< Файла уже нет на карте */
    PresetDeleteErrRemove, /**< `storage_common_remove` вернул ошибку */
    PresetDeleteErrNoCard, /**< SD-карта недоступна */
} PresetDeleteResult;

/** Точка входа приложения. */
int32_t tea_timer_app(void* p);

#ifdef __cplusplus
}
#endif
