# Data Model — управление пресетами (002)

Сущности выведены из `spec.md` (FR-001…FR-023) и дополняют модель
`001-tea-brew-timer/data-model.md`, не изменяя её: `TeaPreset`, `BrewSession`,
`BrewViewModel`, `PresetError` остаются как есть. Формат файла —
[`001/contracts/tea-file-format.md`](../001-tea-brew-timer/contracts/tea-file-format.md).

---

## 1. TeaPreset — пресет (без изменений)

| Поле | Тип | Диапазон / правило |
|---|---|---|
| `name` | `char[25]` | 1–24 байта, `[A-Za-z0-9_ ]`, не пусто |
| `leaf_tenths_g` | `uint16_t` | 1–500 (0.1–50.0 г) |
| `first_pour_sec` | `uint16_t` | 1–3600 |
| `increment_sec` | `uint16_t` | 0–600 |
| `max_pours` | `uint8_t` | 1–99 |
| `path` | `char[128]` | путь к файлу на SD; при создании пустой, при правке — исходный |

Поля формата не расширяются. Расширение (например, температура воды в
`003-water-temperature`) — отдельное изменение формата, в 002 не выполняется.

---

## 2. Редактор пресета (Editor) — состояние формы

Живёт в `TeaTimerApp` весь сценарий создания/правки, не сохраняется на SD.

| Поле | Тип | Правило |
|---|---|---|
| `draft` | `TeaPreset` | значения формы; при создании — умолчания, при правке — копия выбранного пресета вместе с `path` |
| `editing` | `bool` | `false` — создание (подбирается новое имя файла); `true` — правка (пишем в `draft.path`) |
| `step` | `uint8_t` (`EditStep`) | текущий шаг формы: `Name`, `Leaf`, `First`, `Increment`, `MaxPours`, `Summary` |

**Инварианты**:

- `draft.name` — всегда `\0`-терминированная строка ≤ 24 байт;
- при `editing = true` `draft.path` непустой и совпадает с путём файла, открытого в сессии;
- значения по умолчанию для создания: `5.0 / 20 / 5 / 6` (FR-005);
- черновик не переживает выход из формы: при отмене и при выходе из приложения сбрасывается
  (FR-010, Assumptions).

**Деривативные (не хранятся)**: `leaf_text` (`%u.%u`), `slug` (имя файла без пути),
`target_path` (путь записи).

---

## 3. Конечный автомат формы

```text
                 ┌──────────── (создание) ────────────┐
Menu ─New preset─► Name ─► Leaf ─► First ─► Step ─► Max ─► Summary ─Save─► запись ─► Saved
                    ▲       ▲       ▲        ▲       ▲        │
                    └───────┴───────┴────────┴───────┘        │ BACK
                          BACK = предыдущий шаг               ▼
                                                        предыдущий шаг (Max)

Card ─>─► Actions ─Edit─► Name (с текущими значениями) ─► … ─► Summary ─Save─► запись в тот же файл ─► Saved
                       └─Delete─► DeleteConfirm ─No/BACK─► Card
                                            └─Yes─► удаление ─► Deleted ─► FileSelect
```

| Переход | Условие | Побочные эффекты |
|---|---|---|
| `Menu → Name` | пункт `New preset` | `editing=false`, `draft` = умолчания, `step=Name` |
| `Name → Leaf` | название прошло валидатор и непустое | `draft.name` заполнен |
| `Leaf → First → Step → Max` | OK на шаге | `draft` обновляется; BACK — предыдущий шаг |
| `Max → Summary` | OK | `step=Summary` |
| `Summary → запись` | кнопка `Save` | `tea_preset_check_draft()` → `tea_preset_save()` |
| `Summary → Max` | BACK | возврат к последнему полю |
| `Card → Actions` | кнопка `>` | меню `Edit`/`Delete` |
| `Actions → Name` | `Edit` | `editing=true`, `draft` = копия `session.preset` |
| `Actions → DeleteConfirm` | `Delete` | показывает название чая и имя файла |
| `DeleteConfirm → Card` | `No` или BACK | без изменений |
| `DeleteConfirm → удаление` | `Yes` | `tea_preset_delete()`, при совпадении с сессией — `brew_session_clear()` |
| `Saved/Deleted → FileSelect` | `OK` | список перечитывается при входе |

Отмена (BACK из формы создания на шаге `Name`) возвращает в меню и не создаёт файл; из
формы правки — в меню действий.

---

## 4. Результаты операций

### 4.1 PresetSaveResult

| Значение | Когда | Сообщение UI |
|---|---|---|
| `PresetSaveOK` | файл записан и проверен обратным чтением | `Saved` + имя файла |
| `PresetSaveErrDraft` | `tea_preset_check_draft()` не прошёл | `Out of range` / `Bad name` (по `TeaPresetResult`) |
| `PresetSaveErrNoDir` | каталог не создан | `Cannot save file` |
| `PresetSaveErrOpen` | `.tmp` не открылся на запись | `Cannot save file` |
| `PresetSaveErrWrite` | записано меньше байт | `Cannot save file` |
| `PresetSaveErrVerify` | обратное чтение не совпало | `Cannot save file`, `.tmp` удалён |
| `PresetSaveErrReplace` | `remove`/`rename` целевого не удались | `Cannot save file`, `.tmp` остаётся |
| `PresetSaveErrNoName` | не найдено свободное имя (`_2`…`_99`) | `Too many copies` |

### 4.2 PresetDeleteResult

| Значение | Когда | Сообщение UI |
|---|---|---|
| `PresetDeleteOK` | файла больше нет (удалён этим вызовом) | `Deleted` + имя файла |
| `PresetDeleteErrNotFound` | файла уже нет на карте | `File not found` |
| `PresetDeleteErrRemove` | `storage_common_remove` вернул ошибку (карта, права) | `Cannot delete file` |
| `PresetDeleteErrNoCard` | `storage_sd_status != FSE_OK` перед удалением | `SD card not found` |

Инвариант: при любом результате, кроме `PresetDeleteOK`, файл остаётся доступным (если он
существовал до вызова).

---

## 5. Имена файлов и пути

| Правило | Значение |
|---|---|
| Основа имени (`slug`) | `name` → нижний регистр, пробелы → `_`, остаются `[a-z0-9_]`, пусто → `preset` |
| Расширение | `.tea` |
| Каталог | `STORAGE_APP_DATA_PATH_PREFIX` (`/ext/apps_data/tea_timer`) |
| Свободное имя | первый индекс `0, 2, 3, … 99`: `slug.tea`, `slug_2.tea`, … |
| Правка | путь не меняется, даже если `name` изменён (FR-012) |
| Временный файл | `<target>.tmp` рядом с целевым, удаляется после успешной подмены |

---

## 6. Валидация (сводная таблица)

| Правило | Проверяется в | Тест (набор на устройстве) |
|---|---|---|
| `name` 1–24, `[A-Za-z0-9_ ]` | `tea_timer_edit_name_validator` + `tea_preset_check_draft` | `preset_write` |
| `leaf_tenths_g` 1–500 | `number_input(1…500)` + `check_draft` | `preset_write` |
| `first_pour_sec` 1–3600 | `number_input(1…3600)` + `check_draft` | `preset_write` |
| `increment_sec` 0–600 | `number_input(0…600)` + `check_draft` | `preset_write` |
| `max_pours` 1–99 | `number_input(1…99)` + `check_draft` | `preset_write` |
| Сериализация совместима с парсером 001 | `tea_preset_serialize` → `tea_preset_parse_buf` | `preset_write` |
| Уникальность имени (`_2`, `_3`) | `tea_preset_make_path` + `storage_file_exists` | `preset_write` |
| Правка в тот же путь | `tea_preset_save(editing=true)` | `preset_write` |
| Сброс сессии удалённого пресета | `brew_session_clear` | `preset_write` |

---

## 7. Связи сущностей

```text
TeaPreset (файл .tea) ──копия──► Editor.draft ──serialize──► .tea.tmp ──rename──► .tea
        │                              │
        │ source_path                  │ editing?
        ▼                              ▼
   BrewSession ──── clear при удалении ───┘
        │
        ▼
   BrewViewModel / карточка (001)
```

- Один черновик на приложение: открытие формы перезаписывает `draft`;
- `BrewSession` не изменяется при создании нового пресета;
- при удалении активного пресета сессия очищается полностью (`has_preset=false`).
