# Контракт записи, правки и удаления пресета (002)

Дополняет [`001/contracts/tea-file-format.md`](../../001-tea-brew-timer/contracts/tea-file-format.md)
и [`001/contracts/storage-layout.md`](../../001-tea-brew-timer/contracts/storage-layout.md).
Содержимое файла и набор ключей **не меняются** — 002 добавляет только операции записи и
удаления. Любое расхождение с 001-контрактом считается ошибкой 002.

---

## 1. Публичные функции

```c
/* Чистый слой — без Furi и Storage (тестируется на устройстве). */
size_t tea_preset_build_slug(const char* name, char* out, size_t size);
size_t tea_preset_make_path(const char* base, const char* slug, uint8_t index, char* out, size_t size);
TeaPresetResult tea_preset_serialize(const TeaPreset* preset, char* buf, size_t size, size_t* out_len);
TeaPresetResult tea_preset_check_draft(const TeaPreset* draft);

/* Слой Storage. */
PresetSaveResult tea_preset_save(
    Storage* storage, TeaPreset* draft, bool editing, char* out_path, size_t out_path_size);
PresetDeleteResult tea_preset_delete(Storage* storage, const char* path);
void brew_session_clear(BrewSession* session);
```

- `tea_preset_save()` записывает `draft->path` при `editing = true` и подбирает свободное имя
  при `editing = false`; выбранный путь возвращается в `out_path` и копируется в `draft->path`.
- Буфер сериализации выделяется внутри `tea_preset_save()` (`TEA_FILE_MAX_SIZE + 1`) и
  освобождается на всех путях.

---

## 2. Имя файла

| Шаг | Правило | Пример |
|---|---|---|
| 1 | Нижний регистр | `Green Tea` → `green tea` |
| 2 | Пробелы и недопустимые символы → отброшены (пробелы → `_`) | `green tea` → `green_tea`, `oolong-2` → `oolong2` |
| 3 | Пустой результат | `preset` |
| 4 | Длина ≤ `TEA_NAME_MAX` (24) | — |
| 5 | Расширение | `.tea` |
| 6 | Занято → суффикс `_2`, `_3`, … до `_99` | `green_tea_2.tea` |
| 7 | Свободных имён до `_99` нет | `PresetSaveErrNoName` |

`tea_preset_build_slug(name, out, size)` возвращает длину результата без `\0`.
`tea_preset_make_path(base, slug, 0, …)` → `base/slug.tea`; `index > 0` → `base/slug_<index>.tea`.

Проверка занятости — только `storage_file_exists()`; `storage_common_stat()` используется
только для диагностики (не влияет на выбор имени).

---

## 3. Сериализация

| Правило | Значение |
|---|---|
| Порядок ключей | `name`, `leaf_g_per_100ml`, `first_pour_sec`, `increment_sec`, `max_pours` |
| Формат `leaf` | `%u.%u` — ровно одна цифра после точки |
| Перевод строки | `\n` после каждой строки, включая последнюю |
| Кодировка | ASCII, без BOM |
| Максимальная длина | ≤ `TEA_FILE_MAX_SIZE` (1024) байта |
| Предпроверка | `tea_preset_check_draft()` до записи |

`tea_preset_check_draft()` возвращает `TeaPresetResultOK` либо первый по порядку код:
`ErrEmptyName`, `ErrBadName`, `ErrOutOfRange` (с именем поля в `error_field`).

**Инвариант обратимости**: `tea_preset_serialize(p) → tea_preset_parse_buf()` даёт те же
пять значений, что и `p`, и `TeaPresetResultOK`.

---

## 4. Алгоритм сохранения

```text
tea_preset_save(storage, draft, editing, out_path, size)
├─ если !storage_dir_exists(base): storage_common_mkdir(base)
│    └─ ошибка, кроме FSE_EXIST → ErrNoDir
├─ tea_preset_check_draft(draft) ≠ OK → ErrDraft
├─ если editing: target = draft->path
│  иначе: цикл index = 0..99, найти свободный target (storage_file_exists)
│    └─ свободного нет → ErrNoName
├─ serialize(draft) в буфер в куче; ошибка → ErrDraft
├─ tmp = "<target>.tmp"
├─ storage_file_open(tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS)
│    └─ ошибка → ErrOpen (буфер освобождён)
├─ storage_file_write == len? иначе → закрыть, remove(tmp), ErrWrite
├─ storage_file_close(tmp) на всех путях
├─ tea_preset_load(storage, tmp) == OK и значения совпадают?
│    └─ нет → remove(tmp), ErrVerify
├─ если editing и storage_file_exists(target): storage_common_remove(target)
├─ storage_common_rename(tmp, target)
│    └─ ошибка → ErrReplace (tmp оставлен)
├─ скопировать target в draft->path и в out_path
└─ OK
```

**Гарантии**:

- при `Err*` до шага подмены целевой файл не изменён (для правки) и новый не создан;
- после `OK` файл по `target` существует и читается парсером 001;
- `.tmp` не остаётся после `OK`, `ErrWrite`, `ErrVerify`; после `ErrReplace` остаётся
  осознанно (шанс ручного восстановления) и упоминается в логе `FURI_LOG_E`.

---

## 5. Алгоритм удаления

```text
tea_preset_delete(storage, path)
├─ storage_sd_status(storage) ≠ FSE_OK → ErrNoCard
├─ !storage_file_exists(path) → ErrNotFound
├─ storage_common_remove(storage, path)
│    └─ FS_Error ≠ FSE_OK → ErrRemove
├─ storage_file_exists(path) → ErrRemove (файл не исчез)
└─ OK
```

Вызывающий код (`tea_timer_scene_delete_confirm.c`) при `OK`:

1. если `session.has_preset` и `strcmp(session.preset.path, path) == 0` →
   `brew_session_clear(&app->session)`;
2. `FURI_LOG_I("deleted %s", path)`;
3. переход на `Saved` с типом `Deleted`.

---

## 6. Правила безопасности

| Правило | Причина |
|---|---|
| Удаляется только путь, полученный из карточки выбранного `.tea`-файла | FR-013/FR-015: каталоги и прочие файлы не затрагиваются |
| Путь не собирается из пользовательского ввода при удалении | защита от выхода за пределы `APP_DATA_PATH` |
| `tea_preset_save()` при `editing = false` не перезаписывает существующий файл | ответ на clarify Q1 (автосуффикс) |
| `tea_preset_save()` при `editing = true` пишет только в `draft->path` | FR-012: имя файла не меняется |
| Все открытия файлов закрываются на всех путях | принцип V |
| `.tmp` пишется в тот же каталог | `rename` в пределах одной ФС |

---

## 7. Тестовые векторы (набор `preset_write`, на устройстве)

| Вход | Ожидание |
|---|---|
| `build_slug("Green Tea")` | `green_tea` |
| `build_slug("  A  B  ")` | `a__b` (крайние пробелы обрезаны) |
| `build_slug("")` / `build_slug("!!!")` | `preset` |
| `build_slug(24 символа)` | длина 24 |
| `build_slug("Oolong-2")` | `oolong2` |
| `make_path(base,"green_tea",0)` / `(...,2)` | `base/green_tea.tea` / `base/green_tea_2.tea` |
| `serialize` эталонного пресета | 5 строк, `leaf 5.0`, завершающий `\n` |
| `serialize` → `parse_buf` | значения совпадают, `OK` |
| `check_draft` с именем `` / 25 символов / `Green-Tea` | `ErrEmptyName` / `ErrBadName` / `ErrBadName` |
| `check_draft` границы 1/500, 1/3600, 0/600, 1/99 | `OK` |
| `check_draft` 0/501, 0/3601, 601, 0/100 | `ErrOutOfRange` + поле |
| `save` в свободное имя | файл создан, прочитан, значения совпадают |
| `save` при существующем `green_tea.tea` | создан `green_tea_2.tea`, первый не изменён |
| `save` с `editing=true` | путь прежний, имя файла не изменилось, содержимое новое |
| `delete` существующего | `PresetDeleteOK`, `storage_file_exists == false` |
| `delete` отсутствующего | `PresetDeleteErrNotFound` |
| `brew_session_clear` | `has_preset=false`, счётчик 0, фаза `IDLE`, `path[0]=='\0'` |

Тесты используют временный каталог `APP_DATA_PATH("test")` и удаляют его содержимое после
прогона; реальные пресеты пользователя не читаются и не изменяются.
