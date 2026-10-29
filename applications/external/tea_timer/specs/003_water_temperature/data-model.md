# Data Model — температура воды в пресете (003)

Сущности выведены из `spec.md` (FR-001…FR-027) и дополняют модель
`001-tea-brew-timer/data-model.md` и `002-preset-management/data-model.md`, не изменяя
существующих правил. Формат файла — [contracts/tea-file-format.md](./contracts/tea-file-format.md).

---

## 1. TeaPreset — пресет

| Поле | Тип | Диапазон / правило | Источник |
|---|---|---|---|
| `name` | `char[25]` | 1–24 байта, `[A-Za-z0-9_ ]` | 001 |
| `leaf_tenths_g` | `uint16_t` | 1–500 (0.1–50.0 г) | 001 |
| **`water_temp_min_c`** | **`uint8_t`** | **85–90; ≤ `water_temp_max_c`** | **003, FR-001…FR-003** |
| **`water_temp_max_c`** | **`uint8_t`** | **85–90; ≥ `water_temp_min_c`** | **003, FR-001…FR-003** |
| `first_pour_sec` | `uint16_t` | 1–3600 | 001 |
| `increment_sec` | `uint16_t` | 0–600 | 001 |
| `max_pours` | `uint8_t` | 1–99 | 001 |
| `path` | `char[128]` | путь на SD; при создании пуст, при правке — исходный | 001/002 |

Порядок полей в структуре рекомендуется совпадающим с порядком ключей в файле
(`name`, `leaf`, `water_min`, `water_max`, `first`, `increment`, `max_pours`, `path`).

**Инварианты**:

- после успешного разбора и в любом корректном черновике оба поля заполнены и лежат
  в 85–90;
- `water_temp_min_c <= water_temp_max_c`; равенство допустимо (`87–87`);
- «нет температуры» в модели не существует: отсутствие обоих ключей в файле даёт
  умолчание `85`/`90`, неполная пара — ошибку разбора (в модель не попадает);
- структура копируется целиком (`BrewSession.preset = TeaPreset`), поэтому новые поля не
  требуют отдельных путей передачи.

**Константы**:

| Имя | Значение | Назначение |
|---|---|---|
| `TEA_WATER_TEMP_MIN` | 85 | нижняя граница ввода и файла |
| `TEA_WATER_TEMP_MAX` | 90 | верхняя граница ввода и файла |
| `TEA_WATER_TEMP_DEFAULT_MIN` | 85 | умолчание создания и старого файла |
| `TEA_WATER_TEMP_DEFAULT_MAX` | 90 | умолчание создания и старого файла |

---

## 2. Редактор пресета (Editor) — состояние формы

Живёт в `TeaTimerApp`, не сохраняется на SD. Изменения относительно 002:

| Поле | Тип | Правило |
|---|---|---|
| `draft` | `TeaPreset` | при создании — умолчания `5.0 / 85 / 90 / 20 / 5 / 6`; при правке — копия выбранного пресета вместе с `path` |
| `editing` | `bool` | `false` — создание, `true` — правка (путь не меняется) |
| `edit_step` | `uint8_t` (`TeaEditStep`) | шаги: `Name`, `Leaf`, **`WaterMin`**, **`WaterMax`**, `First`, `Increment`, `MaxPours`, `Summary` |

**Инварианты**:

- `draft` всегда содержит оба значения температуры в 85–90;
- при `editing = true` `draft.path` непустой и не меняется при сохранении;
- при отмене (BACK) файл не трогается, черновик отбрасывается;
- ошибка на сводке не очищает `draft`: пользователь возвращается на проблемный шаг с
  введёнными значениями.

**Деривативные (не хранятся)**: `leaf_text` (`%u.%u`), `water_text` (`%u-%u C`),
`slug`, `target_path`.

---

## 3. Конечный автомат формы

```text
Menu ─New preset─► Name ─► Leaf ─► WaterMin ─► WaterMax ─► First ─► Increment ─► MaxPours ─► Summary ─Save─► запись ─► Saved
                    ▲       ▲         ▲            ▲          ▲          ▲            ▲          │
                    └───────┴─────────┴────────────┴──────────┴──────────┴────────────┘          │ BACK
                                    BACK = предыдущий шаг                                       ▼
                                                                                        MaxPours

Card ─>─► Actions ─Edit─► Name (текущие значения, включая температуру) ─► … ─► Summary ─Save─► тот же файл ─► Saved
                       └─Delete─► DeleteConfirm ─No/BACK─► Card
                                            └─Yes─► удаление ─► Deleted ─► FileSelect
```

| Переход | Условие | Побочные эффекты |
|---|---|---|
| `Menu → Name` | `New preset` | `editing=false`; `draft` = `5.0`, `85`, `90`, `20`, `5`, `6`; `step=Name` |
| `Name → Leaf` | название непустое и прошло валидатор | `draft.name` заполнен |
| `Leaf → WaterMin` | OK | `draft.leaf_tenths_g`; `number_input` 85…90, значение `draft.water_temp_min_c` |
| `WaterMin → WaterMax` | OK | `draft.water_temp_min_c`; `number_input` `draft.water_temp_min_c`…90 |
| `WaterMax → First` | OK | `draft.water_temp_max_c` |
| `First → Increment → MaxPours` | OK | как в 002 |
| `MaxPours → Summary` | OK | `step=Summary` |
| `Summary → запись` | `Save` | `check_draft` → `tea_preset_save()` |
| `Summary → шаг ошибки` | `check_draft` != OK | по `error_field`: `water_temp_min_c`/`water_temp_max_c` → соответствующий шаг |
| `Summary → MaxPours` | BACK | возврат к последнему полю |
| BACK из `WaterMin` | — | на `Leaf`; из `WaterMax` — на `WaterMin`; из `First` — на `WaterMax` |
| `Card → Actions` | кнопка `>` | как в 002 |
| `Actions → Name` | `Edit` | `draft` = копия `session.preset` (с температурой), `editing=true` |
| `Saved → FileSelect` | `OK` | список перечитывается при входе |

Отмена (BACK из `Name` при создании) возвращает в меню и файл не создаёт; из формы правки —
в меню действий. Частично введённые значения при этом не записываются (FR-021).

---

## 4. Разбор и результаты

### 4.1 Используемые результаты `TeaPresetResult` (новых кодов нет)

| Ситуация | Результат | `error_field` |
|---|---|---|
| Оба ключа отсутствуют | `OK` | — (поля = 85/90) |
| Оба ключа корректны, `min ≤ max` | `OK` | — |
| Отсутствует `water_temp_min_c` | `ErrNoField` | `water_temp_min_c` |
| Отсутствует `water_temp_max_c` | `ErrNoField` | `water_temp_max_c` |
| Значение вне 85–90 (`84`, `91`) | `ErrOutOfRange` | имя ключа |
| `min > max` (`88`/`86`) | `ErrOutOfRange` | `water_temp_max_c` |
| Дубликат ключа | `ErrDuplicateField` | имя ключа |
| `85.5`, `85abc`, пусто | `ErrBadNumber` | имя ключа |
| ` 085 ` | `OK` | — (значение 85) |

### 4.2 Запись (`PresetSaveResult`, без изменений)

| Значение | Когда |
|---|---|
| `PresetSaveOK` | 7 ключей записаны, обратное чтение совпало, включая температуру |
| `PresetSaveErrDraft` | `check_draft` отклонил температуру (84/91, `min > max`) |
| `PresetSaveErrVerify` | обратное чтение не совпало по любому полю, включая температуру |
| прочие | как в 002 |

---

## 5. Валидация (сводная таблица)

| Правило | Парсер `tea_preset_parse_buf` | `tea_preset_check_draft` | Ввод `number_input` | Тест |
|---|---|---|---|---|
| Оба значения целые | да (`tea_parse_uint`) | тип `uint8_t` | только целые | `tea_preset` |
| 85 ≤ min ≤ 90 | да | да | `min=85`, `max=90` | оба набора |
| 85 ≤ max ≤ 90 | да | да | `max=90` | оба набора |
| min ≤ max | да | да | `min=draft.water_temp_min_c` | оба набора |
| Равенство `87–87` допустимо | да | да | допустимо | `tea_preset` |
| Дубликат ключа | да | — | — | `tea_preset` |
| Неполная пара | да | — | — | `tea_preset` |
| Отсутствие обоих ключей (`85/90`) | да | — | — | `tea_preset` |
| Запись обоих ключей | — | — | — | `preset_write` |
| Совместимость serialize → parse | — | — | — | `preset_write` |
| Правка старого файла добавляет ключи | — | — | — | `preset_write` |
| Температура не выводится на таймере | — | — | — | `quickstart.md` |

---

## 6. Экранные модели

### 6.1 Карточка пресета (`widget`)

Источник данных — `app->session.preset` (новые поля приходят автоматически при копировании).

| Строка | Значение | Условие |
|---|---|---|
| 1 | `name` | всегда |
| 2 | `Leaf %u.%u g/100ml` | всегда |
| 3 | `Water %u-%u C` | всегда (FR-010) |
| 4 | `Pours %u / %u` | всегда |
| 5 | `Pour %u  %s` | всегда |

Кнопки: `Start` (центр), `>` (вправо) при `has_preset`.

### 6.2 Сводка формы (`widget`)

| Строка | Значение |
|---|---|
| 1 | `New preset` / `Edit preset` |
| 2 | `name` |
| 3 | `Leaf %u.%u  Water %u-%u C` |
| 4 | `First %s  Step %s` |
| 5 | `Max %u pours` |

Кнопка: `Save`.

### 6.3 Экран заваривания (`BrewViewModel`)

**Без изменений.** Полей температуры в модели нет; ни одна строка не содержит `Water`,
`C` или чисел 85–90. Состав: название, граммовка, крупный отсчёт, счётчик проливов,
действие (FR-012, FR-027).

---

## 7. Связи сущностей

```text
.tea (7 ключей) ──parse──► TeaPreset (+2 поля) ──копия──► BrewSession.preset ──► карточка
                                 │                          │
                                 │                          └─► BrewViewModel (без температуры)
                                 ├──копия──► Editor.draft ──serialize──► .tea.tmp ──rename──► .tea
                                 │
                                 └── TeaPreset.path ── правка: путь не меняется
```

- Один черновик на приложение: открытие формы перезаписывает `draft`.
- `BrewSession` не изменяется при создании нового пресета; температура живёт в его
  `preset`, но не выводится на экран заваривания.
- При удалении активного пресета сессия очищается полностью (`brew_session_clear`),
  включая температуру.
